module;

#include <cstdio>
#include <slang-com-ptr.h>
#include <slang.h>

#undef assert

export module gse.platform:shader_layout_compiler;

import std;

import gse.platform.vulkan;
import gse.assert;

import :asset_compiler;
import :shader_layout;

namespace gse::layout_compile {
    auto write_string(std::ofstream& stream, const std::string& str) -> void {
        const auto size = static_cast<std::uint32_t>(str.size());
        stream.write(reinterpret_cast<const char*>(&size), sizeof(size));
        stream.write(str.c_str(), size);
    }

    template<typename T>
    auto write_data(std::ofstream& stream, const T& value) -> void {
        stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
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
}

export template<>
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
            std::println("Failed to load layout module: {}", layout_name);
            return false;
        }

        slang::IComponentType* parts[] = { mod.get() };
        Slang::ComPtr<slang::IComponentType> composed;
        if (SLANG_FAILED(session->createCompositeComponentType(parts, 1, composed.writeRef(), diags.writeRef()))) {
            return false;
        }

        Slang::ComPtr<slang::IComponentType> program;
        if (SLANG_FAILED(composed->link(program.writeRef(), diags.writeRef()))) {
            if (diags && diags->getBufferSize()) {
                std::fprintf(stderr, "%s", static_cast<const char*>(diags->getBufferPointer()));
            }
            return false;
        }

        slang::ProgramLayout* layout = program->getLayout(0, diags.writeRef());
        if (!layout) {
            return false;
        }

        std::map<std::uint32_t, std::vector<shader_layout_binding>> sets_map;

        auto* globals_vl = layout->getGlobalParamsVarLayout();
        if (!globals_vl) {
            std::println("Layout {} has no global parameters", layout_name);
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
            std::println("Layout {} has unexpected structure", layout_name);
            return false;
        }

        const int field_count = element_tl->getFieldCount();
        vk::ShaderStageFlags all_stages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

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

                auto& set_bindings = sets_map[set_idx];
                bool exists = std::ranges::any_of(set_bindings, [&](const shader_layout_binding& b) {
                    return b.layout_binding.binding == binding;
                });

                if (!exists) {
                    set_bindings.push_back({
                        .name = var->getName(),
                        .layout_binding = layout_binding
                    });
                }
            }
        }

        std::filesystem::create_directories(destination.parent_path());
        std::ofstream out(destination, std::ios::binary);
        if (!out.is_open()) {
            return false;
        }

        constexpr std::uint32_t magic = 0x474C4159;
        constexpr std::uint32_t version = 1;
        write_data(out, magic);
        write_data(out, version);
        write_string(out, layout_name);

        const auto set_count = static_cast<std::uint32_t>(sets_map.size());
        write_data(out, set_count);

        for (const auto& [set_idx, bindings] : sets_map) {
            write_data(out, set_idx);
            const auto binding_count = static_cast<std::uint32_t>(bindings.size());
            write_data(out, binding_count);

            for (const auto& b : bindings) {
                write_string(out, b.name);
                write_data(out, b.layout_binding);
            }
        }

        std::println("Layout compiled: {}", destination.filename().string());
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
