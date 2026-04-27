export module gse.gpu.shader:shader_layout_compiler;

import std;

import gse.gpu.vulkan;
import gse.assert;
import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.assets;
import gse.slang;
import :shader_layout;

namespace gse::layout_compile {
    auto log_diagnostics(slang::IBlob* diagnostics) -> void {
        if (!diagnostics || diagnostics->getBufferSize() == 0) {
            return;
        }

        const std::string message(
            static_cast<const char*>(diagnostics->getBufferPointer()),
            diagnostics->getBufferSize()
        );
        log::println(log::level::error, log::category::assets, "{}", message);
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
        const auto base_bt = static_cast<SlangBindingType>(bt_bits & slang_binding_type_base_mask);
        const bool is_mutable = (bt_bits & slang_binding_type_mutable_flag) != 0;

        slang::TypeLayoutReflection* leaf_layout = tl->getBindingRangeLeafTypeLayout(range_index);
        slang::TypeReflection* leaf_type = leaf_layout ? leaf_layout->getType() : nullptr;
        const auto access = leaf_type ? leaf_type->getResourceAccess() : slang_resource_access_read;
        const auto shape = leaf_type ? static_cast<SlangResourceShape>(leaf_type->getResourceShape() & slang_resource_base_shape_mask) : static_cast<SlangResourceShape>(0);

        switch (base_bt) {
            case slang_binding_type_combined_texture_sampler: return vk::DescriptorType::eCombinedImageSampler;
            case slang_binding_type_texture:
                if (shape == slang_texture_buffer) {
                    return (is_mutable || access != slang_resource_access_read) ? vk::DescriptorType::eStorageTexelBuffer : vk::DescriptorType::eUniformTexelBuffer;
                }
                return (is_mutable || access != slang_resource_access_read) ? vk::DescriptorType::eStorageImage : vk::DescriptorType::eSampledImage;
            case slang_binding_type_sampler: return vk::DescriptorType::eSampler;
            case slang_binding_type_typed_buffer:
                return (is_mutable || access != slang_resource_access_read) ? vk::DescriptorType::eStorageTexelBuffer : vk::DescriptorType::eUniformTexelBuffer;
            case slang_binding_type_raw_buffer: return vk::DescriptorType::eStorageBuffer;
            case slang_binding_type_constant_buffer:
            case slang_binding_type_parameter_block: return vk::DescriptorType::eUniformBuffer;
            case slang_binding_type_input_render_target: return vk::DescriptorType::eSampledImage;
            case slang_binding_type_ray_tracing_acceleration_structure: return vk::DescriptorType::eAccelerationStructureKHR;
            default: return vk::DescriptorType::eStorageBuffer;
        }
    }
}

template<>
struct gse::asset_compiler<gse::shader_layout> {
    static auto source_extensions() -> std::vector<std::string> {
        return { ".slang" };
    }

    static auto baked_extension() -> std::string {
        return ".glayout";
    }

    static auto source_directory() -> std::string {
        return "Shaders/Layouts";
    }

    static auto baked_directory() -> std::string {
        return "Layouts";
    }

    static auto compile_one(
        const std::filesystem::path& source,
        const std::filesystem::path& destination
    ) -> bool {
        using namespace layout_compile;

        const std::string layout_name = source.stem().string();
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
            log::println(log::level::error, log::category::assets, "Failed to load layout module: {}", layout_name);
            return false;
        }

        slang::IComponentType* parts[] = { mod.get() };
        Slang::ComPtr<slang::IComponentType> composed;
        if (slang_failed(session->createCompositeComponentType(parts, 1, composed.writeRef(), diags.writeRef()))) {
            return false;
        }

        Slang::ComPtr<slang::IComponentType> program;
        if (slang_failed(composed->link(program.writeRef(), diags.writeRef()))) {
            log_diagnostics(diags.get());
            return false;
        }

        slang::ProgramLayout* layout = program->getLayout(0, diags.writeRef());
        if (!layout) {
            return false;
        }

        std::map<std::uint32_t, shader_layout_set> sets_map;

