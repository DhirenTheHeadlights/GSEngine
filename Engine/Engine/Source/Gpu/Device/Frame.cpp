module gse.gpu;

import std;

import :frame;
import :transient_executor;
import :vulkan_command_pools;
import :vulkan_commands;
import :vulkan_device;
import :vulkan_queues;
import :vulkan_swapchain;
import :vulkan_sync;
import :device;
import :swap_chain;
import :types;

import gse.os;
import gse.assert;
import gse.diag;

auto gse::gpu::frame::create(device& dev, swap_chain& sc) -> std::unique_ptr<frame> {
    auto sync = create_sync_objects(dev.vulkan_device(), sc.config());
    return std::make_unique<frame>(std::move(sync), 0, dev.vulkan_command().frame_command_buffer(0), dev, sc);
}

gse::gpu::frame::frame(vulkan::sync&& sync, const std::uint32_t image_index, const handle<vulkan::command_buffer> command_buffer, device& dev, swap_chain& sc)
    : m_sync(std::move(sync)), m_image_index(image_index), m_command_buffer(command_buffer), m_device(&dev), m_swapchain(&sc) {}

auto gse::gpu::frame::current_frame() const -> std::uint32_t {
    return m_current_frame;
}

auto gse::gpu::frame::image_index() const -> std::uint32_t {
    return m_image_index;
}

auto gse::gpu::frame::command_buffer() const -> handle<vulkan::command_buffer> {
    return m_command_buffer;
}

auto gse::gpu::frame::frame_in_progress() const -> bool {
    return m_frame_in_progress;
}

auto gse::gpu::frame::set_sync(vulkan::sync&& sync) -> void {
    m_sync = std::move(sync);
}

auto gse::gpu::frame::recreate_resources(const window& win) -> void {
    m_device->wait_idle();
    m_swapchain->recreate(win.viewport());
    m_sync = create_sync_objects(m_device->vulkan_device(), m_swapchain->config());
    m_swapchain->notify_recreated();
    m_device->wait_idle();
}

auto gse::gpu::frame::begin(window& win) -> std::expected<frame_token, frame_status> {
    auto& dev = m_device->vulkan_device();

    m_frame_in_progress = false;

    if (win.minimized()) {
        return std::unexpected(frame_status::minimized);
    }

    try {
        const auto fence_result = vulkan::wait_for_fence(dev, m_sync.in_flight_fence(m_current_frame));
        assert(fence_result == result::success, std::source_location::current(), "Failed to wait for in-flight fence!");
    } catch (const vk::DeviceLostError&) {
        m_device->report_device_lost(std::format("begin_frame waitForFences (frame {})", m_current_frame));
        return std::unexpected(frame_status::device_lost);
    }

    try {
        m_device->transient().begin_frame(dev);
    } catch (const vk::DeviceLostError&) {
        m_device->report_device_lost(std::format("transient begin_frame (frame {})", m_current_frame));
        return std::unexpected(frame_status::device_lost);
    }

    if (win.frame_buffer_resized()) {
        recreate_resources(win);
        return std::unexpected(frame_status::swapchain_out_of_date);
    }

    result acquire_status = result::error_unknown;
    std::uint32_t acquired_image_index = 0;
    try {
        const auto acquired = vulkan::acquire_next_image(
            dev,
            m_swapchain->config().swap_chain_handle(),
            m_sync.image_available(m_current_frame)
        );
        acquire_status = acquired.result;
        acquired_image_index = acquired.image_index;
    } catch (const vk::OutOfDateKHRError&) {
        acquire_status = result::error_out_of_date_khr;
    } catch (const vk::DeviceLostError&) {
        m_device->report_device_lost(std::format("acquireNextImage2KHR (frame {})", m_current_frame));
        return std::unexpected(frame_status::device_lost);
    }

    if (acquire_status == result::error_out_of_date_khr) {
        recreate_resources(win);
        return std::unexpected(frame_status::swapchain_out_of_date);
    }

    assert(
        acquire_status == result::success || acquire_status == result::suboptimal_khr,
        std::source_location::current(),
        "Failed to acquire swap chain image!"
    );

    vulkan::reset_fence(dev, m_sync.in_flight_fence(m_current_frame));

    m_image_index = acquired_image_index;
    m_command_buffer = m_device->vulkan_command().frame_command_buffer(m_current_frame);
    const vulkan::commands cmd{ m_command_buffer };
    cmd.reset();
    cmd.begin();

    m_device->transient().recorder().run_pre_frame(m_command_buffer);

    m_frame_in_progress = true;

    return frame_token{
        .frame_index = m_current_frame,
        .image_index = acquired_image_index,
    };
}

