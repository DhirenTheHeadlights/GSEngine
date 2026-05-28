export module gse.vulkan:pipeline_layout;

import std;
import vulkan;

import :handles;
import :types;
import :device;

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

		[[nodiscard]]
		static auto create(
			const device& dev,
			std::span<const gpu::push_constant_range> push_ranges
		) -> pipeline_layout;

		[[nodiscard]] auto handle(
			this const pipeline_layout& self
		) -> gpu::handle<pipeline_layout>;

		[[nodiscard]] auto valid() const -> bool;

	private:
		explicit pipeline_layout(
			vk::raii::PipelineLayout&& layout
		);

		vk::raii::PipelineLayout m_layout = nullptr;
	};
}

gse::vulkan::pipeline_layout::pipeline_layout(vk::raii::PipelineLayout&& layout) : m_layout(std::move(layout)) {
}

auto gse::vulkan::pipeline_layout::create(const device& dev, const std::span<const gpu::push_constant_range> push_ranges) -> pipeline_layout {
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
		.pushConstantRangeCount = static_cast<std::uint32_t>(vk_ranges.size()),
		.pPushConstantRanges = vk_ranges.data(),
	};
	return pipeline_layout(dev.raii_device().createPipelineLayout(info));
}

auto gse::vulkan::pipeline_layout::handle(this const pipeline_layout& self) -> gpu::handle<pipeline_layout> {
	return std::bit_cast<gpu::handle<pipeline_layout>>(*self.m_layout);
}

auto gse::vulkan::pipeline_layout::valid() const -> bool {
	return *m_layout != nullptr;
}
