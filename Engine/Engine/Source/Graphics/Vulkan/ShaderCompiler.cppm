module;

#include <cstdio>
#include <slang-com-ptr.h>
#include <slang.h>

#undef assert

export module gse.graphics:shader_compiler;

import std;

import gse.platform;
import gse.assert;

import :asset_compiler;
import :shader;

namespace gse::shader_compile {
    auto write_string(std::ofstream& stream, const std::string& str) -> void {
        const auto size = static_cast<std::uint32_t>(str.size());
        stream.write(reinterpret_cast<const char*>(&size), sizeof(size));
        stream.write(str.c_str(), size);
    }

    template<typename T>
    auto write_data(std::ofstream& stream, const T& value) -> void {
        stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    auto to_vk_vertex_format(slang::TypeReflection* type) -> vk::Format {
        using kind = slang::TypeReflection::Kind;

        auto elem_and_count = [](slang::TypeReflection* t) -> std::pair<slang::TypeReflection*, int> {
            switch (t->getKind()) {
                case kind::Vector: return { t->getElementType(), static_cast<int>(t->getElementCount()) };
                case kind::Scalar: return { t, 1 };
                default: return { t, 0 };
            }
        };

        auto [elem, count] = elem_and_count(type);
        if (!elem || count <= 0) return vk::Format::eUndefined;

        switch (elem->getScalarType()) {
            case slang::TypeReflection::Float32:
                switch (count) {
                    case 1: return vk::Format::eR32Sfloat;
                    case 2: return vk::Format::eR32G32Sfloat;
                    case 3: return vk::Format::eR32G32B32Sfloat;
                    case 4: return vk::Format::eR32G32B32A32Sfloat;
                    default: return vk::Format::eUndefined;
                }
            case slang::TypeReflection::Int32:
                switch (count) {
                    case 1: return vk::Format::eR32Sint;
                    case 2: return vk::Format::eR32G32Sint;
                    case 3: return vk::Format::eR32G32B32Sint;
                    case 4: return vk::Format::eR32G32B32A32Sint;
                    default: return vk::Format::eUndefined;
                }
            case slang::TypeReflection::UInt32:
                switch (count) {
                    case 1: return vk::Format::eR32Uint;
                    case 2: return vk::Format::eR32G32Uint;
                    case 3: return vk::Format::eR32G32B32Uint;
                    case 4: return vk::Format::eR32G32B32A32Uint;
                    default: return vk::Format::eUndefined;
                }
            default: break;
        }
        return vk::Format::eUndefined;
    }

    auto to_vk_descriptor_type(
        slang::TypeLayoutReflection* tl, 
        const int range_index
    ) -> vk::DescriptorType {
        if (tl == nullptr) return vk::DescriptorType::eStorageBuffer;
        if (const int range_count = tl->getBindingRangeCount(); range_index < 0 || range_index >= range_count) {
            return vk::DescriptorType::eStorageBuffer;
        }

        const slang::BindingType bt = tl->getBindingRangeType(range_index);
        const SlangBindingTypeIntegral bt_bits = static_cast<SlangBindingTypeIntegral>(bt);
        const auto base_bt = static_cast<SlangBindingType>(bt_bits & SLANG_BINDING_TYPE_BASE_MASK);
        const bool is_mutable = (bt_bits & SLANG_BINDING_TYPE_MUTABLE_FLAG) != 0;

        slang::TypeLayoutReflection* leaf_layout = tl->getBindingRangeLeafTypeLayout(range_index);
        slang::TypeReflection* leaf_type = leaf_layout ? leaf_layout->getType() : nullptr;
        const auto access = leaf_type ? leaf_type->getResourceAccess() : SLANG_RESOURCE_ACCESS_READ;
        const auto shape = leaf_type ? static_cast<SlangResourceShape>(leaf_type->getResourceShape() & SLANG_RESOURCE_BASE_SHAPE_MASK) : static_cast<SlangResourceShape>(0);

        switch (base_bt) {
            case SLANG_BINDING_TYPE_COMBINED_TEXTURE_SAMPLER: return vk::DescriptorType::eCombinedImageSampler;
            case SLANG_BINDING_TYPE_TEXTURE:
                if (shape == SLANG_TEXTURE_BUFFER) {
                    return (is_mutable || access != SLANG_RESOURCE_ACCESS_READ) ? vk::DescriptorType::eStorageTexelBuffer : vk::DescriptorType::eUniformTexelBuffer;
                }
                return (is_mutable || access != SLANG_RESOURCE_ACCESS_READ) ? vk::DescriptorType::eStorageImage : vk::DescriptorType::eSampledImage;
            case SLANG_BINDING_TYPE_SAMPLER: return vk::DescriptorType::eSampler;
            case SLANG_BINDING_TYPE_TYPED_BUFFER:
                return (is_mutable || access != SLANG_RESOURCE_ACCESS_READ) ? vk::DescriptorType::eStorageTexelBuffer : vk::DescriptorType::eUniformTexelBuffer;
            case SLANG_BINDING_TYPE_RAW_BUFFER: return vk::DescriptorType::eStorageBuffer;
            case SLANG_BINDING_TYPE_CONSTANT_BUFFER:
            case SLANG_BINDING_TYPE_PARAMETER_BLOCK: return vk::DescriptorType::eUniformBuffer;
            case SLANG_BINDING_TYPE_INPUT_RENDER_TARGET: return vk::DescriptorType::eSampledImage;
            case SLANG_BINDING_TYPE_RAY_TRACING_ACCELERATION_STRUCTURE: return vk::DescriptorType::eAccelerationStructureKHR;
            default: return vk::DescriptorType::eStorageBuffer;
        }
    }

    auto reflect_members(
        slang::TypeLayoutReflection* struct_layout,
        const std::optional<slang::ParameterCategory> preferred_cat = std::nullopt
    ) -> std::unordered_map<std::string, shader::uniform_member> {
        using kind = slang::TypeReflection::Kind;
        std::unordered_map<std::string, shader::uniform_member> members;

        if (!struct_layout || struct_layout->getKind() != kind::Struct) return members;

        const slang::ParameterCategory order[] = {
            preferred_cat.value_or(slang::ParameterCategory::Uniform),
            slang::ParameterCategory::ShaderResource,
            slang::ParameterCategory::UnorderedAccess,
            slang::ParameterCategory::Uniform
        };

        slang::ParameterCategory chosen = order[0];
        bool ok = false;
        for (const auto c : order) {
            if (struct_layout->getSize(c) > 0) { chosen = c; ok = true; break; }
        }
        if (!ok) chosen = slang::ParameterCategory::Uniform;

        for (int i = 0; i < struct_layout->getFieldCount(); ++i) {
            auto* m_var = struct_layout->getFieldByIndex(i);
            auto* m_type = m_var ? m_var->getTypeLayout() : nullptr;
            if (!m_type) continue;

            const auto k = m_type->getKind();
            if (k == kind::Resource || k == kind::SamplerState) continue;

            std::uint32_t sz = static_cast<std::uint32_t>(m_type->getSize(chosen));
            if (sz == 0) sz = static_cast<std::uint32_t>(m_type->getSize(slang::ParameterCategory::Uniform));
            if (sz == 0) continue;

            std::uint32_t off = static_cast<std::uint32_t>(m_var->getOffset(chosen));
            if (off == 0 && sz != 0) off = static_cast<std::uint32_t>(m_var->getOffset(slang::ParameterCategory::Uniform));

            members[m_var->getName()] = {
                .name = m_var->getName(),
                .type_name = m_type->getType() ? m_type->getType()->getName() : std::string{},
                .offset = off,
                .size = sz,
                .array_size = (k == kind::Array) ? static_cast<std::uint32_t>(m_type->getElementCount()) : 0u
            };
        }
        return members;
    }

    auto extract_struct_layout(slang::VariableLayoutReflection* var) -> slang::TypeLayoutReflection* {
        if (!var) return nullptr;
        auto* type = var->getTypeLayout();
        if (!type) return nullptr;

        slang::TypeLayoutReflection* struct_layout = nullptr;
        switch (type->getKind()) {
            case slang::TypeReflection::Kind::ConstantBuffer:
            case slang::TypeReflection::Kind::ParameterBlock:
                if (auto* elem_var = type->getElementVarLayout()) struct_layout = elem_var->getTypeLayout();
                break;
            case slang::TypeReflection::Kind::Struct:
                struct_layout = type;
                break;
            default: break;
        }
        return struct_layout && struct_layout->getKind() == slang::TypeReflection::Kind::Struct ? struct_layout : nullptr;
    }

    auto to_vk_stage_flags(const SlangStage slang_stage) -> vk::ShaderStageFlags {
        switch (slang_stage) {
            case SLANG_STAGE_VERTEX:   return vk::ShaderStageFlagBits::eVertex;
            case SLANG_STAGE_FRAGMENT: return vk::ShaderStageFlagBits::eFragment;
            case SLANG_STAGE_GEOMETRY: return vk::ShaderStageFlagBits::eGeometry;
            case SLANG_STAGE_COMPUTE:  return vk::ShaderStageFlagBits::eCompute;
            default: return {};
        }
    }

    auto reflect_descriptor_sets(
        slang::ProgramLayout* program_layout,
        const std::function<vk::DescriptorType(slang::TypeLayoutReflection*, int)>& to_vk_desc_type,
        const std::function<std::unordered_map<std::string, shader::uniform_member>(slang::TypeLayoutReflection*, std::optional<slang::ParameterCategory>)>& reflect_mem,
        const std::function<slang::TypeLayoutReflection*(slang::VariableLayoutReflection*)>& extract_struct
    ) -> struct shader::layout {
        struct shader::layout result;
        auto* globals_vl = program_layout->getGlobalParamsVarLayout();
        if (!globals_vl) return result;

        auto* globals_tl = globals_vl->getTypeLayout();
        if (!globals_tl) return result;

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

        if (!element_tl || element_tl->getKind() != slang::TypeReflection::Kind::Struct) return result;

        const auto container_space = static_cast<std::uint32_t>(container_vl ? container_vl->getBindingSpace(slang::ParameterCategory::DescriptorTableSlot) : 0);
        const int field_count = element_tl->getFieldCount();

        for (int i = 0; i < field_count; ++i) {
            auto* var = element_tl->getFieldByIndex(i);
            auto* tl = var->getTypeLayout();
            const auto binding_range_count = tl->getBindingRangeCount();

            for (int range_index = 0; range_index < binding_range_count; ++range_index) {
                const auto binding = static_cast<std::uint32_t>(var->getOffset(slang::ParameterCategory::DescriptorTableSlot));
                const auto set_idx = container_space + static_cast<std::uint32_t>(var->getBindingSpace(slang::ParameterCategory::DescriptorTableSlot));

                auto set_type = static_cast<shader::set::binding_type>(set_idx);
                auto& current_set = result.sets[set_type];
                current_set.type = set_type;

                vk::DescriptorSetLayoutBinding layout_binding{
                    .binding = binding,
                    .descriptorType = to_vk_desc_type(tl, range_index),
                    .descriptorCount = tl->getKind() == slang::TypeReflection::Kind::Array ? static_cast<std::uint32_t>(tl->getElementCount()) : 1u,
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
                            (type_layout->getType()->getResourceShape() & SLANG_RESOURCE_BASE_SHAPE_MASK) == SLANG_STRUCTURED_BUFFER);

                    if (!is_structured) return std::nullopt;

                    slang::TypeLayoutReflection* elem_tl = nullptr;
                    if (type_layout->getKind() == kind::ShaderStorageBuffer) {
                        if (auto* evl = type_layout->getElementVarLayout()) elem_tl = evl->getTypeLayout();
                    } else {
                        elem_tl = type_layout->getElementTypeLayout();
                    }

                    if (!elem_tl || elem_tl->getKind() != kind::Struct) return std::nullopt;

                    const auto cat = (type_layout->getKind() == kind::ShaderStorageBuffer) ? slang::ParameterCategory::UnorderedAccess : slang::ParameterCategory::ShaderResource;

                    std::uint32_t elem_stride = static_cast<std::uint32_t>(elem_tl->getStride(cat));
                    if (elem_stride == 0) {
                        const std::uint32_t sz = static_cast<std::uint32_t>(elem_tl->getSize(slang::ParameterCategory::Uniform));
                        std::uint32_t al = static_cast<std::uint32_t>(elem_tl->getAlignment(slang::ParameterCategory::Uniform));
                        if (al == 0) al = 16;
                        if (sz) elem_stride = (sz + (al - 1)) & ~(al - 1);
                    }

                    auto members = reflect_mem(elem_tl, cat);

                    if (elem_stride == 0 && !members.empty()) {
                        uint32_t max_end = 0;
                        for (const auto& m : members | std::views::values) {
                            max_end = std::max(max_end, m.offset + m.size);
                        }
                        constexpr std::uint32_t align = 16;
                        elem_stride = (max_end + (align - 1)) & ~(align - 1);
                    }

                    if (elem_stride == 0 && members.empty()) return std::nullopt;

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
                            .binding = binding,
                            .set = set_idx,
                            .size = static_cast<std::uint32_t>(struct_layout->getSize(slang::ParameterCategory::Uniform)),
                            .members = reflect_mem(struct_layout, slang::ParameterCategory::Uniform)
                        };
                        if (block.size > 0 && !block.members.empty()) block_member = block;
                    }
                } else {
                    block_member = try_structured_buffer_block(var, tl, binding, set_idx);
                }

                const bool binding_exists = std::ranges::any_of(current_set.bindings, [&](const auto& b) {
                    return b.layout_binding.binding == binding;
                });

                if (!binding_exists) {
                    current_set.bindings.emplace_back(var->getName(), layout_binding, block_member);
                }
            }
        }
        return result;
    }
}

