export module gse.gpu:gpu_frame;

import std;

import :vulkan_runtime;
import :gpu_device;
import :gpu_swapchain;
import :gpu_sync;
import :gpu_types;
import gse.os;

import gse.assert;
import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
export namespace gse::gpu {
	class frame final : public non_copyable {
	public:
		[[nodiscard]] static auto create(
			device& dev,
			swap_chain& sc
		) -> std::unique_ptr<frame>;

		frame(
			vulkan::sync_config&& sync,
			vulkan::frame_context_config&& frame_ctx,
			device& dev,
			swap_chain& sc
		);

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

		auto add_wait_semaphore(const compute_semaphore_state& state
		) -> void;

		auto add_transient_work(
			auto&& commands
		) -> void;

	private:
		auto recreate_resources(const window& win
		) -> void;

		auto cleanup_finished(
			std::uint32_t frame_index
		) -> void;

		static auto create_sync_objects(
			const vulkan::device_config& device_data,
			const vulkan::swap_chain_config& swap_chain_data
		) -> vulkan::sync_config;

		vulkan::sync_config m_sync;
		vulkan::frame_context_config m_frame_context;
		std::uint32_t m_current_frame = 0;
		bool m_frame_in_progress = false;
		device* m_device;
		swap_chain* m_swapchain;
		std::vector<compute_semaphore_state> m_extra_waits;
		std::vector<std::vector<vulkan::transient_gpu_work>> m_graveyard;
	};
}

auto gse::gpu::frame::create(device& dev, swap_chain& sc) -> std::unique_ptr<frame> {
	auto sync = create_sync_objects(dev.device_config(), sc.config());
	vulkan::frame_context_config frame_ctx(0, *dev.command_config().buffers[0]);
	return std::make_unique<frame>(std::move(sync), std::move(frame_ctx), dev, sc);
}

gse::gpu::frame::frame(vulkan::sync_config&& sync, vulkan::frame_context_config&& frame_ctx, device& dev, swap_chain& sc)
	: m_sync(std::move(sync)), m_frame_context(std::move(frame_ctx)), m_device(&dev), m_swapchain(&sc) {
	m_graveyard.resize(vulkan::max_frames_in_flight);
}

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

auto gse::gpu::frame::recreate_resources(const window& win) -> void {
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
		cleanup_finished(m_current_frame);
	} catch (const vk::DeviceLostError&) {
		m_device->report_device_lost(std::format("cleanup_finished (frame {})", m_current_frame));
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

	assert(
		result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR,
		std::source_location::current(),
		"Failed to acquire swap chain image!"
	);

	device.resetFences(*m_sync.in_flight_fences[m_current_frame]);

	m_frame_context = { acquired_image_index, *m_device->command_config().buffers[m_current_frame] };
	m_frame_context.command_buffer.reset({});

	constexpr vk::CommandBufferBeginInfo cmd_begin_info{
		.flags = {},
		.pInheritanceInfo = nullptr
	};
	m_frame_context.command_buffer.begin(cmd_begin_info);

	m_frame_in_progress = true;

	return frame_token{
		.frame_index = m_current_frame,
		.image_index = acquired_image_index
	};
}

auto gse::gpu::frame::add_wait_semaphore(const compute_semaphore_state& state) -> void {
	if (state.semaphore && state.value > 0) {
		m_extra_waits.push_back(state);
	}
}

auto gse::gpu::frame::end(window& win) -> void {
	trace::scope(find_or_generate_id("end_frame::cmd_end"), [&] {
		m_frame_context.command_buffer.end();
	});

	std::vector<vk::SemaphoreSubmitInfo> wait_infos;
	wait_infos.push_back({
		.semaphore = *m_sync.image_available_semaphores[m_current_frame],
		.value = 0,
		.stageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
		.deviceIndex = 0
	});

	for (const auto& [semaphore, value, dst_stage] : m_extra_waits) {
		wait_infos.push_back({
			.semaphore = **semaphore,
			.value = value,
			.stageMask = dst_stage,
			.deviceIndex = 0
		});
	}
	m_extra_waits.clear();

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
		.waitSemaphoreInfoCount = static_cast<std::uint32_t>(wait_infos.size()),
		.pWaitSemaphoreInfos = wait_infos.data(),
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &cmd_info,
		.signalSemaphoreInfoCount = 1,
		.pSignalSemaphoreInfos = &signal_info
	};

	trace::scope(find_or_generate_id("end_frame::submit"), [&] {
		try {
			m_device->queue_config().graphics.submit2(
				submit_info2,
				*m_sync.in_flight_fences[m_current_frame]
			);
		} catch (const vk::DeviceLostError&) {
			m_device->report_device_lost(std::format("submit2 (frame {}, image {})", m_current_frame, m_frame_context.image_index));
			throw;
		}
	});

	const vk::Semaphore render_finished_handle = *m_sync.render_finished_semaphores[m_frame_context.image_index];

	const vk::PresentInfoKHR present_info{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &render_finished_handle,
		.swapchainCount = 1,
		.pSwapchains = &*m_swapchain->config().swap_chain,
		.pImageIndices = &m_frame_context.image_index
	};

	vk::Result present_result;
	trace::scope(find_or_generate_id("end_frame::present"), [&] {
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
	});

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

