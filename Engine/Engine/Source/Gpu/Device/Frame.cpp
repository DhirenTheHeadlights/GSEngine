module gse.gpu;

import std;

import :frame;
import :device;
import :swap_chain;

import gse.os;
import gse.assert;
import gse.diag;

auto gse::gpu::frame::create(device& dev, swap_chain& sc) -> std::unique_ptr<frame> {
	auto s = create_sync_objects(dev, sc);
	return std::unique_ptr<frame>(new frame(std::move(s), 0, dev, sc));
}

gse::gpu::frame::frame(sync&& s, const std::uint32_t image_index, device& dev, swap_chain& sc)
	: m_sync(std::move(s)), m_image_index(image_index), m_device(&dev), m_swapchain(&sc) {
}

auto gse::gpu::frame::current_frame() const -> std::uint32_t {
	return m_current_frame;
}

auto gse::gpu::frame::image_index() const -> std::uint32_t {
	return m_image_index;
}

auto gse::gpu::frame::command_buffer(const queue_type queue) const -> handle<gpu::command_buffer> {
	return m_command_buffers[static_cast<std::size_t>(queue)];
}

auto gse::gpu::frame::frame_in_progress() const -> bool {
	return m_frame_in_progress;
}

auto gse::gpu::frame::set_sync(sync&& s) -> void {
	m_sync = std::move(s);
}

auto gse::gpu::frame::recreate_resources(const window::data& win) -> void {
	const auto requested_size = window::viewport(win);
	const auto requested_mode = present_mode_from_setting_index(win.current_present_mode_index);
	const auto current_extent = m_swapchain->extent();
	const auto current_mode = m_swapchain->present_mode();

	const bool size_unchanged = current_extent.x() == static_cast<std::uint32_t>(requested_size.x()) &&
		current_extent.y() == static_cast<std::uint32_t>(requested_size.y());

	if (size_unchanged && current_mode != requested_mode) {
		m_swapchain->set_present_mode(requested_mode);
		return;
	}

	m_device->wait_idle();
	m_swapchain->recreate(requested_size, requested_mode);
	m_sync = create_sync_objects(*m_device, *m_swapchain);
	m_swapchain->notify_recreated();
	m_present_ids_in_flight.fill(0);
	m_device->wait_idle();
}