export template<>
struct gse::asset_compiler<gse::shader> {
    static auto source_extensions() -> std::vector<std::string> {
        return { ".slang" };
    }

    static auto baked_extension() -> std::string {
        return ".gshader";
    }

    static auto source_directory() -> std::string {
        return "Shaders";
    }

    static auto baked_directory() -> std::string {
        return "Shaders";
    }

    static auto compile_one(
        const std::filesystem::path& source,
        const std::filesystem::path& destination
    ) -> bool {
        using namespace shader_compile;

        const std::string filename = source.filename().string();
        if (source.parent_path().filename() == "Layouts" || filename == "common.slang") {
            return true;
        }

        const auto shader_root = config::resource_path / "Shaders";
        const auto layout_dir_path = shader_root / "Layouts";

        Slang::ComPtr<slang::IGlobalSession> global_session;
        if (SLANG_FAILED(createGlobalSession(global_session.writeRef())) || !global_session) {
            std::println("Failed to create Slang global session");
            return false;
        }

        Slang::ComPtr<slang::ISession> session;
        {
            slang::TargetDesc target{
                .format = SLANG_SPIRV,
                .profile = global_session->findProfile("spirv_1_5")
            };

            std::vector<std::string> sp_storage;
            std::vector<const char*> sp_c_strs;
            sp_storage.push_back(shader_root.string());

            for (const auto& e : std::filesystem::recursive_directory_iterator(shader_root)) {
                if (e.is_directory()) sp_storage.push_back(e.path().string());
            }

            sp_c_strs.reserve(sp_storage.size());
            for (auto& s : sp_storage) sp_c_strs.push_back(s.c_str());

            slang::SessionDesc sdesc{
                .targets = &target,
                .targetCount = 1,
                .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
                .searchPaths = sp_c_strs.data(),
                .searchPathCount = static_cast<SlangInt>(sp_c_strs.size()),
            };

            if (SLANG_FAILED(global_session->createSession(sdesc, session.writeRef())) || !session) {
                std::println("Failed to create Slang session");
                return false;
            }
        }


        Slang::ComPtr<slang::IBlob> diags;
        Slang::ComPtr mod(session->loadModule(source.string().c_str(), diags.writeRef()));

        if (diags && diags->getBufferSize()) {
            std::fprintf(stderr, "%s", static_cast<const char*>(diags->getBufferPointer()));
        }

        if (!mod) {
            std::println("Failed to load shader module: {}", filename);
            return false;
        }

        Slang::ComPtr<slang::IEntryPoint> vs_ep, fs_ep, cs_ep;
        {
            const int ep_count = mod->getDefinedEntryPointCount();
            for (int i = 0; i < ep_count; ++i) {
                Slang::ComPtr<slang::IEntryPoint> ep;
                mod->getDefinedEntryPoint(i, ep.writeRef());
                if (const char* n = ep->getFunctionReflection()->getName(); !std::strcmp(n, "vs_main")) {
                    vs_ep = ep;
                } else if (!std::strcmp(n, "fs_main")) {
                    fs_ep = ep;
                } else if (!std::strcmp(n, "cs_main")) {
                    cs_ep = ep;
                }
            }
        }

        const bool is_compute = cs_ep && !vs_ep && !fs_ep;
        const bool is_graphics = vs_ep && fs_ep && !cs_ep;

        if (!is_compute && !is_graphics) {
            std::println("Shader '{}' must define either (vs_main and fs_main) or cs_main", filename);
            return false;
        }

        Slang::ComPtr<slang::IComponentType> composed;
        {
            if (is_compute) {
                slang::IComponentType* parts[] = { cs_ep.get() };
                if (SLANG_FAILED(session->createCompositeComponentType(parts, 1, composed.writeRef(), diags.writeRef()))) {
                    return false;
                }
            } else {
                slang::IComponentType* parts[] = { vs_ep.get(), fs_ep.get() };
                if (SLANG_FAILED(session->createCompositeComponentType(parts, 2, composed.writeRef(), diags.writeRef()))) {
                    return false;
                }
            }
        }

        Slang::ComPtr<slang::IComponentType> program;
        if (SLANG_FAILED(composed->link(program.writeRef(), diags.writeRef()))) {
            if (diags && diags->getBufferSize()) {
                std::fprintf(stderr, "%s", static_cast<const char*>(diags->getBufferPointer()));
            }
            return false;
        }

        // Map LayoutKind enum values (from common.slang) to layout file names
        auto map_layout_kind = [](const SlangInt v) -> std::string {
            switch (v) {
                case 0: return "deferred_3d";
                case 1: return "forward_3d";
                case 2: return "post_process";
                case 3: return "standard_2d";
                case 4: return "standard_3d";
                default: return "";
            }
        };

        auto layout_from_attr = [&](slang::IEntryPoint* ep) -> std::string {
            if (!ep) return "";
            auto* fn = ep->getFunctionReflection();
            slang::UserAttribute* attr = fn->findUserAttributeByName(session->getGlobalSession(), "Layout");
            if (!attr) return "";
            int v = 0;
            if (SLANG_SUCCEEDED(attr->getArgumentValueInt(0, &v))) {
                return map_layout_kind(v);
            }
            return "";
        };

        std::string shader_layout_name;
        if (!is_compute) {
            auto v_layout = layout_from_attr(vs_ep.get());
            auto f_layout = layout_from_attr(fs_ep.get());
            if (!v_layout.empty() && !f_layout.empty() && v_layout != f_layout) {
                std::println("Mismatched [Layout(...)] on vs_main/fs_main in shader: {}", filename);
                return false;
            }
            shader_layout_name = !v_layout.empty() ? v_layout : f_layout;
        }

        slang::ProgramLayout* layout = program->getLayout(0, diags.writeRef());
        if (!layout) {
            return false;
        }

        shader::vertex_input reflected_vertex_input;
        std::unordered_map<shader::set::binding_type, shader::set> reflected_sets;
        std::vector<struct shader::uniform_block> reflected_pcs;

        auto unwrap_to_struct = [](slang::TypeLayoutReflection* tl) -> slang::TypeLayoutReflection* {
            using k = slang::TypeReflection::Kind;
            if (!tl) return nullptr;
            if (tl->getKind() == k::Struct) return tl;
            if (tl->getKind() == k::ConstantBuffer || tl->getKind() == k::ParameterBlock) {
                if (auto* evl = tl->getElementVarLayout()) {
                    auto* etl = evl->getTypeLayout();
                    return (etl && etl->getKind() == k::Struct) ? etl : nullptr;
                }
            }
            return nullptr;
        };

        auto explicit_location = [&](slang::VariableLayoutReflection* vl, slang::IGlobalSession* g) -> std::optional<std::uint32_t> {
            if (!vl) return std::nullopt;
            if (auto* var = vl->getVariable()) {
                if (auto* a = var->findUserAttributeByName(g, "vk::location")) {
                    int v = 0;
                    if (SLANG_SUCCEEDED(a->getArgumentValueInt(0, &v)) && v >= 0) return static_cast<uint32_t>(v);
                }
                if (auto* a = var->findUserAttributeByName(g, "location")) {
                    int v = 0;
                    if (SLANG_SUCCEEDED(a->getArgumentValueInt(0, &v)) && v >= 0) return static_cast<uint32_t>(v);
                }
            }
            if (const char* sem = vl->getSemanticName()) {
                if (std::strncmp(sem, "SV_", 3) && std::strncmp(sem, "builtin_", 8)) {
                    const char* end = sem + std::strlen(sem);
                    const char* p = end;
                    while (p > sem && std::isdigit(static_cast<unsigned char>(p[-1]))) --p;
                    if (p < end) return static_cast<std::uint32_t>(std::atoi(p));
                }
            }
            return std::nullopt;
        };

        for (std::uint32_t epi = 0; epi < layout->getEntryPointCount(); ++epi) {
            auto* ep = layout->getEntryPointByIndex(epi);
            if (!ep || ep->getStage() != SLANG_STAGE_VERTEX) continue;

            auto* scope_vl = ep->getVarLayout();
            auto* scope_tl = scope_vl ? scope_vl->getTypeLayout() : nullptr;
            auto* param_struct = unwrap_to_struct(scope_tl);
            if (!param_struct) continue;

            std::uint32_t next_loc = 0;
            std::unordered_set<std::uint32_t> used_locations;

            std::function<void(slang::VariableLayoutReflection*)> visit_input;
            visit_input = [&](slang::VariableLayoutReflection* vl) {
                if (!vl) return;
                if (const char* sem = vl->getSemanticName()) {
                    if (!std::strncmp(sem, "SV_", 3) || !std::strncmp(sem, "builtin_", 8)) return;
                }

                using k = slang::TypeReflection::Kind;
                auto* tl = vl->getTypeLayout();
                if (!tl) return;

                if (tl->getKind() == k::Struct) {
                    for (int i = 0; i < tl->getFieldCount(); ++i) visit_input(tl->getFieldByIndex(i));
                    return;
                }
                if (tl->getKind() == k::Array) {
                    if (auto* evl = tl->getElementVarLayout()) visit_input(evl);
                    return;
                }
                if (tl->getKind() == k::Resource || tl->getKind() == k::SamplerState ||
                    tl->getKind() == k::Matrix || tl->getKind() == k::ConstantBuffer ||
                    tl->getKind() == k::ParameterBlock) return;

                auto* ty = tl->getType();
                if (!ty) return;

                const vk::Format fmt = to_vk_vertex_format(ty);
                if (fmt == vk::Format::eUndefined) return;

                std::uint32_t loc;
                if (const auto el = explicit_location(vl, session->getGlobalSession()); el && !used_locations.contains(*el)) {
                    loc = *el;
                } else {
                    while (used_locations.contains(next_loc)) ++next_loc;
                    loc = next_loc++;
                }
                used_locations.insert(loc);

                reflected_vertex_input.attributes.push_back(vk::VertexInputAttributeDescription{
                    .location = loc,
                    .binding = 0,
                    .format = fmt,
                    .offset = 0
                });
            };

            for (int i = 0; i < param_struct->getFieldCount(); ++i) {
                visit_input(param_struct->getFieldByIndex(i));
            }
        }

        std::ranges::sort(reflected_vertex_input.attributes, {}, &vk::VertexInputAttributeDescription::location);

        auto [sets] = reflect_descriptor_sets(layout, to_vk_descriptor_type, reflect_members, extract_struct_layout);
        vk::ShaderStageFlags used_stages{};

        for (std::uint32_t epi = 0; epi < layout->getEntryPointCount(); ++epi) {
            auto* ep = layout->getEntryPointByIndex(epi);
            if (!ep) continue;
            switch (ep->getStage()) {
                case SLANG_STAGE_VERTEX:   used_stages |= vk::ShaderStageFlagBits::eVertex;   break;
                case SLANG_STAGE_FRAGMENT: used_stages |= vk::ShaderStageFlagBits::eFragment; break;
                case SLANG_STAGE_COMPUTE:  used_stages |= vk::ShaderStageFlagBits::eCompute;  break;
                default: break;
            }
        }

        for (auto& s_data : sets | std::views::values) {
            for (auto& [n, lb, mem] : s_data.bindings) lb.stageFlags = used_stages;
        }

        reflected_sets = std::move(sets);

        auto process_push_constant_variable = [&](
            slang::VariableLayoutReflection* var,
            std::vector<struct shader::uniform_block>& pcs,
            const vk::ShaderStageFlags stage
        ) {
            if (!var || var->getCategory() != slang::ParameterCategory::PushConstantBuffer) return;
            auto* struct_layout = extract_struct_layout(var);
            if (!struct_layout) return;

            const std::string block_name = struct_layout->getName() && struct_layout->getName()[0]
                ? struct_layout->getName() : "push_constants";

            const auto it = std::ranges::find_if(pcs, [&](const auto& b) { return b.name == block_name; });

            if (it != pcs.end()) {
                it->stage_flags |= stage;
            } else {
                struct shader::uniform_block b{
                    .name = block_name,
                    .size = static_cast<std::uint32_t>(struct_layout->getSize(slang::ParameterCategory::Uniform)),
                    .members = reflect_members(struct_layout, slang::ParameterCategory::Uniform),
                    .stage_flags = stage
                };
                pcs.push_back(std::move(b));
            }
        };

        for (std::uint32_t epi = 0; epi < layout->getEntryPointCount(); ++epi) {
            if (auto* ep = layout->getEntryPointByIndex(epi)) {
                vk::ShaderStageFlags stage_flag = to_vk_stage_flags(ep->getStage());
                process_push_constant_variable(ep->getVarLayout(), reflected_pcs, stage_flag);
            }
        }

        if (auto* globals_vl = layout->getGlobalParamsVarLayout()) {
            vk::ShaderStageFlags all_entry_point_stages = {};
            for (std::uint32_t epi = 0; epi < layout->getEntryPointCount(); ++epi) {
                if (auto* ep = layout->getEntryPointByIndex(epi)) {
                    all_entry_point_stages |= to_vk_stage_flags(ep->getStage());
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
        if (!out.is_open()) return false;

        write_data(out, is_compute);
        write_string(out, shader_layout_name);
        write_data(out, static_cast<std::uint32_t>(reflected_vertex_input.attributes.size()));
        for (const auto& attr : reflected_vertex_input.attributes) write_data(out, attr);

        write_data(out, static_cast<std::uint32_t>(reflected_sets.size()));
        for (const auto& [type, set_data] : reflected_sets) {
            write_data(out, type);
            write_data(out, static_cast<std::uint32_t>(set_data.bindings.size()));
            for (const auto& [name, layout_binding, member] : set_data.bindings) {
                write_string(out, name);
                write_data(out, layout_binding);
                write_data(out, member.has_value());
                if (member) {
                    write_string(out, member->name);
                    write_data(out, member->binding);
                    write_data(out, member->set);
                    write_data(out, member->size);
                    write_data(out, static_cast<std::uint32_t>(member->members.size()));
                    for (const auto& [m_name, m_data] : member->members) {
                        write_string(out, m_name);
                        write_string(out, m_data.name);
                        write_data(out, m_data.offset);
                        write_data(out, m_data.size);
                        write_data(out, m_data.array_size);
                    }
                }
            }
        }

        write_data(out, static_cast<std::uint32_t>(reflected_pcs.size()));
        for (const auto& pc : reflected_pcs) {
            write_string(out, pc.name);
            write_data(out, pc.size);
            write_data(out, pc.stage_flags);
            write_data(out, static_cast<std::uint32_t>(pc.members.size()));
            for (const auto& [m_name, m_data] : pc.members) {
                write_string(out, m_name);
                write_string(out, m_data.name);
                write_data(out, m_data.offset);
                write_data(out, m_data.size);
                write_data(out, m_data.array_size);
            }
        }

        if (is_compute) {
            Slang::ComPtr<ISlangBlob> compute_blob, gen_diags;
            if (SLANG_FAILED(program->getEntryPointCode(0, 0, compute_blob.writeRef(), gen_diags.writeRef())) || !compute_blob) {
                return false;
            }
            const auto compute_size = compute_blob->getBufferSize();
            write_data(out, compute_size);
            out.write(static_cast<const char*>(compute_blob->getBufferPointer()), compute_size);
        } else {
            Slang::ComPtr<ISlangBlob> vert_blob, frag_blob, gen_diags;
            if (SLANG_FAILED(program->getEntryPointCode(0, 0, vert_blob.writeRef(), gen_diags.writeRef())) || !vert_blob) {
                return false;
            }
            if (SLANG_FAILED(program->getEntryPointCode(1, 0, frag_blob.writeRef(), gen_diags.writeRef())) || !frag_blob) {
                return false;
            }
            const auto vert_size = vert_blob->getBufferSize();
            const auto frag_size = frag_blob->getBufferSize();
            write_data(out, vert_size);
            out.write(static_cast<const char*>(vert_blob->getBufferPointer()), vert_size);
            write_data(out, frag_size);
            out.write(static_cast<const char*>(frag_blob->getBufferPointer()), frag_size);
        }

        std::println("Shader compiled: {} ({})", destination.filename().string(), is_compute ? "Compute" : "Graphics");
        return true;
    }

    static auto needs_recompile(
        const std::filesystem::path& source,
        const std::filesystem::path& destination
    ) -> bool {
        if (!std::filesystem::exists(destination)) {
            return true;
        }
        return std::filesystem::last_write_time(source) >
               std::filesystem::last_write_time(destination);
    }

    static auto dependencies(const std::filesystem::path& source) -> std::vector<std::filesystem::path> {
        std::vector<std::filesystem::path> deps;
        if (const auto layout_dir = config::resource_path / "Shaders" / "Layouts"; std::filesystem::exists(layout_dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(layout_dir)) {
                if (entry.is_regular_file()) {
                    deps.push_back(entry.path());
                }
            }
        }
        if (const auto common_file = config::resource_path / "Shaders" / "common.slang"; std::filesystem::exists(common_file)) {
            deps.push_back(common_file);
        }
        return deps;
    }
};
