export module gse.gpu.shader:shader_compiler;

import std;

import gse.assert;
import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.assets;
import gse.slang;
import :shader;

namespace gse::shader_compile {
    auto log_diagnostics(
        slang::IBlob* diagnostics
    ) -> void;

    auto to_vertex_format(
        slang::TypeReflection* type
    ) -> gpu::vertex_format;

    auto to_descriptor_type(
        slang::TypeLayoutReflection* tl,
        int range_index
    ) -> gpu::descriptor_type;

    auto reflect_members(
        slang::TypeLayoutReflection* struct_layout,
        std::optional<slang::ParameterCategory> preferred_cat = std::nullopt
    ) -> std::unordered_map<std::string, shader::uniform_member>;

    auto extract_struct_layout(
        slang::VariableLayoutReflection* var
    ) -> slang::TypeLayoutReflection*;

    auto to_stage_flags(
        SlangStage slang_stage
    ) -> gpu::stage_flags;

    auto reflect_descriptor_sets(
        slang::ProgramLayout* program_layout,
        const std::function<gpu::descriptor_type(slang::TypeLayoutReflection*, int)>& to_desc_type,
        const std::function<std::unordered_map<std::string, shader::uniform_member>(slang::TypeLayoutReflection*, std::optional<slang::ParameterCategory>)>& reflect_mem,
        const std::function<slang::TypeLayoutReflection*(slang::VariableLayoutReflection*)>& extract_struct
    ) -> shader::layout;
}

template <>
struct gse::asset_compiler<gse::shader> {
    static auto source_extensions(
    ) -> std::vector<std::string>;

    static auto baked_extension(
    ) -> std::string;

    static auto source_directory(
    ) -> std::string;

    static auto baked_directory(
    ) -> std::string;

    static auto compile_one(
        const std::filesystem::path& source,
        const std::filesystem::path& destination
    ) -> bool;

    static auto needs_recompile(
        const std::filesystem::path& source,
        const std::filesystem::path& destination
    ) -> bool;

    static auto dependencies(
        const std::filesystem::path& source
    ) -> std::vector<std::filesystem::path>;
};

auto gse::shader_compile::log_diagnostics(slang::IBlob* diagnostics) -> void {
    if (!diagnostics || diagnostics->getBufferSize() == 0) {
        return;
    }

    const std::string message(
        static_cast<const char*>(diagnostics->getBufferPointer()),
        diagnostics->getBufferSize()
    );
    log::println(log::level::error, log::category::assets, "{}", message);
}

auto gse::shader_compile::to_vertex_format(slang::TypeReflection* type) -> gpu::vertex_format {
    using kind = slang::TypeReflection::Kind;

    auto elem_and_count = [](slang::TypeReflection* t) -> std::pair<slang::TypeReflection*, int> {
        switch (t->getKind()) {
            case kind::Vector: return { t->getElementType(), static_cast<int>(t->getElementCount()) };
            case kind::Scalar: return { t, 1 };
            default: return { t, 0 };
        }
    };

    auto [elem, count] = elem_and_count(type);
    if (!elem || count <= 0) {
        return gpu::vertex_format::r32_sfloat;
    }

    switch (elem->getScalarType()) {
        case slang::TypeReflection::Float32:
            switch (count) {
                case 1: return gpu::vertex_format::r32_sfloat;
                case 2: return gpu::vertex_format::r32g32_sfloat;
                case 3: return gpu::vertex_format::r32g32b32_sfloat;
                case 4: return gpu::vertex_format::r32g32b32a32_sfloat;
                default: return gpu::vertex_format::r32_sfloat;
            }
        case slang::TypeReflection::Int32:
            switch (count) {
                case 1: return gpu::vertex_format::r32_sint;
                case 2: return gpu::vertex_format::r32g32_sint;
                case 3: return gpu::vertex_format::r32g32b32_sint;
                case 4: return gpu::vertex_format::r32g32b32a32_sint;
                default: return gpu::vertex_format::r32_sint;
            }
        case slang::TypeReflection::UInt32:
            switch (count) {
                case 1: return gpu::vertex_format::r32_uint;
                case 2: return gpu::vertex_format::r32g32_uint;
                case 3: return gpu::vertex_format::r32g32b32_uint;
                case 4: return gpu::vertex_format::r32g32b32a32_uint;
                default: return gpu::vertex_format::r32_uint;
            }
        default: break;
    }
    return gpu::vertex_format::r32_sfloat;
}