auto gse::gpu::frame::begin(window::data& win) -> std::expected<frame_token, frame_status> {
	m_frame_in_progress = false;

	if (window::minimized(win)) {
		return std::unexpected(frame_status::minimized);
	}

	try {
		trace::scope_guard sg{ trace_id<"begin_frame::wait_fence">() };
		for (std::size_t i = 0; i < queue_type_count; ++i) {
			const auto fence_result =
				m_device->wait_for_fence(
					m_sync.in_flight_fence(static_cast<queue_type>(i), m_current_frame)
				);
			assert(fence_result == result::success, "Failed to wait for in-flight fence!");
		}
	}
	catch (const device_lost_error&) {
		m_device->report_device_lost(std::format("begin_frame waitForFences (frame {})", m_current_frame));
		return std::unexpected(frame_status::device_lost);
	}

	const auto prior_present_id = m_present_ids_in_flight[m_current_frame];
	if (prior_present_id != 0) {
		trace::scope_guard sg{ trace_id<"begin_frame::wait_present">() };
		try {
			const auto present_wait_result = m_swapchain->wait_for_present(prior_present_id);
			assert(
				present_wait_result == result::success || present_wait_result == result::suboptimal_khr,
				"vkWaitForPresentKHR returned unexpected status"
			);
		}
		catch (const out_of_date_error&) {
		}
		catch (const device_lost_error&) {
			m_device->report_device_lost(std::format("waitForPresentKHR (frame {})", m_current_frame));
			return std::unexpected(frame_status::device_lost);
		}
	}

	try {
		trace::scope_guard sg{ trace_id<"begin_frame::transient">() };
		m_device->transient().begin_frame();
	}
	catch (const device_lost_error&) {
		m_device->report_device_lost(std::format("transient begin_frame (frame {})", m_current_frame));
		return std::unexpected(frame_status::device_lost);
	}

	if (window::frame_buffer_resized(win)) {
		recreate_resources(win);
		return std::unexpected(frame_status::swapchain_out_of_date);
	}

	result acquire_status = result::error_unknown;
	std::uint32_t acquired_image_index = 0;

	try {
		trace::scope_guard sg{ trace_id<"begin_frame::acquire">() };
		const auto acquired = m_swapchain->acquire(m_sync.image_available(m_current_frame));
		acquire_status = acquired.result;
		acquired_image_index = acquired.image_index;
	}
	catch (const out_of_date_error&) {
		acquire_status = result::error_out_of_date_khr;
	}
	catch (const device_lost_error&) {
		m_device->report_device_lost(std::format("acquireNextImage2KHR (frame {})", m_current_frame));
		return std::unexpected(frame_status::device_lost);
	}

	if (acquire_status == result::error_out_of_date_khr) {
		recreate_resources(win);
		return std::unexpected(frame_status::swapchain_out_of_date);
	}

	assert(
		acquire_status == result::success || acquire_status == result::suboptimal_khr,
		"Failed to acquire swap chain image!"
	);

	trace::scope_guard sg_setup{ trace_id<"begin_frame::setup">() };

	m_device->reset_fence(m_sync.in_flight_fence(queue_type::graphics, m_current_frame));

	m_image_index = acquired_image_index;

	const auto release_fence = m_swapchain->release_fence(m_image_index);
	const auto release_result = m_device->wait_for_fence(release_fence);
	assert(release_result == result::success, "Failed to wait for swapchain release fence!");

	for (std::size_t i = 0; i < queue_type_count; ++i) {
		m_command_buffers[i] = m_device->frame_command_buffer(static_cast<queue_type>(i), m_current_frame);
	}

	const commands cmd_main{ m_command_buffers[static_cast<std::size_t>(queue_type::graphics)] };
	cmd_main.reset();
	cmd_main.begin();

	const image_barrier acquire_barrier{
		.src_stages = pipeline_stage_flag::top_of_pipe,
		.src_access = {},
		.dst_stages = pipeline_stage_flag::color_attachment_output,
		.dst_access = access_flag::color_attachment_write | access_flag::color_attachment_read,
		.discard_contents = true,
		.image = m_swapchain->image(m_image_index),
		.aspects = image_aspect_flag::color,
	};
	cmd_main.pipeline_barrier(dependency_info{
		.image_barriers = std::span(&acquire_barrier, 1)
	});

	const std::array transient_visibility_barriers{
		memory_barrier{
			.src_stages = pipeline_stage_flag::acceleration_structure_build,
			.src_access = access_flag::acceleration_structure_write,
			.dst_stages = pipeline_stage_flag::acceleration_structure_build | pipeline_stage_flag::vertex_shader |
				pipeline_stage_flag::fragment_shader | pipeline_stage_flag::mesh_shader |
				pipeline_stage_flag::task_shader | pipeline_stage_flag::compute_shader,
			.dst_access = access_flag::acceleration_structure_read | access_flag::shader_read,
		},
		memory_barrier{
			.src_stages = pipeline_stage_flag::copy | pipeline_stage_flag::transfer,
			.src_access = access_flag::transfer_write,
			.dst_stages = pipeline_stage_flag::vertex_input | pipeline_stage_flag::index_input |
				pipeline_stage_flag::vertex_attribute_input | pipeline_stage_flag::draw_indirect |
				pipeline_stage_flag::vertex_shader | pipeline_stage_flag::fragment_shader |
				pipeline_stage_flag::mesh_shader | pipeline_stage_flag::task_shader |
				pipeline_stage_flag::compute_shader | pipeline_stage_flag::acceleration_structure_build,
			.dst_access = access_flag::vertex_attribute_read | access_flag::index_read | access_flag::shader_read |
				access_flag::shader_storage_read | access_flag::uniform_read | access_flag::indirect_command_read |
				access_flag::shader_sampled_read,
		},
	};
	cmd_main.pipeline_barrier(dependency_info{
		.memory_barriers = transient_visibility_barriers
	});

	m_device->transient().recorder().run_pre_frame(m_command_buffers[static_cast<std::size_t>(queue_type::graphics)]);

	m_frame_in_progress = true;

	return frame_token{
		.frame_index = m_current_frame,
		.image_index = acquired_image_index,
	};
}

