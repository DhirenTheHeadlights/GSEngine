export module gse.platform:gpu_frame;

import std;

import :vulkan_runtime;
import :gpu_device;
import :gpu_swapchain;
import :window;

import gse.assert;
import gse.log;
import gse.utility;

export namespace gse::gpu {
	enum class frame_status : std::uint8_t {
		minimized,
		swapchain_out_of_date,
		device_lost
	};

	struct frame_token {
		std::uint32_t frame_index = 0;
		std::uint32_t image_index = 0;
	};

	class frame final : public non_copyable {
	public:
		[[nodiscard]] static auto create(
			device& dev,
			swap_chain& sc
		) -> std::unique_ptr<frame>;

		frame(vulkan::sync_config&& sync, vulkan::frame_context_config&& frame_ctx, device& dev, swap_chain& sc);

		[[nodiscard]] auto current_frame(
		) const -> std::uint32_t;

		[[nodiscard]] auto image_index(
		) const -> std::uint32_t;

		[[nodiscard]] auto command_buffer(
		) const -> vk::CommandBuffer;

		[[nodiscard]] auto frame_in_progress(
		) const -> bool;

		auto begin(
			window& win
		) -> std::expected<frame_token, frame_status>;

		auto end(
			window& win
		) -> void;

		auto set_sync(
			vulkan::sync_config&& sync
		) -> void;

	private:
		auto recreate_resources(
			window& win
		) -> void;

		vulkan::sync_config m_sync;
		vulkan::frame_context_config m_frame_context;
		std::uint32_t m_current_frame = 0;
		bool m_frame_in_progress = false;
		device* m_device;
		swap_chain* m_swapchain;
	};
}

namespace gse::vulkan {
	auto create_sync_objects(const device_config& device_data, const swap_chain_config& swap_chain_data) -> sync_config;
}

auto gse::gpu::frame::create(device& dev, swap_chain& sc) -> std::unique_ptr<frame> {
	auto sync = create_sync_objects(dev.device_config(), sc.config());
	vulkan::frame_context_config frame_ctx(0, *dev.command_config().buffers[0]);
	return std::make_unique<frame>(std::move(sync), std::move(frame_ctx), dev, sc);
}

gse::gpu::frame::frame(vulkan::sync_config&& sync, vulkan::frame_context_config&& frame_ctx, device& dev, swap_chain& sc)
	: m_sync(std::move(sync)), m_frame_context(std::move(frame_ctx)), m_device(&dev), m_swapchain(&sc) {}

auto gse::gpu::frame::current_frame() const -> std::uint32_t {
	return m_current_frame;
}

auto gse::gpu::frame::image_index() const -> std::uint32_t {
	return m_frame_context.image_index;
}

auto gse::gpu::frame::command_buffer() const -> vk::CommandBuffer {
	return m_frame_context.command_buffer;
}

auto gse::gpu::frame::frame_in_progress() const -> bool {
	return m_frame_in_progress;
}

auto gse::gpu::frame::set_sync(vulkan::sync_config&& sync) -> void {
	m_sync = std::move(sync);
}

auto gse::gpu::frame::recreate_resources(window& win) -> void {
	m_device->wait_idle();
	m_swapchain->recreate(win.viewport());
	m_sync = create_sync_objects(m_device->device_config(), m_swapchain->config());
	m_swapchain->notify_recreated();
	m_device->wait_idle();
}

auto gse::gpu::frame::begin(window& win) -> std::expected<frame_token, frame_status> {
	const auto& device = m_device->device_config().device;

	m_frame_in_progress = false;

	if (win.minimized()) {
		return std::unexpected(frame_status::minimized);
	}

	try {
		const auto fence_result = device.waitForFences(
			*m_sync.in_flight_fences[m_current_frame],
			vk::True,
			std::numeric_limits<std::uint64_t>::max()
		);
		assert(fence_result == vk::Result::eSuccess, std::source_location::current(), "Failed to wait for in-flight fence!");
	} catch (const vk::DeviceLostError&) {
		m_device->report_device_lost(std::format("begin_frame waitForFences (frame {})", m_current_frame));
		return std::unexpected(frame_status::device_lost);
	}

	try {
		m_device->cleanup_finished_frame_resources(m_current_frame);
	} catch (const vk::DeviceLostError&) {
		m_device->report_device_lost(std::format("cleanup_finished_frame_resources (frame {})", m_current_frame));
		return std::unexpected(frame_status::device_lost);
	}

	if (win.frame_buffer_resized()) {
		recreate_resources(win);
		return std::unexpected(frame_status::swapchain_out_of_date);
	}

	vk::Result result;
	std::uint32_t acquired_image_index = 0;
	try {
		const vk::AcquireNextImageInfoKHR acquire_info{
			.swapchain = *m_swapchain->config().swap_chain,
			.timeout = std::numeric_limits<std::uint64_t>::max(),
			.semaphore = *m_sync.image_available_semaphores[m_current_frame],
			.fence = nullptr,
			.deviceMask = 1
		};
		std::tie(result, acquired_image_index) = device.acquireNextImage2KHR(acquire_info);
	} catch (const vk::OutOfDateKHRError&) {
		result = vk::Result::eErrorOutOfDateKHR;
	} catch (const vk::DeviceLostError&) {
		m_device->report_device_lost(std::format("acquireNextImage2KHR (frame {})", m_current_frame));
		return std::unexpected(frame_status::device_lost);
	}

	if (result == vk::Result::eErrorOutOfDateKHR) {
		recreate_resources(win);
		return std::unexpected(frame_status::swapchain_out_of_date);
	}

	assert(result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR,
		std::source_location::current(),
		"Failed to acquire swap chain image!");

	device.resetFences(*m_sync.in_flight_fences[m_current_frame]);

	m_frame_context = { acquired_image_index, *m_device->command_config().buffers[m_current_frame] };
	m_frame_context.command_buffer.reset({});

	constexpr vk::CommandBufferBeginInfo cmd_begin_info{
		.flags = {},
		.pInheritanceInfo = nullptr
	};
	m_frame_context.command_buffer.begin(cmd_begin_info);

	m_frame_in_progress = true;
	m_device->set_current_transient_frame(m_current_frame);

	return frame_token{
		.frame_index = m_current_frame,
		.image_index = acquired_image_index
	};
}