auto gse::shader_compile::to_descriptor_type(slang::TypeLayoutReflection* tl, const int range_index) -> gpu::descriptor_type {
    if (tl == nullptr) {
        return gpu::descriptor_type::storage_buffer;
    }
    if (const int range_count = tl->getBindingRangeCount(); range_index < 0 || range_index >= range_count) {
        return gpu::descriptor_type::storage_buffer;
    }

    const slang::BindingType bt = tl->getBindingRangeType(range_index);
    const SlangBindingTypeIntegral bt_bits = static_cast<SlangBindingTypeIntegral>(bt);
    const auto base_bt = static_cast<SlangBindingType>(bt_bits & slang_binding_type_base_mask);
    const bool is_mutable = (bt_bits & slang_binding_type_mutable_flag) != 0;

    slang::TypeLayoutReflection* leaf_layout = tl->getBindingRangeLeafTypeLayout(range_index);
    slang::TypeReflection* leaf_type = leaf_layout ? leaf_layout->getType() : nullptr;
    const auto access = leaf_type ? leaf_type->getResourceAccess() : slang_resource_access_read;

    switch (base_bt) {
        case slang_binding_type_combined_texture_sampler: return gpu::descriptor_type::combined_image_sampler;
        case slang_binding_type_texture:
            return (is_mutable || access != slang_resource_access_read) ? gpu::descriptor_type::storage_image : gpu::descriptor_type::sampled_image;
        case slang_binding_type_sampler: return gpu::descriptor_type::sampler;
        case slang_binding_type_raw_buffer: return gpu::descriptor_type::storage_buffer;
        case slang_binding_type_typed_buffer: return gpu::descriptor_type::storage_buffer;
        case slang_binding_type_constant_buffer:
        case slang_binding_type_parameter_block: return gpu::descriptor_type::uniform_buffer;
        case slang_binding_type_input_render_target: return gpu::descriptor_type::sampled_image;
        case slang_binding_type_ray_tracing_acceleration_structure: return gpu::descriptor_type::acceleration_structure;
        default: return gpu::descriptor_type::storage_buffer;
    }
}

auto gse::shader_compile::reflect_members(slang::TypeLayoutReflection* struct_layout, const std::optional<slang::ParameterCategory> preferred_cat) -> std::unordered_map<std::string, shader::uniform_member> {
    using kind = slang::TypeReflection::Kind;
    std::unordered_map<std::string, shader::uniform_member> members;

    if (!struct_layout || struct_layout->getKind() != kind::Struct) {
        return members;
    }

    const slang::ParameterCategory order[] = {
        preferred_cat.value_or(slang::ParameterCategory::Uniform),
        slang::ParameterCategory::ShaderResource,
        slang::ParameterCategory::UnorderedAccess,
        slang::ParameterCategory::Uniform
    };

    slang::ParameterCategory chosen = order[0];
    bool ok = false;
    for (const auto c : order) {
        if (struct_layout->getSize(c) > 0) {
            chosen = c;
            ok = true;
            break;
        }
    }
    if (!ok) {
        chosen = slang::ParameterCategory::Uniform;
    }

    for (int i = 0; i < struct_layout->getFieldCount(); ++i) {
        auto* m_var = struct_layout->getFieldByIndex(i);
        auto* m_type = m_var ? m_var->getTypeLayout() : nullptr;
        if (!m_type) {
            continue;
        }

        const auto k = m_type->getKind();
        if (k == kind::Resource || k == kind::SamplerState) {
            continue;
        }

        std::uint32_t sz = static_cast<std::uint32_t>(m_type->getSize(chosen));
        if (sz == 0) {
            sz = static_cast<std::uint32_t>(m_type->getSize(slang::ParameterCategory::Uniform));
        }
        if (sz == 0) {
            continue;
        }

        std::uint32_t off = static_cast<std::uint32_t>(m_var->getOffset(chosen));
        if (off == 0 && sz != 0) {
            off = static_cast<std::uint32_t>(m_var->getOffset(slang::ParameterCategory::Uniform));
        }

        members[m_var->getName()] = {
            .name = m_var->getName(),
            .type_name = m_type->getType() ? m_type->getType()->getName() : std::string{},
            .offset = off,
            .size = sz,
            .array_size = (k == kind::Array) ? static_cast<std::uint32_t>(m_type->getElementCount()) : 0u,
        };
    }
    return members;
}