auto gse::gpu::frame::add_wait_semaphore(const compute_semaphore_state& state) -> void {
    if (state.raii_semaphore() && state.value() > 0) {
        m_extra_waits.push_back(state);
    }
}

auto gse::gpu::frame::end(window& win) -> void {
    m_device->transient().recorder().run_post_frame(m_command_buffer);

    {
    	trace::scope_guard sg{trace_id<"end_frame::cmd_end">()};
        vulkan::commands{ m_command_buffer }.end();
    }

    std::vector<semaphore_submit_info> wait_infos;
    wait_infos.push_back({
        .semaphore = m_sync.image_available(m_current_frame),
        .value = 0,
        .stages = pipeline_stage_flag::top_of_pipe,
    });

    for (const auto& wait : m_extra_waits) {
        wait_infos.push_back({
            .semaphore = std::bit_cast<handle<semaphore>>(**wait.raii_semaphore()),
            .value = wait.value(),
            .stages = pipeline_stage_flag::all_commands,
        });
    }
    m_extra_waits.clear();

    const command_buffer_submit_info cmd_info{
        .command_buffer = m_command_buffer,
    };

    const semaphore_submit_info signal_info{
        .semaphore = m_sync.render_finished(m_image_index),
        .value = 0,
        .stages = pipeline_stage_flag::bottom_of_pipe,
    };

    const submit_info submit{
        .wait_semaphores = wait_infos,
        .command_buffers = std::span(&cmd_info, 1),
        .signal_semaphores = std::span(&signal_info, 1),
    };

    {
    	trace::scope_guard sg{trace_id<"end_frame::submit">()};
        try {
            m_device->vulkan_queue().submit_graphics(
                submit,
                m_sync.in_flight_fence(m_current_frame)
            );
        } catch (const vk::DeviceLostError&) {
            m_device->report_device_lost(std::format("submit2 (frame {}, image {})", m_current_frame, m_image_index));
            throw;
        }
    }

    const handle<semaphore> render_finished_handle = m_sync.render_finished(m_image_index);
    const handle<vulkan::swap_chain> swapchain_handle = m_swapchain->config().swap_chain_handle();

    const present_info present_info{
        .wait_semaphores = std::span(&render_finished_handle, 1),
        .swapchains = std::span(&swapchain_handle, 1),
        .image_indices = std::span(&m_image_index, 1),
    };

    result present_result;
    {
    	trace::scope_guard sg{trace_id<"end_frame::present">()};
        try {
            present_result = m_device->vulkan_queue().present(present_info);
        }
        catch (const vk::OutOfDateKHRError&) {
            present_result = result::error_out_of_date_khr;
        }
        catch (const vk::DeviceLostError&) {
            m_device->report_device_lost(std::format("presentKHR (frame {}, image {})", m_current_frame, m_image_index));
            throw;
        }
    }

    if (present_result == result::error_out_of_date_khr || present_result == result::suboptimal_khr) {
        recreate_resources(win);
    }
    else {
        assert(
            present_result == result::success,
            std::source_location::current(),
            "Failed to present swap chain image!"
        );
    }

    m_current_frame = (m_current_frame + 1) % vulkan::max_frames_in_flight;
    m_frame_in_progress = false;
}

auto gse::gpu::frame::create_sync_objects(const vulkan::device& device_data, const vulkan::swap_chain& swap_chain_data) -> vulkan::sync {
    return vulkan::sync::create(device_data, swap_chain_data.image_count());
}
