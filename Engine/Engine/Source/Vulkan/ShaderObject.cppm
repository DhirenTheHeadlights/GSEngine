export module gse.vulkan:shader_object;

import std;
import vulkan;

import :handles;
import :types;
import :device;

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

	class shader_object final : public non_copyable {
	public:
		shader_object() {}
		~shader_object() = default;

		shader_object(
			shader_object&&
		) noexcept = default;

		auto operator=(
			shader_object&&
		) noexcept -> shader_object& = default;

		[[nodiscard]]
		static auto create(
			const device& dev,
			const shader_object_create_info& info
		) -> shader_object;

		[[nodiscard]]
		static auto create_linked(
			const device& dev,
			std::span<const shader_object_create_info> infos
		) -> std::vector<shader_object>;

		[[nodiscard]] auto handle() const -> gpu::shader_object_handle;

		[[nodiscard]] auto stage() const -> gpu::stage_flag;

		[[nodiscard]] auto valid() const -> bool;

	private:
		shader_object(
			vk::raii::ShaderEXT&& shader,
			gpu::stage_flag stage
		);

		vk::raii::ShaderEXT m_shader = nullptr;
		gpu::stage_flag m_stage = gpu::stage_flag::vertex;
	};
}

namespace gse::vulkan {
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

gse::vulkan::shader_object::shader_object(vk::raii::ShaderEXT&& shader, const gpu::stage_flag stage)
	: m_shader(std::move(shader)), m_stage(stage) {
}

auto gse::vulkan::shader_object::create(const device& dev, const shader_object_create_info& info) -> shader_object {
	std::optional<vk::ShaderRequiredSubgroupSizeCreateInfoEXT> subgroup_size_scratch;
	std::optional<vk::ShaderDescriptorSetAndBindingMappingInfoEXT> mapping_scratch;
	shader_spec_scratch spec_scratch;
	const auto vk_info = build_vk_shader_create_info(
		info,
		subgroup_size_scratch,
		mapping_scratch,
		spec_scratch,
		{}
	);

	auto shaders = dev.raii_device().createShadersEXT(vk_info);
	return shader_object(std::move(shaders[0]), info.stage);
}

auto gse::vulkan::shader_object::create_linked(const device& dev, const std::span<const shader_object_create_info> infos) -> std::vector<shader_object> {
	std::vector<std::optional<vk::ShaderRequiredSubgroupSizeCreateInfoEXT>> subgroup_size_scratch(infos.size());
	std::vector<std::optional<vk::ShaderDescriptorSetAndBindingMappingInfoEXT>> mapping_scratch(infos.size());
	std::vector<shader_spec_scratch> spec_scratch(infos.size());
	std::vector<vk::ShaderCreateInfoEXT> vk_infos;
	vk_infos.reserve(infos.size());
	for (std::size_t i = 0; i < infos.size(); ++i) {
		vk_infos.push_back(
			build_vk_shader_create_info(
				infos[i],
				subgroup_size_scratch[i],
				mapping_scratch[i],
				spec_scratch[i],
				vk::ShaderCreateFlagBitsEXT::eLinkStage
			)
		);
	}

	auto shaders = dev.raii_device().createShadersEXT(vk_infos);
	std::vector<shader_object> result;
	result.reserve(shaders.size());
	for (std::size_t i = 0; i < shaders.size(); ++i) {
		result.emplace_back(shader_object(std::move(shaders[i]), infos[i].stage));
	}
	return result;
}

auto gse::vulkan::shader_object::handle() const -> gpu::shader_object_handle {
	return std::bit_cast<gpu::shader_object_handle>(*m_shader);
}

auto gse::vulkan::shader_object::stage() const -> gpu::stage_flag {
	return m_stage;
}

auto gse::vulkan::shader_object::valid() const -> bool {
	return *m_shader != nullptr;
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