auto gse::shader_compile::extract_struct_layout(slang::VariableLayoutReflection* var) -> slang::TypeLayoutReflection* {
    if (!var) {
        return nullptr;
    }
    auto* type = var->getTypeLayout();
    if (!type) {
        return nullptr;
    }

    slang::TypeLayoutReflection* struct_layout = nullptr;
    switch (type->getKind()) {
        case slang::TypeReflection::Kind::ConstantBuffer:
        case slang::TypeReflection::Kind::ParameterBlock:
            if (auto* elem_var = type->getElementVarLayout()) {
                struct_layout = elem_var->getTypeLayout();
            }
            break;
        case slang::TypeReflection::Kind::Struct:
            struct_layout = type;
            break;
        default: break;
    }
    return struct_layout && struct_layout->getKind() == slang::TypeReflection::Kind::Struct ? struct_layout : nullptr;
}

auto gse::shader_compile::to_stage_flags(const SlangStage slang_stage) -> gpu::stage_flags {
    using enum gpu::stage_flag;
    switch (slang_stage) {
        case slang_stage_vertex: return vertex;
        case slang_stage_fragment: return fragment;
        case slang_stage_compute: return compute;
        case slang_stage_amplification: return task;
        case slang_stage_mesh: return mesh;
        default: return {};
    }
}