auto gse::gpu::frame::end(window& win) -> void {
	const auto& device = m_device->device_config().device;

	m_frame_context.command_buffer.end();

	const vk::SemaphoreSubmitInfo wait_info{
		.semaphore = *m_sync.image_available_semaphores[m_current_frame],
		.value = 0,
		.stageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
		.deviceIndex = 0
	};

	const vk::CommandBufferSubmitInfo cmd_info{
		.commandBuffer = m_frame_context.command_buffer,
		.deviceMask = 1
	};

	const vk::SemaphoreSubmitInfo signal_info{
		.semaphore = *m_sync.render_finished_semaphores[m_frame_context.image_index],
		.value = 0,
		.stageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
		.deviceIndex = 0
	};

	const vk::SubmitInfo2 submit_info2{
		.flags = {},
		.waitSemaphoreInfoCount = 1,
		.pWaitSemaphoreInfos = &wait_info,
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &cmd_info,
		.signalSemaphoreInfoCount = 1,
		.pSignalSemaphoreInfos = &signal_info
	};

	try {
		m_device->queue_config().graphics.submit2(
			submit_info2,
			*m_sync.in_flight_fences[m_current_frame]
		);
	} catch (const vk::DeviceLostError&) {
		m_device->report_device_lost(std::format("submit2 (frame {}, image {})", m_current_frame, m_frame_context.image_index));
		throw;
	}

	const vk::Semaphore render_finished_handle = *m_sync.render_finished_semaphores[m_frame_context.image_index];

	const vk::PresentInfoKHR present_info{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &render_finished_handle,
		.swapchainCount = 1,
		.pSwapchains = &*m_swapchain->config().swap_chain,
		.pImageIndices = &m_frame_context.image_index
	};

	vk::Result present_result;
	try {
		present_result = m_device->queue_config().present.presentKHR(present_info);
	}
	catch (const vk::OutOfDateKHRError&) {
		present_result = vk::Result::eErrorOutOfDateKHR;
	}
	catch (const vk::DeviceLostError&) {
		m_device->report_device_lost(std::format("presentKHR (frame {}, image {})", m_current_frame, m_frame_context.image_index));
		throw;
	}

	if (present_result == vk::Result::eErrorOutOfDateKHR || present_result == vk::Result::eSuboptimalKHR) {
		recreate_resources(win);
	}
	else {
		assert(
			present_result == vk::Result::eSuccess,
			std::source_location::current(),
			"Failed to present swap chain image!"
		);
	}

	m_current_frame = (m_current_frame + 1) % vulkan::max_frames_in_flight;
	m_frame_in_progress = false;
}

namespace gse::vulkan {
	auto create_sync_objects(const device_config& device_data, const swap_chain_config& swap_chain_data) -> sync_config {
		std::vector<vk::raii::Semaphore> image_available;
		std::vector<vk::raii::Semaphore> render_finished;
		std::vector<vk::raii::Fence> in_flight_fences;

		const auto swap_chain_image_count = swap_chain_data.images.size();
		image_available.reserve(swap_chain_image_count);
		render_finished.reserve(swap_chain_image_count);

		in_flight_fences.reserve(max_frames_in_flight);

		constexpr vk::FenceCreateInfo fence_ci{
			.flags = vk::FenceCreateFlagBits::eSignaled
		};

		for (std::size_t i = 0; i < swap_chain_image_count; ++i) {
			constexpr vk::SemaphoreCreateInfo bin_sem_ci{};
			image_available.emplace_back(device_data.device, bin_sem_ci);
			render_finished.emplace_back(device_data.device, bin_sem_ci);
		}

		for (std::size_t i = 0; i < max_frames_in_flight; ++i) {
			in_flight_fences.emplace_back(device_data.device, fence_ci);
		}

		return sync_config(
			std::move(image_available),
			std::move(render_finished),
			std::move(in_flight_fences)
		);
	}
}