        auto* globals_vl = layout->getGlobalParamsVarLayout();
        if (!globals_vl) {
            log::println(log::level::warning, log::category::assets, "Layout {} has no global parameters", layout_name);
            return false;
        }

        auto* globals_tl = globals_vl->getTypeLayout();
        if (!globals_tl) {
            return false;
        }

        slang::TypeLayoutReflection* element_tl = nullptr;
        std::uint32_t container_space = 0;

        switch (globals_tl->getKind()) {
            case slang::TypeReflection::Kind::Struct:
                element_tl = globals_tl;
                break;
            case slang::TypeReflection::Kind::ConstantBuffer:
            case slang::TypeReflection::Kind::ParameterBlock:
                if (auto* container_vl = globals_tl->getContainerVarLayout()) {
                    container_space = static_cast<std::uint32_t>(container_vl->getBindingSpace(slang::ParameterCategory::DescriptorTableSlot));
                }
                if (auto* element_vl = globals_tl->getElementVarLayout()) {
                    element_tl = element_vl->getTypeLayout();
                }
                break;
            default: break;
        }

        if (!element_tl || element_tl->getKind() != slang::TypeReflection::Kind::Struct) {
            log::println(log::level::warning, log::category::assets, "Layout {} has unexpected structure", layout_name);
            return false;
        }

        const int field_count = element_tl->getFieldCount();
        vk::ShaderStageFlags all_stages = vk::ShaderStageFlagBits::eAll;

        for (int i = 0; i < field_count; ++i) {
            auto* var = element_tl->getFieldByIndex(i);
            auto* tl = var->getTypeLayout();
            const auto binding_range_count = tl->getBindingRangeCount();

            for (int range_index = 0; range_index < binding_range_count; ++range_index) {
                const auto binding = static_cast<std::uint32_t>(var->getOffset(slang::ParameterCategory::DescriptorTableSlot));
                const auto set_idx = container_space + static_cast<std::uint32_t>(var->getBindingSpace(slang::ParameterCategory::DescriptorTableSlot));

                vk::DescriptorSetLayoutBinding layout_binding{
                    .binding = binding,
                    .descriptorType = to_vk_descriptor_type(tl, range_index),
                    .descriptorCount = tl->getKind() == slang::TypeReflection::Kind::Array
                        ? static_cast<std::uint32_t>(tl->getElementCount())
                        : 1u,
                    .stageFlags = all_stages,
                };

                auto& set = sets_map[set_idx];
                set.set_index = set_idx;
                const bool exists = std::ranges::any_of(set.bindings, [&](const shader_layout_binding& b) {
                    return b.layout_binding.binding == binding;
                });

                if (!exists) {
                    set.bindings.push_back({
                        .name = var->getName(),
                        .layout_binding = layout_binding,
                    });
                }
            }
        }

        std::filesystem::create_directories(destination.parent_path());
        std::ofstream out(destination, std::ios::binary);
        if (!out.is_open()) {
            return false;
        }

        std::vector<shader_layout_set> sets;
        sets.reserve(sets_map.size());
        for (auto& set : std::views::values(sets_map)) {
            sets.push_back(std::move(set));
        }

        binary_writer ar(out, 0x474C4159, 1);
        ar & layout_name & sets;

        log::println(log::category::assets, "Layout compiled: {}", destination.filename().string());
        return true;
    }

    static auto needs_recompile(
        const std::filesystem::path& source,
        const std::filesystem::path& destination
    ) -> bool {
        if (!std::filesystem::exists(destination)) {
            return true;
        }
        return std::filesystem::last_write_time(source) > std::filesystem::last_write_time(destination);
    }
    
    static auto dependencies(
        const std::filesystem::path&
    ) -> std::vector<std::filesystem::path> {
        std::vector<std::filesystem::path> deps;
        if (const auto common_file = config::resource_path / "Shaders" / "common.slang"; std::filesystem::exists(common_file)) {
            deps.push_back(common_file);
        }
        return deps;
    }
};
