export module gse.vulkan:shader_object;

import std;
import vulkan;

import gse.gpu_backend;
import :types;

import gse.core;

export namespace gse::vulkan {
	struct specialization_entry {
		std::uint32_t constant_id = 0;
		std::uint32_t offset = 0;
		std::uint32_t size = 0;
	};

	struct shader_object_create_info {
		gpu::stage_flag stage = gpu::stage_flag::vertex;
		std::span<const std::uint32_t> spirv;
		std::string_view entry_point = "main";
		gpu::stage_flags next_stage = {};
		std::optional<std::uint32_t> required_subgroup_size;
		bool require_full_subgroups = false;
		std::span<const vk::DescriptorSetAndBindingMappingEXT> bindless_mappings;
		std::span<const specialization_entry> spec_entries;
		std::span<const std::byte> spec_data;
	};

	struct shader_spec_scratch {
		std::vector<vk::SpecializationMapEntry> entries;
		std::optional<vk::SpecializationInfo> info;
	};

	auto build_vk_shader_create_info(
		const shader_object_create_info& info,
		std::optional<vk::ShaderRequiredSubgroupSizeCreateInfoEXT>& subgroup_size_scratch,
		std::optional<vk::ShaderDescriptorSetAndBindingMappingInfoEXT>& mapping_scratch,
		shader_spec_scratch& spec_scratch,
		vk::ShaderCreateFlagsEXT extra_flags
	) -> vk::ShaderCreateInfoEXT;
}

auto gse::vulkan::build_vk_shader_create_info(const shader_object_create_info& info, std::optional<vk::ShaderRequiredSubgroupSizeCreateInfoEXT>& subgroup_size_scratch, std::optional<vk::ShaderDescriptorSetAndBindingMappingInfoEXT>& mapping_scratch, shader_spec_scratch& spec_scratch, const vk::ShaderCreateFlagsEXT extra_flags) -> vk::ShaderCreateInfoEXT {
	auto flags = extra_flags | vk::ShaderCreateFlagBitsEXT::eDescriptorHeap;
	if (info.require_full_subgroups) {
		flags |= vk::ShaderCreateFlagBitsEXT::eRequireFullSubgroups;
	}

	subgroup_size_scratch.reset();
	mapping_scratch.reset();
	const void* pnext_head = nullptr;
	if (info.required_subgroup_size.has_value()) {
		subgroup_size_scratch = vk::ShaderRequiredSubgroupSizeCreateInfoEXT{
			.requiredSubgroupSize = *info.required_subgroup_size,
		};
		pnext_head = &*subgroup_size_scratch;
	}
	if (!info.bindless_mappings.empty()) {
		mapping_scratch = vk::ShaderDescriptorSetAndBindingMappingInfoEXT{
			.pNext = pnext_head,
			.mappingCount = static_cast<std::uint32_t>(info.bindless_mappings.size()),
			.pMappings = info.bindless_mappings.data(),
		};
		pnext_head = &*mapping_scratch;
	}

	spec_scratch.entries.clear();
	spec_scratch.info.reset();
	if (!info.spec_entries.empty()) {
		spec_scratch.entries.reserve(info.spec_entries.size());
		for (const auto& e : info.spec_entries) {
			spec_scratch.entries.push_back(vk::SpecializationMapEntry{
				.constantID = e.constant_id,
				.offset = e.offset,
				.size = e.size,
			});
		}
		spec_scratch.info = vk::SpecializationInfo{
			.mapEntryCount = static_cast<std::uint32_t>(spec_scratch.entries.size()),
			.pMapEntries = spec_scratch.entries.data(),
			.dataSize = info.spec_data.size(),
			.pData = info.spec_data.data(),
		};
	}

	return vk::ShaderCreateInfoEXT{
		.pNext = pnext_head,
		.flags = flags,
		.stage = to_vk(info.stage),
		.nextStage = to_vk(info.next_stage),
		.codeType = vk::ShaderCodeTypeEXT::eSpirv,
		.codeSize = info.spirv.size_bytes(),
		.pCode = info.spirv.data(),
		.pName = info.entry_point.data(),
		.pSpecializationInfo = spec_scratch.info.has_value() ? &*spec_scratch.info : nullptr,
	};
}
