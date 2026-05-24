export module gse.gpu:vulkan_shader_object;

import std;
import vulkan;

import :handles;
import :types;
import :vulkan_descriptor_set_layout;
import :vulkan_device;

import gse.core;

export namespace gse::vulkan {
	struct shader_object_create_info {
		gpu::stage_flag stage = gpu::stage_flag::vertex;
		std::span<const std::uint32_t> spirv;
		std::string_view entry_point = "main";
		std::span<const gpu::handle<descriptor_set_layout>> set_layouts;
		std::optional<gpu::push_constant_range> push_constant_range;
		gpu::stage_flags next_stage = {};
		std::optional<std::uint32_t> required_subgroup_size;
		bool require_full_subgroups = false;
		std::span<const vk::DescriptorSetAndBindingMappingEXT> bindless_mappings;
	};

	class shader_object final : public non_copyable {
	public:
		shader_object() = default;

		~shader_object() override = default;

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

		[[nodiscard]] auto handle() const -> gpu::handle<shader_object>;

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
	auto build_vk_shader_create_info(
		const shader_object_create_info& info,
		std::vector<vk::DescriptorSetLayout>& set_layouts_scratch,
		std::vector<vk::PushConstantRange>& push_constants_scratch,
		std::optional<vk::ShaderRequiredSubgroupSizeCreateInfoEXT>& subgroup_size_scratch,
		std::optional<vk::ShaderDescriptorSetAndBindingMappingInfoEXT>& mapping_scratch,
		vk::ShaderCreateFlagsEXT extra_flags
	) -> vk::ShaderCreateInfoEXT;
}

gse::vulkan::shader_object::shader_object(vk::raii::ShaderEXT&& shader, const gpu::stage_flag stage)
	: m_shader(std::move(shader)), m_stage(stage) {
}

auto gse::vulkan::shader_object::create(const device& dev, const shader_object_create_info& info) -> shader_object {
	std::vector<vk::DescriptorSetLayout> set_layouts_scratch;
	std::vector<vk::PushConstantRange> push_constants_scratch;
	std::optional<vk::ShaderRequiredSubgroupSizeCreateInfoEXT> subgroup_size_scratch;
	std::optional<vk::ShaderDescriptorSetAndBindingMappingInfoEXT> mapping_scratch;
	const auto vk_info = build_vk_shader_create_info(
		info,
		set_layouts_scratch,
		push_constants_scratch,
		subgroup_size_scratch,
		mapping_scratch,
		{}
	);

	auto shaders = dev.raii_device().createShadersEXT(vk_info);
	return shader_object(std::move(shaders[0]), info.stage);
}

auto gse::vulkan::shader_object::create_linked(const device& dev, const std::span<const shader_object_create_info> infos) -> std::vector<shader_object> {
	std::vector<std::vector<vk::DescriptorSetLayout>> set_layouts_scratch(infos.size());
	std::vector<std::vector<vk::PushConstantRange>> push_constants_scratch(infos.size());
	std::vector<std::optional<vk::ShaderRequiredSubgroupSizeCreateInfoEXT>> subgroup_size_scratch(infos.size());
	std::vector<std::optional<vk::ShaderDescriptorSetAndBindingMappingInfoEXT>> mapping_scratch(infos.size());
	std::vector<vk::ShaderCreateInfoEXT> vk_infos;
	vk_infos.reserve(infos.size());
	for (std::size_t i = 0; i < infos.size(); ++i) {
		vk_infos.push_back(
			build_vk_shader_create_info(
				infos[i],
				set_layouts_scratch[i],
				push_constants_scratch[i],
				subgroup_size_scratch[i],
				mapping_scratch[i],
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

auto gse::vulkan::shader_object::handle() const -> gpu::handle<shader_object> {
	return std::bit_cast<gpu::handle<shader_object>>(*m_shader);
}

auto gse::vulkan::shader_object::stage() const -> gpu::stage_flag {
	return m_stage;
}

auto gse::vulkan::shader_object::valid() const -> bool {
	return *m_shader != nullptr;
}

auto gse::vulkan::build_vk_shader_create_info(const shader_object_create_info& info, std::vector<vk::DescriptorSetLayout>& set_layouts_scratch, std::vector<vk::PushConstantRange>& push_constants_scratch, std::optional<vk::ShaderRequiredSubgroupSizeCreateInfoEXT>& subgroup_size_scratch, std::optional<vk::ShaderDescriptorSetAndBindingMappingInfoEXT>& mapping_scratch, const vk::ShaderCreateFlagsEXT extra_flags) -> vk::ShaderCreateInfoEXT {
	const bool descriptor_heap_mode = !info.bindless_mappings.empty();

	set_layouts_scratch.clear();
	if (!descriptor_heap_mode) {
		set_layouts_scratch.reserve(info.set_layouts.size());
		for (const auto h : info.set_layouts) {
			set_layouts_scratch.push_back(std::bit_cast<vk::DescriptorSetLayout>(h));
		}
	}

	push_constants_scratch.clear();
	if (!descriptor_heap_mode && info.push_constant_range.has_value()) {
		push_constants_scratch.push_back(to_vk(*info.push_constant_range));
	}

	auto flags = extra_flags;
	if (info.require_full_subgroups) {
		flags |= vk::ShaderCreateFlagBitsEXT::eRequireFullSubgroups;
	}
	if (descriptor_heap_mode) {
		flags |= vk::ShaderCreateFlagBitsEXT::eDescriptorHeap;
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

	return vk::ShaderCreateInfoEXT{
		.pNext = pnext_head,
		.flags = flags,
		.stage = to_vk(info.stage),
		.nextStage = to_vk(info.next_stage),
		.codeType = vk::ShaderCodeTypeEXT::eSpirv,
		.codeSize = info.spirv.size_bytes(),
		.pCode = info.spirv.data(),
		.pName = info.entry_point.data(),
		.setLayoutCount = static_cast<std::uint32_t>(set_layouts_scratch.size()),
		.pSetLayouts = set_layouts_scratch.data(),
		.pushConstantRangeCount = static_cast<std::uint32_t>(push_constants_scratch.size()),
		.pPushConstantRanges = push_constants_scratch.data(),
	};
}
