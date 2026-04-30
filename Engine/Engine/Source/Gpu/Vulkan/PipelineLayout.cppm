export module gse.gpu:vulkan_pipeline_layout;

import std;
import vulkan;

import :handles;
import :types;
import :vulkan_device;

import gse.core;

export namespace gse::vulkan {
	class pipeline_layout final : public non_copyable {
	public:
		pipeline_layout() = default;

		~pipeline_layout() override = default;

		pipeline_layout(
			pipeline_layout&&
		) noexcept = default;

		auto operator=(
			pipeline_layout&&
		) noexcept -> pipeline_layout& = default;

		[[nodiscard]] static auto create(
			const device& dev,
			std::span<const gpu::handle<descriptor_set_layout>> set_layouts,
			std::span<const gpu::push_constant_range> push_ranges
		) -> pipeline_layout;

		[[nodiscard]] auto handle(
			this const pipeline_layout& self
		) -> gpu::handle<pipeline_layout>;

		explicit operator bool(
		) const;

	private:
		explicit pipeline_layout(
			vk::raii::PipelineLayout&& layout
		);

		vk::raii::PipelineLayout m_layout = nullptr;
	};
}

gse::vulkan::pipeline_layout::pipeline_layout(vk::raii::PipelineLayout&& layout) : m_layout(std::move(layout)) {}

auto gse::vulkan::pipeline_layout::create(const device& dev, const std::span<const gpu::handle<descriptor_set_layout>> set_layouts, const std::span<const gpu::push_constant_range> push_ranges) -> pipeline_layout {
	std::vector<vk::DescriptorSetLayout> vk_layouts;
	vk_layouts.reserve(set_layouts.size());
	for (const auto h : set_layouts) {
		vk_layouts.push_back(std::bit_cast<vk::DescriptorSetLayout>(h));
	}

	std::vector<vk::PushConstantRange> vk_ranges;
	vk_ranges.reserve(push_ranges.size());
	for (const auto& r : push_ranges) {
		vk_ranges.push_back({
			.stageFlags = to_vk(r.stages),
			.offset = r.offset,
			.size = r.size,
		});
	}

	const vk::PipelineLayoutCreateInfo info{
		.setLayoutCount = static_cast<std::uint32_t>(vk_layouts.size()),
		.pSetLayouts = vk_layouts.data(),
		.pushConstantRangeCount = static_cast<std::uint32_t>(vk_ranges.size()),
		.pPushConstantRanges = vk_ranges.data(),
	};
	return pipeline_layout(dev.raii_device().createPipelineLayout(info));
}

auto gse::vulkan::pipeline_layout::handle(this const pipeline_layout& self) -> gpu::handle<pipeline_layout> {
	return std::bit_cast<gpu::handle<pipeline_layout>>(*self.m_layout);
}

gse::vulkan::pipeline_layout::operator bool() const {
	return *m_layout != nullptr;
}