auto gse::shader_compile::reflect_descriptor_sets(
    slang::ProgramLayout* program_layout,
    const std::function<gpu::descriptor_type(slang::TypeLayoutReflection*, int)>& to_desc_type,
    const std::function<std::unordered_map<std::string, shader::uniform_member>(slang::TypeLayoutReflection*, std::optional<slang::ParameterCategory>)>& reflect_mem,
    const std::function<slang::TypeLayoutReflection*(slang::VariableLayoutReflection*)>& extract_struct
) -> shader::layout {
    shader::layout result;
    auto* globals_vl = program_layout->getGlobalParamsVarLayout();
    if (!globals_vl) {
        return result;
    }

    auto* globals_tl = globals_vl->getTypeLayout();
    if (!globals_tl) {
        return result;
    }

    slang::VariableLayoutReflection* container_vl = nullptr;
    slang::TypeLayoutReflection* element_tl = nullptr;

    switch (globals_tl->getKind()) {
        case slang::TypeReflection::Kind::Struct:
            container_vl = globals_vl;
            element_tl = globals_tl;
            break;
        case slang::TypeReflection::Kind::ConstantBuffer:
        case slang::TypeReflection::Kind::ParameterBlock:
            container_vl = globals_tl->getContainerVarLayout();
            if (auto* element_vl = globals_tl->getElementVarLayout()) {
                element_tl = element_vl->getTypeLayout();
            }
            break;
        default: break;
    }

    if (!element_tl || element_tl->getKind() != slang::TypeReflection::Kind::Struct) {
        return result;
    }

    const auto container_space = static_cast<std::uint32_t>(container_vl ? container_vl->getBindingSpace(slang::ParameterCategory::DescriptorTableSlot) : 0);
    const int field_count = element_tl->getFieldCount();

    for (int i = 0; i < field_count; ++i) {
        auto* var = element_tl->getFieldByIndex(i);
        if (var->getCategory() == slang::ParameterCategory::PushConstantBuffer) {
            continue;
        }
        auto* tl = var->getTypeLayout();
        const auto binding_range_count = tl->getBindingRangeCount();

        for (int range_index = 0; range_index < binding_range_count; ++range_index) {
            const auto binding_idx = static_cast<std::uint32_t>(var->getOffset(slang::ParameterCategory::DescriptorTableSlot));
            const auto set_idx = container_space + static_cast<std::uint32_t>(var->getBindingSpace(slang::ParameterCategory::DescriptorTableSlot));

            auto set_type = static_cast<gpu::descriptor_set_type>(set_idx);
            auto& [type, bindings] = result.sets[set_type];
            type = set_type;

            gpu::descriptor_binding_desc desc{
                .binding = binding_idx,
                .type = to_desc_type(tl, range_index),
                .count = tl->getKind() == slang::TypeReflection::Kind::Array ? static_cast<std::uint32_t>(tl->getElementCount()) : 1u,
            };

            using kind = slang::TypeReflection::Kind;

            auto try_structured_buffer_block = [&](
                slang::VariableLayoutReflection* variable,
                slang::TypeLayoutReflection* type_layout,
                const std::uint32_t binding_t,
                const std::uint32_t set_index
            ) -> std::optional<struct shader::uniform_block> {
                const bool is_structured =
                    type_layout->getKind() == kind::ShaderStorageBuffer ||
                    (type_layout->getKind() == kind::Resource &&
                        (type_layout->getType()->getResourceShape() & slang_resource_base_shape_mask) == SLANG_STRUCTURED_BUFFER);

                if (!is_structured) {
                    return std::nullopt;
                }

                slang::TypeLayoutReflection* elem_tl = nullptr;
                if (type_layout->getKind() == kind::ShaderStorageBuffer) {
                    if (auto* evl = type_layout->getElementVarLayout()) {
                        elem_tl = evl->getTypeLayout();
                    }
                } else {
                    elem_tl = type_layout->getElementTypeLayout();
                }

                if (!elem_tl || elem_tl->getKind() != kind::Struct) {
                    return std::nullopt;
                }

                const auto cat = (type_layout->getKind() == kind::ShaderStorageBuffer) ? slang::ParameterCategory::UnorderedAccess : slang::ParameterCategory::ShaderResource;

                std::uint32_t elem_stride = static_cast<std::uint32_t>(elem_tl->getStride(cat));
                if (elem_stride == 0) {
                    const std::uint32_t sz = static_cast<std::uint32_t>(elem_tl->getSize(slang::ParameterCategory::Uniform));
                    std::uint32_t al = static_cast<std::uint32_t>(elem_tl->getAlignment(slang::ParameterCategory::Uniform));
                    if (al == 0) {
                        al = 16;
                    }
                    if (sz) {
                        elem_stride = (sz + (al - 1)) & ~(al - 1);
                    }
                }

                auto members = reflect_mem(elem_tl, cat);

                if (elem_stride == 0 && !members.empty()) {
                    std::uint32_t max_end = 0;
                    for (const auto& m : members | std::views::values) {
                        max_end = std::max(max_end, m.offset + m.size);
                    }
                    constexpr std::uint32_t align = 16;
                    elem_stride = (max_end + (align - 1)) & ~(align - 1);
                }

                if (elem_stride == 0 && members.empty()) {
                    return std::nullopt;
                }

                return std::optional<struct shader::uniform_block>({
                    .name = variable->getName(),
                    .binding = binding_t,
                    .set = set_index,
                    .size = elem_stride,
                    .members = std::move(members),
                });
            };

            std::optional<struct shader::uniform_block> block_member;

            if (tl->getKind() == kind::ConstantBuffer || tl->getKind() == kind::ParameterBlock) {
                if (auto* struct_layout = extract_struct(var)) {
                    struct shader::uniform_block block = {
                        .name = var->getName(),
                        .binding = binding_idx,
                        .set = set_idx,
                        .size = static_cast<std::uint32_t>(struct_layout->getSize(slang::ParameterCategory::Uniform)),
                        .members = reflect_mem(struct_layout, slang::ParameterCategory::Uniform),
                    };
                    if (block.size > 0 && !block.members.empty()) {
                        block_member = block;
                    }
                }
            } else {
                block_member = try_structured_buffer_block(var, tl, binding_idx, set_idx);
            }

            const bool binding_exists = std::ranges::any_of(bindings, [&](const auto& b) {
                return b.desc.binding == binding_idx;
            });

            if (!binding_exists) {
                bindings.emplace_back(var->getName(), desc, block_member);
            }
        }
    }
    return result;
}

auto gse::asset_compiler<gse::shader>::source_extensions() -> std::vector<std::string> {
    return { ".slang" };
}