auto gse::gpu::frame::add_transient_work(auto&& commands) -> void {
	const auto& vk_device = m_device->logical_device();

	const vk::CommandBufferAllocateInfo alloc_info{
		.commandPool = *m_device->command_config().pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1
	};

	auto buffers = vk_device.allocateCommandBuffers(alloc_info);
	vk::raii::CommandBuffer command_buffer = std::move(buffers[0]);

	constexpr vk::CommandBufferBeginInfo begin_info{
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	};

	command_buffer.begin(begin_info);

	std::vector<vulkan::buffer_resource> transient_buffers;
	if constexpr (std::is_void_v<std::invoke_result_t<decltype(commands), vk::raii::CommandBuffer&>>) {
		std::invoke(std::forward<decltype(commands)>(commands), command_buffer);
	} else {
		transient_buffers = std::invoke(std::forward<decltype(commands)>(commands), command_buffer);
	}

	command_buffer.end();

	vk::raii::Fence fence = vk_device.createFence({});

	const vk::SubmitInfo submit_info{
		.commandBufferCount = 1,
		.pCommandBuffers = &*command_buffer
	};

	try {
		m_device->queue_config().graphics.submit(submit_info, *fence);
	} catch (const vk::DeviceLostError&) {
		m_device->report_device_lost("transient graphics submission");
		throw;
	}

	m_graveyard[m_current_frame].push_back({
		.command_buffer = std::move(command_buffer),
		.fence = std::move(fence),
		.transient_buffers = std::move(transient_buffers)
	});
}

auto gse::gpu::frame::cleanup_finished(const std::uint32_t frame_index) -> void {
	for (auto& work : m_graveyard[frame_index]) {
		if (*work.fence) {
			try {
				(void)m_device->logical_device().waitForFences(
					*work.fence,
					vk::True,
					std::numeric_limits<std::uint64_t>::max()
				);
			} catch (const vk::DeviceLostError&) {
				m_device->report_device_lost("transient fence wait");
				throw;
			}
		}
	}
	m_graveyard[frame_index].clear();
}

auto gse::gpu::frame::create_sync_objects(const vulkan::device_config& device_data, const vulkan::swap_chain_config& swap_chain_data) -> vulkan::sync_config {
	std::vector<vk::raii::Semaphore> image_available;
	std::vector<vk::raii::Semaphore> render_finished;
	std::vector<vk::raii::Fence> in_flight_fences;

	const auto swap_chain_image_count = swap_chain_data.images.size();
	image_available.reserve(swap_chain_image_count);
	render_finished.reserve(swap_chain_image_count);

	in_flight_fences.reserve(vulkan::max_frames_in_flight);

	constexpr vk::FenceCreateInfo fence_ci{
		.flags = vk::FenceCreateFlagBits::eSignaled
	};

	for (std::size_t i = 0; i < swap_chain_image_count; ++i) {
		constexpr vk::SemaphoreCreateInfo bin_sem_ci{};
		image_available.emplace_back(device_data.device, bin_sem_ci);
		render_finished.emplace_back(device_data.device, bin_sem_ci);
	}

	for (std::size_t i = 0; i < vulkan::max_frames_in_flight; ++i) {
		in_flight_fences.emplace_back(device_data.device, fence_ci);
	}

	return vulkan::sync_config(
		std::move(image_available),
		std::move(render_finished),
		std::move(in_flight_fences)
	);
}