auto gse::gpu::frame::end(window::data& win, std::span<const queue_submission> aux_submissions, std::span<const semaphore_submit_info> extra_graphics_waits) -> void {
	const auto graphics_cb = m_command_buffers[static_cast<std::size_t>(queue_type::graphics)];
	m_device->transient().recorder().run_post_frame(graphics_cb);

	const commands cmd_tail{ graphics_cb };

	cmd_tail.release_swapchain_image_to_present(
		m_swapchain->image(m_image_index),
		pipeline_stage_flag::color_attachment_output,
		access_flag::color_attachment_write
	);

	{
		trace::scope_guard sg{ trace_id<"end_frame::cmd_end">() };
		cmd_tail.end();
	}

	std::array<std::size_t, queue_type_count> last_idx_per_queue;
	last_idx_per_queue.fill(static_cast<std::size_t>(-1));
	for (std::size_t i = 0; i < aux_submissions.size(); ++i) {
		last_idx_per_queue[static_cast<std::size_t>(aux_submissions[i].queue)] = i;
	}
	for (std::size_t qi = 0; qi < queue_type_count; ++qi) {
		if (qi == static_cast<std::size_t>(queue_type::graphics)) {
			continue;
		}
		if (last_idx_per_queue[qi] != static_cast<std::size_t>(-1)) {
			m_device->reset_fence(m_sync.in_flight_fence(static_cast<queue_type>(qi), m_current_frame));
		}
	}

	{
		trace::scope_guard sg{ trace_id<"end_frame::submit_aux">() };
		for (std::size_t i = 0; i < aux_submissions.size(); ++i) {
			const auto& sub = aux_submissions[i];
			assert(
				sub.queue != queue_type::graphics,
				"graphics submissions go through the main submit path, not aux_submissions"
			);
			const bool last_for_queue = (last_idx_per_queue[static_cast<std::size_t>(sub.queue)] == i);
			try {
				const command_buffer_submit_info cmd_info{
					.command_buffer = sub.command_buffer,
				};
				const submit_info submit{
					.wait_semaphores = sub.waits,
					.command_buffers = std::span(&cmd_info, 1),
					.signal_semaphores = sub.signals,
				};
				m_device->submit(
					sub.queue,
					submit,
					last_for_queue ? m_sync.in_flight_fence(sub.queue, m_current_frame)
								   : handle<fence>{}
				);
			}
			catch (const device_lost_error&) {
				m_device->report_device_lost(
					std::format(
						"aux submit (frame {}, image {})",
						m_current_frame,
						m_image_index
					)
				);
				throw;
			}
		}
	}

	std::vector<semaphore_submit_info> main_waits;
	main_waits.push_back({
		.semaphore = m_sync.image_available(m_current_frame),
		.value = 0,
		.stages = pipeline_stage_flag::top_of_pipe,
	});

	for (const auto& w : extra_graphics_waits) {
		main_waits.push_back(w);
	}

	const semaphore_submit_info render_finished_signal{
		.semaphore = m_sync.render_finished(m_image_index),
		.value = 0,
		.stages = pipeline_stage_flag::bottom_of_pipe,
	};

	{
		trace::scope_guard sg{ trace_id<"end_frame::submit">() };
		try {
			const command_buffer_submit_info cmd_info{
				.command_buffer = graphics_cb,
			};
			const submit_info submit{
				.wait_semaphores = main_waits,
				.command_buffers = std::span(&cmd_info, 1),
				.signal_semaphores = std::span(&render_finished_signal, 1),
			};
			m_device->submit(queue_type::graphics, submit,
							 m_sync.in_flight_fence(queue_type::graphics, m_current_frame));
		}
		catch (const device_lost_error&) {
			m_device->report_device_lost(std::format("submit2 (frame {}, image {})", m_current_frame, m_image_index));
			throw;
		}
	}

	const handle<semaphore> render_finished_handle = m_sync.render_finished(m_image_index);
	const std::uint64_t present_id = m_next_present_id++;

	result present_result;
	{
		trace::scope_guard sg{ trace_id<"end_frame::present">() };
		try {
			present_result = m_swapchain->present(render_finished_handle, m_image_index, present_id);
		}
		catch (const out_of_date_error&) {
			present_result = result::error_out_of_date_khr;
		}
		catch (const device_lost_error&) {
			m_device->report_device_lost(std::format("presentKHR (frame {}, image {})", m_current_frame,
													 m_image_index));
			throw;
		}
	}

	if (present_result == result::error_out_of_date_khr || present_result == result::suboptimal_khr) {
		recreate_resources(win);
		m_present_ids_in_flight.fill(0);
	}
	else {
		assert(present_result == result::success, "Failed to present swap chain image!");
		m_present_ids_in_flight[m_current_frame] = present_id;
	}

	m_current_frame = (m_current_frame + 1) % max_frames_in_flight;
	m_frame_in_progress = false;
}

auto gse::gpu::frame::create_sync_objects(device& dev, const swap_chain& sc) -> sync {
	return sync::create(dev.vulkan_device(), sc.image_count());
}