auto gse::asset_compiler<gse::shader>::baked_extension() -> std::string {
    return ".gshader";
}

auto gse::asset_compiler<gse::shader>::source_directory() -> std::string {
    return "Shaders";
}

auto gse::asset_compiler<gse::shader>::baked_directory() -> std::string {
    return "Shaders";
}

auto gse::asset_compiler<gse::shader>::compile_one(const std::filesystem::path& source, const std::filesystem::path& destination) -> bool {
    using namespace shader_compile;

    const std::string filename = source.filename().string();
    if (source.parent_path().filename() == "Layouts") {
        return true;
    }

    const auto shader_root = config::resource_path / "Shaders";

    Slang::ComPtr<slang::IGlobalSession> global_session;
    if (slang_failed(createGlobalSession(global_session.writeRef())) || !global_session) {
        log::println(log::level::error, log::category::assets, "Failed to create Slang global session");
        return false;
    }

    Slang::ComPtr<slang::ISession> session;
    {
        slang::TargetDesc target{
            .format = slang_spirv,
            .profile = global_session->findProfile("spirv_1_5"),
            .forceGLSLScalarBufferLayout = true,
        };

        std::vector<std::string> sp_storage;
        std::vector<const char*> sp_c_strs;
        sp_storage.push_back(shader_root.string());

        for (const auto& e : std::filesystem::recursive_directory_iterator(shader_root)) {
            if (e.is_directory()) {
                sp_storage.push_back(e.path().string());
            }
        }

        sp_c_strs.reserve(sp_storage.size());
        for (auto& s : sp_storage) {
            sp_c_strs.push_back(s.c_str());
        }

        slang::SessionDesc sdesc{
            .targets = &target,
            .targetCount = 1,
            .defaultMatrixLayoutMode = slang_matrix_layout_column_major,
            .searchPaths = sp_c_strs.data(),
            .searchPathCount = static_cast<SlangInt>(sp_c_strs.size()),
        };

        if (slang_failed(global_session->createSession(sdesc, session.writeRef())) || !session) {
            log::println(log::level::error, log::category::assets, "Failed to create Slang session");
            return false;
        }
    }

    Slang::ComPtr<slang::IBlob> diags;
    Slang::ComPtr mod(session->loadModule(source.string().c_str(), diags.writeRef()));

    log_diagnostics(diags.get());

    if (!mod) {
        log::println(log::level::error, log::category::assets, "Failed to load shader module: {}", filename);
        return false;
    }

    Slang::ComPtr<slang::IEntryPoint> vs_ep;
    Slang::ComPtr<slang::IEntryPoint> fs_ep;
    Slang::ComPtr<slang::IEntryPoint> cs_ep;
    Slang::ComPtr<slang::IEntryPoint> as_ep;
    Slang::ComPtr<slang::IEntryPoint> ms_ep;
    {
        const int ep_count = mod->getDefinedEntryPointCount();
        for (int i = 0; i < ep_count; ++i) {
            Slang::ComPtr<slang::IEntryPoint> ep;
            mod->getDefinedEntryPoint(i, ep.writeRef());
            const char* n = ep->getFunctionReflection()->getName();
            if (!std::strcmp(n, "vs_main")) {
                vs_ep = ep;
            } else if (!std::strcmp(n, "fs_main")) {
                fs_ep = ep;
            } else if (!std::strcmp(n, "cs_main")) {
                cs_ep = ep;
            } else if (!std::strcmp(n, "as_main")) {
                as_ep = ep;
            } else if (!std::strcmp(n, "ms_main")) {
                ms_ep = ep;
            }
        }
    }

    const bool is_compute = cs_ep && !vs_ep && !fs_ep && !as_ep && !ms_ep;
    const bool is_graphics = vs_ep && fs_ep && !cs_ep && !as_ep && !ms_ep;
    const bool is_mesh_pipeline = as_ep && ms_ep && fs_ep && !vs_ep && !cs_ep;

    if (!is_compute && !is_graphics && !is_mesh_pipeline) {
        return true;
    }

    Slang::ComPtr<slang::IComponentType> composed;
    if (is_compute) {
        slang::IComponentType* parts[] = { cs_ep.get() };
        if (slang_failed(session->createCompositeComponentType(parts, 1, composed.writeRef(), diags.writeRef()))) {
            return false;
        }
    } else if (is_mesh_pipeline) {
        slang::IComponentType* parts[] = { as_ep.get(), ms_ep.get(), fs_ep.get() };
        if (slang_failed(session->createCompositeComponentType(parts, 3, composed.writeRef(), diags.writeRef()))) {
            return false;
        }
    } else {
        slang::IComponentType* parts[] = { vs_ep.get(), fs_ep.get() };
        if (slang_failed(session->createCompositeComponentType(parts, 2, composed.writeRef(), diags.writeRef()))) {
            return false;
        }
    }

    Slang::ComPtr<slang::IComponentType> program;
    if (slang_failed(composed->link(program.writeRef(), diags.writeRef()))) {
        log_diagnostics(diags.get());
        return false;
    }

    auto* gsess = session->getGlobalSession();

    auto map_layout_kind = [](const SlangInt v) -> std::string {
        switch (v) {
            case 0: return "deferred_3d";
            case 1: return "forward_3d";
            case 2: return "post_process";
            case 3: return "standard_2d";
            case 4: return "standard_3d";
            case 5: return "vbd_physics";
            default: return "";
        }
    };

    auto layout_from_attr = [&](slang::IEntryPoint* ep) -> std::string {
        if (!ep) {
            return "";
        }
        auto* fn = ep->getFunctionReflection();
        slang::UserAttribute* attr = fn->findUserAttributeByName(gsess, "Layout");
        if (!attr) {
            return "";
        }
        int v = 0;
        if (slang_succeeded(attr->getArgumentValueInt(0, &v))) {
            return map_layout_kind(v);
        }
        return "";
    };

    std::string shader_layout_name;
    if (is_compute) {
        shader_layout_name = layout_from_attr(cs_ep.get());
    } else {
        std::array candidates = {
            layout_from_attr(vs_ep.get()),
            layout_from_attr(fs_ep.get()),
            layout_from_attr(as_ep.get()),
            layout_from_attr(ms_ep.get()),
        };

        for (const auto& c : candidates) {
            if (c.empty()) {
                continue;
            }
            if (!shader_layout_name.empty() && shader_layout_name != c) {
                log::println(log::level::warning, log::category::assets, "Mismatched [Layout(...)] across entry points in shader: {}", filename);
                return false;
            }
            shader_layout_name = c;
        }
    }

    slang::ProgramLayout* layout = program->getLayout(0, diags.writeRef());
    if (!layout) {
        return false;
    }

    shader::vertex_input reflected_vertex_input;
    std::unordered_map<gpu::descriptor_set_type, shader::set> reflected_sets;
    std::vector<struct shader::uniform_block> reflected_pcs;

    auto unwrap_to_struct = [](slang::TypeLayoutReflection* tl) -> slang::TypeLayoutReflection* {
        using k = slang::TypeReflection::Kind;
        if (!tl) {
            return nullptr;
        }
        if (tl->getKind() == k::Struct) {
            return tl;
        }
        if (tl->getKind() == k::ConstantBuffer || tl->getKind() == k::ParameterBlock) {
            if (auto* evl = tl->getElementVarLayout()) {
                auto* etl = evl->getTypeLayout();
                return (etl && etl->getKind() == k::Struct) ? etl : nullptr;
            }
        }
        return nullptr;
    };

    auto explicit_location = [&](slang::VariableLayoutReflection* vl) -> std::optional<std::uint32_t> {
        if (!vl) {
            return std::nullopt;
        }
        if (auto* var = vl->getVariable()) {
            if (auto* a = var->findUserAttributeByName(gsess, "vk::location")) {
                int v = 0;
                if (slang_succeeded(a->getArgumentValueInt(0, &v)) && v >= 0) {
                    return static_cast<std::uint32_t>(v);
                }
            }
            if (auto* a = var->findUserAttributeByName(gsess, "location")) {
                int v = 0;
                if (slang_succeeded(a->getArgumentValueInt(0, &v)) && v >= 0) {
                    return static_cast<std::uint32_t>(v);
                }
            }
        }
        if (const char* sem = vl->getSemanticName()) {
            if (std::strncmp(sem, "SV_", 3) && std::strncmp(sem, "builtin_", 8)) {
                const char* end = sem + std::strlen(sem);
                const char* p = end;
                while (p > sem && std::isdigit(static_cast<unsigned char>(p[-1]))) {
                    --p;
                }
                if (p < end) {
                    return static_cast<std::uint32_t>(std::atoi(p));
                }
            }
        }
        return std::nullopt;
    };

    for (std::uint32_t epi = 0; epi < layout->getEntryPointCount(); ++epi) {
        auto* ep = layout->getEntryPointByIndex(epi);
        if (!ep || ep->getStage() != slang_stage_vertex) {
            continue;
        }

        auto* scope_vl = ep->getVarLayout();
        auto* scope_tl = scope_vl ? scope_vl->getTypeLayout() : nullptr;
        auto* param_struct = unwrap_to_struct(scope_tl);
        if (!param_struct) {
            continue;
        }

        std::uint32_t next_loc = 0;
        std::uint64_t used_locations = 0;

        auto bit_for = [](const std::uint32_t b) -> std::uint64_t {
            return b < 64 ? (std::uint64_t{1} << b) : 0;
        };

        auto visit_input = [&](this auto&& self, slang::VariableLayoutReflection* vl) -> void {
            if (!vl) {
                return;
            }
            if (const char* sem = vl->getSemanticName()) {
                if (!std::strncmp(sem, "SV_", 3) || !std::strncmp(sem, "builtin_", 8)) {
                    return;
                }
            }

            using k = slang::TypeReflection::Kind;
            auto* tl = vl->getTypeLayout();
            if (!tl) {
                return;
            }

            if (tl->getKind() == k::Struct) {
                for (int i = 0; i < tl->getFieldCount(); ++i) {
                    self(tl->getFieldByIndex(i));
                }
                return;
            }
            if (tl->getKind() == k::Array) {
                if (auto* evl = tl->getElementVarLayout()) {
                    self(evl);
                }
                return;
            }
            if (tl->getKind() == k::Resource || tl->getKind() == k::SamplerState || tl->getKind() == k::Matrix || tl->getKind() == k::ConstantBuffer || tl->getKind() == k::ParameterBlock) {
                return;
            }

            auto* ty = tl->getType();
            if (!ty) {
                return;
            }

            const auto fmt = to_vertex_format(ty);

            std::uint32_t loc;
            if (const auto el = explicit_location(vl); el && !(used_locations & bit_for(*el))) {
                loc = *el;
            } else {
                while (used_locations & bit_for(next_loc)) {
                    ++next_loc;
                }
                loc = next_loc++;
            }
            used_locations |= bit_for(loc);

            reflected_vertex_input.attributes.push_back({
                .location = loc,
                .binding = 0,
                .format = fmt,
                .offset = 0,
            });
        };

        for (int i = 0; i < param_struct->getFieldCount(); ++i) {
            visit_input(param_struct->getFieldByIndex(i));
        }
    }

    std::ranges::sort(reflected_vertex_input.attributes, [](const auto& a, const auto& b) {
        return a.location < b.location;
    });

    auto [sets] = reflect_descriptor_sets(layout, to_descriptor_type, reflect_members, extract_struct_layout);
    gpu::stage_flags used_stages{};

    for (std::uint32_t epi = 0; epi < layout->getEntryPointCount(); ++epi) {
        auto* ep = layout->getEntryPointByIndex(epi);
        if (!ep) {
            continue;
        }
        used_stages |= to_stage_flags(ep->getStage());
    }

    for (auto& [type, bindings] : sets | std::views::values) {
        for (auto& b : bindings) {
            b.desc.stages = used_stages;
        }
    }

    reflected_sets = std::move(sets);

    auto process_push_constant_variable = [&](slang::VariableLayoutReflection* var, std::vector<struct shader::uniform_block>& pcs, const gpu::stage_flags stage) {
        if (!var || var->getCategory() != slang::ParameterCategory::PushConstantBuffer) {
            return;
        }
        auto* struct_layout = extract_struct_layout(var);
        if (!struct_layout) {
            return;
        }

        const std::string block_name = struct_layout->getName() && struct_layout->getName()[0] ? struct_layout->getName() : "push_constants";

        const auto it = std::ranges::find_if(pcs, [&](const auto& b) {
            return b.name == block_name;
        });

        if (it != pcs.end()) {
            it->stage_flags |= stage;
        } else {
            struct shader::uniform_block b{
                .name = block_name,
                .size = static_cast<std::uint32_t>(struct_layout->getSize(slang::ParameterCategory::Uniform)),
                .members = reflect_members(struct_layout, slang::ParameterCategory::Uniform),
                .stage_flags = stage,
            };
            pcs.push_back(std::move(b));
        }
    };

    for (std::uint32_t epi = 0; epi < layout->getEntryPointCount(); ++epi) {
        if (auto* ep = layout->getEntryPointByIndex(epi)) {
            gpu::stage_flags stage_flag = to_stage_flags(ep->getStage());
            process_push_constant_variable(ep->getVarLayout(), reflected_pcs, stage_flag);
        }
    }

    if (auto* globals_vl = layout->getGlobalParamsVarLayout()) {
        gpu::stage_flags all_entry_point_stages = {};
        for (std::uint32_t epi = 0; epi < layout->getEntryPointCount(); ++epi) {
            if (auto* ep = layout->getEntryPointByIndex(epi)) {
                all_entry_point_stages |= to_stage_flags(ep->getStage());
            }
        }

        if (auto* struct_layout = extract_struct_layout(globals_vl)) {
            for (int i = 0; i < struct_layout->getFieldCount(); ++i) {
                process_push_constant_variable(struct_layout->getFieldByIndex(i), reflected_pcs, all_entry_point_stages);
            }
        }
    }

    std::filesystem::create_directories(destination.parent_path());
    std::ofstream out(destination, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }

    binary_writer ar(out, 0x47534852, 1);

    const std::uint8_t shader_type = is_compute ? static_cast<std::uint8_t>(1) : is_mesh_pipeline ? static_cast<std::uint8_t>(2) : static_cast<std::uint8_t>(0);
    ar & shader_type & shader_layout_name & reflected_vertex_input.attributes;
    ar & shader::layout{ .sets = std::move(reflected_sets) };
    ar & reflected_pcs;

    auto write_spirv = [&](const std::uint32_t entry_point_index) -> bool {
        Slang::ComPtr<ISlangBlob> blob;
        Slang::ComPtr<ISlangBlob> gen_diags;
        if (slang_failed(program->getEntryPointCode(entry_point_index, 0, blob.writeRef(), gen_diags.writeRef())) || !blob) {
            return false;
        }
        const auto byte_size = blob->getBufferSize();
        std::vector<std::uint32_t> spirv(byte_size / sizeof(std::uint32_t));
        std::memcpy(spirv.data(), blob->getBufferPointer(), byte_size);
        ar & raw_blob(spirv);
        return true;
    };

    if (is_compute) {
        if (!write_spirv(0)) {
            return false;
        }
    } else if (is_mesh_pipeline) {
        if (!write_spirv(0)) {
            return false;
        }
        if (!write_spirv(1)) {
            return false;
        }
        if (!write_spirv(2)) {
            return false;
        }
    } else {
        if (!write_spirv(0)) {
            return false;
        }
        if (!write_spirv(1)) {
            return false;
        }
    }

    constexpr const char* type_names[] = { "Graphics", "Compute", "Mesh Pipeline" };
    log::println(log::category::assets, "Shader compiled: {} ({})", destination.filename().string(), type_names[shader_type]);
    return true;
}

auto gse::asset_compiler<gse::shader>::needs_recompile(const std::filesystem::path& source, const std::filesystem::path& destination) -> bool {
    if (!std::filesystem::exists(destination)) {
        return true;
    }
    return std::filesystem::last_write_time(source) > std::filesystem::last_write_time(destination);
}

auto gse::asset_compiler<gse::shader>::dependencies(const std::filesystem::path& source) -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> deps;
    const auto shader_root = config::resource_path / "Shaders";
    if (!std::filesystem::exists(shader_root)) {
        return deps;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(shader_root)) {
        if (entry.is_regular_file() && entry.path() != source) {
            deps.push_back(entry.path());
        }
    }
    return deps;
}
