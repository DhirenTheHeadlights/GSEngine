export module gse.platform:gpu_device;

import std;

import :vulkan_runtime;
import :vulkan_allocator;
import :descriptor_heap;
import :gpu_types;
import :gpu_image;

import gse.utility;

export namespace gse::gpu {
	class device final : public non_copyable {
	public:
		explicit device(vulkan::runtime& runtime);

		[[nodiscard]] auto logical_device(
			this auto&& self
		) -> decltype(auto);

		[[nodiscard]] auto physical_device(
		) -> const vk::raii::PhysicalDevice&;

		[[nodiscard]] auto allocator(
		) -> vulkan::allocator&;

		[[nodiscard]] auto allocator(
		) const -> const vulkan::allocator&;

		[[nodiscard]] auto descriptor_heap(
			this auto& self
		) -> decltype(auto);

		[[nodiscard]] auto surface_format(
		) const -> vk::Format;

		[[nodiscard]] auto compute_queue_family(
		) const -> std::uint32_t;

		[[nodiscard]] auto compute_queue(
		) -> const vk::raii::Queue&;

		auto add_transient_work(
			const auto& commands
		) -> void;

		auto transition_image_layout(
			const image& img,
			image_layout target
		) -> void;

		auto wait_idle(
		) -> void;

		auto report_device_lost(
			std::string_view operation
		) const -> void;

		[[nodiscard]] auto runtime_ref(
		) const -> vulkan::runtime&;
	private:
		vulkan::runtime* m_runtime;
	};

	auto transition_image_layout(device& dev, const image& img, image_layout target) -> void;
}

gse::gpu::device::device(vulkan::runtime& runtime)
	: m_runtime(&runtime) {}

auto gse::gpu::device::logical_device(this auto&& self) -> decltype(auto) {
	using self_t = std::remove_reference_t<decltype(self)>;
	if constexpr (std::is_const_v<self_t>) {
		return static_cast<const vk::raii::Device&>(self.m_runtime->device_config().device);
	} else {
		return static_cast<vk::raii::Device&>(self.m_runtime->device_config().device);
	}
}

auto gse::gpu::device::physical_device() -> const vk::raii::PhysicalDevice& {
	return m_runtime->device_config().physical_device;
}

auto gse::gpu::device::allocator() -> vulkan::allocator& {
	return m_runtime->allocator();
}

auto gse::gpu::device::allocator() const -> const vulkan::allocator& {
	return m_runtime->allocator();
}

auto gse::gpu::device::descriptor_heap(this auto& self) -> decltype(auto) {
	return self.m_runtime->descriptor_heap();
}

auto gse::gpu::device::surface_format() const -> vk::Format {
	return m_runtime->swap_chain_config().surface_format.format;
}

auto gse::gpu::device::compute_queue_family() const -> std::uint32_t {
	return m_runtime->queue_config().compute_family_index;
}

auto gse::gpu::device::compute_queue() -> const vk::raii::Queue& {
	return m_runtime->queue_config().compute;
}

auto gse::gpu::device::add_transient_work(const auto& commands) -> void {
	m_runtime->add_transient_work(commands);
}

auto gse::gpu::device::wait_idle() -> void {
	m_runtime->device_config().device.waitIdle();
}

auto gse::gpu::device::report_device_lost(const std::string_view operation) const -> void {
	m_runtime->report_device_lost(operation);
}

auto gse::gpu::device::runtime_ref() const -> vulkan::runtime& {
	return *m_runtime;
}

auto gse::gpu::transition_image_layout(device& dev, const image& img, image_layout target) -> void {
	const bool is_depth = img.format() == image_format::d32_sfloat;
	const auto aspect = is_depth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;
	const auto target_vk = [&] -> vk::ImageLayout {
		switch (target) {
			case image_layout::general:          return vk::ImageLayout::eGeneral;
			case image_layout::shader_read_only: return vk::ImageLayout::eShaderReadOnlyOptimal;
			default:                             return vk::ImageLayout::eUndefined;
		}
	}();
	const auto img_handle = img.native().image;
	const auto dst_stage = is_depth
		? (vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests)
		: vk::PipelineStageFlagBits2::eAllCommands;
	const auto dst_access = is_depth
		? (vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead)
		: vk::AccessFlagBits2::eShaderRead;

	dev.add_transient_work([img_handle, target_vk, aspect, dst_stage, dst_access](const auto& cmd) {
		const vk::ImageMemoryBarrier2 barrier{
			.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
			.srcAccessMask = {},
			.dstStageMask = dst_stage,
			.dstAccessMask = dst_access,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = target_vk,
			.image = img_handle,
			.subresourceRange = {
				.aspectMask = aspect,
				.baseMipLevel = 0, .levelCount = 1,
				.baseArrayLayer = 0, .layerCount = 1
			}
		};
		const vk::DependencyInfo dep{ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier };
		cmd.pipelineBarrier2(dep);
		return std::vector<vulkan::buffer_resource>{};
	});

	const_cast<image&>(img).set_layout(target);
}

auto gse::gpu::device::transition_image_layout(const image& img, image_layout target) -> void {
	gpu::transition_image_layout(*this, img, target);
}
