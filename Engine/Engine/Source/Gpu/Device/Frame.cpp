module gse.gpu:frame_impl;

import std;

import :frame;
import :device;
import :swap_chain;
import :present_pacer;

import gse.gpu_backend;
import gse.os;
import gse.assert;
import gse.diag;
import gse.log;
import gse.meta;
import gse.time;

namespace gse::gpu {
	constexpr std::uint32_t pacing_health_check_frame = 600;

	std::uint32_t present_total = 0;
	std::uint32_t present_queue_full = 0;
	bool pacing_health_reported = false;
}

auto gse::gpu::frame::create(device& dev, swap_chain* sc) -> std::unique_ptr<frame> {
	auto s = sc ? create_sync_objects(dev, *sc) : frame_sync<device>::create(dev, max_frames_in_flight);
	return std::unique_ptr<frame>(new frame(std::move(s), dev, sc));
}

gse::gpu::frame::frame(frame_sync<device>&& s, device& dev, swap_chain* sc)
	: m_sync(std::move(s)), m_device(&dev), m_swapchain(sc) {
}

auto gse::gpu::frame::current_frame() const -> std::uint32_t {
	return m_current_frame;
}

auto gse::gpu::frame::image_index() const -> std::uint32_t {
	return m_image_index;
}

auto gse::gpu::frame::command_buffer(const queue_type queue) const -> command_buffer_handle {
	return command_buffer_handle{ m_command_buffers[static_cast<std::size_t>(queue)] };
}

auto gse::gpu::frame::frame_in_progress() const -> bool {
	return m_frame_in_progress;
}

auto gse::gpu::frame::set_sync(frame_sync<device>&& s) -> void {
	m_sync = std::move(s);
}

auto gse::gpu::frame::recreate_resources(const window::data& win) -> void {
	const auto requested_size = window::viewport(win);
	const auto requested_mode = gse::enum_from_annotation<present_mode_setting>(win.current_present_mode_index, present_mode::fifo);
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

auto gse::gpu::frame::begin(window::data* win) -> std::expected<frame_token, frame_status> {
	m_frame_in_progress = false;

	if (win && window::minimized(*win)) {
		return std::unexpected(frame_status::minimized);
	}

	{
		trace::scope_guard sg{ trace_id<"begin_frame::wait_fence">() };
		for (std::size_t i = 0; i < queue_type_count; ++i) {
			const auto fence_result =
				m_device->wait_for_fence(
					m_sync.in_flight_fence(static_cast<queue_type>(i), m_current_frame)
				);
			if (fence_result == result::error_device_lost) {
				m_device->report_device_lost(std::format("begin_frame waitForFences (frame {})", m_current_frame));
				return std::unexpected(frame_status::device_lost);
			}
			assert(fence_result == result::success, "Failed to wait for in-flight fence!");
		}
	}

	if (m_swapchain && !(win && win->attached)) {
		trace::scope_guard sg{ trace_id<"begin_frame::present_feedback">() };
		const auto refresh = m_swapchain->refresh_interval();
		m_pacer.observe(m_swapchain->past_presentation_timing(), refresh);
		system_clock::submit_refresh_interval(quantity_cast<system_clock::internal_time>(refresh));
		if (m_pacer.has_feedback()) {
			system_clock::submit_display_interval(m_pacer.frame_delta());
		}

		if (!pacing_health_reported && present_total >= pacing_health_check_frame) {
			pacing_health_reported = true;
			const auto seen = m_pacer.samples_seen();
			const auto used = m_pacer.samples_used();
			if (!m_pacer.has_feedback() || seen == 0 || used * 4 < seen) {
				log::println(
					log::level::warning,
					log::category::render,
					"present pacing degraded after {} presents: feedback={} samples_seen={} samples_used={} queue_full_retries={}. "
					"dt will fall back to CPU loop timing, which does not match display cadence and shows up as motion judder",
					present_total,
					m_pacer.has_feedback(),
					seen,
					used,
					present_queue_full
				);
			}
		}
	}

	{
		trace::scope_guard sg{ trace_id<"begin_frame::transient">() };
		m_device->transient().begin_frame();
	}

	if (m_swapchain) {
		if (window::frame_buffer_resized(*win)) {
			recreate_resources(*win);
			return std::unexpected(frame_status::swapchain_out_of_date);
		}

		result acquire_status = result::error_unknown;
		std::uint32_t acquired_image_index = 0;

		{
			trace::scope_guard sg{ trace_id<"begin_frame::acquire">() };
			const auto acquired = m_swapchain->acquire(m_sync.image_available(m_current_frame));
			acquire_status = acquired.result;
			acquired_image_index = acquired.image_index;
		}
		if (acquire_status == result::error_device_lost) {
			m_device->report_device_lost(std::format("acquireNextImage2KHR (frame {})", m_current_frame));
			return std::unexpected(frame_status::device_lost);
		}

		if (acquire_status == result::error_out_of_date_khr) {
			recreate_resources(*win);
			return std::unexpected(frame_status::swapchain_out_of_date);
		}

		if (acquire_status != result::success && acquire_status != result::suboptimal_khr) {
			recreate_resources(*win);
			return std::unexpected(frame_status::swapchain_out_of_date);
		}

		m_device->reset_fence(m_sync.in_flight_fence(queue_type::graphics, m_current_frame));

		m_image_index = acquired_image_index;

		const auto release_fence = m_swapchain->release_fence(m_image_index);
		const auto release_result = m_device->wait_for_fence(release_fence);
		if (release_result == result::error_device_lost) {
			m_device->report_device_lost(std::format("begin_frame release fence (frame {}, image {})", m_current_frame, m_image_index));
			return std::unexpected(frame_status::device_lost);
		}
		assert(release_result == result::success, "Failed to wait for swapchain release fence!");
	}

	trace::scope_guard sg_setup{ trace_id<"begin_frame::setup">() };

	for (std::size_t i = 0; i < queue_type_count; ++i) {
		m_command_buffers[i] = m_device->frame_command_buffer(static_cast<queue_type>(i), m_current_frame).value;
	}

	const auto cmd_main = command_buffer_handle{ m_command_buffers[static_cast<std::size_t>(queue_type::graphics)] };
	m_device->cmd_reset(cmd_main);
	m_device->cmd_begin(cmd_main);

	if (m_swapchain) {
		const image_barrier acquire_barrier{
			.src_stages = pipeline_stage_flag::top_of_pipe,
			.src_access = {},
			.dst_stages = pipeline_stage_flag::color_attachment_output,
			.dst_access = { access_flag::color_attachment_write, access_flag::color_attachment_read },
			.discard_contents = true,
			.image = m_swapchain->image(m_image_index),
			.aspects = image_aspect_flag::color,
		};
		m_device->cmd_pipeline_barrier(cmd_main, dependency_info{
			.image_barriers = std::span(&acquire_barrier, 1)
		});
	}

	const std::array transient_visibility_barriers{
		memory_barrier{
			.src_stages = pipeline_stage_flag::acceleration_structure_build,
			.src_access = access_flag::acceleration_structure_write,
			.dst_stages = { pipeline_stage_flag::acceleration_structure_build, pipeline_stage_flag::vertex_shader,
				pipeline_stage_flag::fragment_shader, pipeline_stage_flag::mesh_shader,
				pipeline_stage_flag::task_shader, pipeline_stage_flag::compute_shader },
			.dst_access = { access_flag::acceleration_structure_read, access_flag::shader_read },
		},
		memory_barrier{
			.src_stages = { pipeline_stage_flag::copy, pipeline_stage_flag::transfer },
			.src_access = access_flag::transfer_write,
			.dst_stages = { pipeline_stage_flag::vertex_input, pipeline_stage_flag::index_input,
				pipeline_stage_flag::vertex_attribute_input, pipeline_stage_flag::draw_indirect,
				pipeline_stage_flag::vertex_shader, pipeline_stage_flag::fragment_shader,
				pipeline_stage_flag::mesh_shader, pipeline_stage_flag::task_shader,
				pipeline_stage_flag::compute_shader, pipeline_stage_flag::acceleration_structure_build },
			.dst_access = { access_flag::vertex_attribute_read, access_flag::index_read, access_flag::shader_read,
				access_flag::shader_storage_read, access_flag::uniform_read, access_flag::indirect_command_read,
				access_flag::shader_sampled_read },
		},
	};
	m_device->cmd_pipeline_barrier(cmd_main, dependency_info{
		.memory_barriers = transient_visibility_barriers
	});

	m_device->transient().recorder().run_pre_frame(command_buffer_handle{ m_command_buffers[static_cast<std::size_t>(queue_type::graphics)] });

	m_frame_in_progress = true;

	return frame_token{
		.frame_index = m_current_frame,
		.image_index = m_image_index,
	};
}

auto gse::gpu::frame::end(window::data* win, std::span<const queue_submission> aux_submissions, std::span<const semaphore_submit_info> extra_graphics_waits, std::span<const command_buffer_handle> graphics_buffers, std::span<const semaphore_submit_info> extra_graphics_signals) -> void {
	const auto graphics_begin = command_buffer_handle{ m_command_buffers[static_cast<std::size_t>(queue_type::graphics)] };
	{
		trace::scope_guard sg{ trace_id<"end_frame::cmd_end">() };
		m_device->cmd_end(graphics_begin);
	}

	const auto graphics_end = m_device->acquire_worker_command_buffer(queue_type::graphics, 0, m_current_frame);
	m_device->cmd_begin(graphics_end);
	m_device->transient().recorder().run_post_frame(graphics_end);
	if (m_swapchain) {
		m_device->cmd_release_swapchain_to_present(
			graphics_end,
			m_swapchain->image(m_image_index),
			pipeline_stage_flag::color_attachment_output,
			access_flag::color_attachment_write
		);
	}
	m_device->cmd_end(graphics_end);

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
			{
				std::vector<command_buffer_submit_info> cmd_infos;
				cmd_infos.reserve(sub.command_buffers.size());
				for (const auto cb : sub.command_buffers) {
					cmd_infos.push_back({ .command_buffer = cb });
				}
				const submit_info submit{
					.wait_semaphores = sub.waits,
					.command_buffers = cmd_infos,
					.signal_semaphores = sub.signals,
				};
				m_device->submit(
					sub.queue,
					submit,
					last_for_queue ? m_sync.in_flight_fence(sub.queue, m_current_frame)
								   : handle<gpu::fence>{}
				);
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

	std::vector<semaphore_submit_info> main_signals;
	main_signals.push_back(render_finished_signal);
	for (const auto& s : extra_graphics_signals) {
		main_signals.push_back(s);
	}

	{
		trace::scope_guard sg{ trace_id<"end_frame::submit">() };
		std::vector<command_buffer_submit_info> cmd_infos;
		cmd_infos.reserve(2 + graphics_buffers.size());
		cmd_infos.push_back({ .command_buffer = graphics_begin });
		for (const auto cb : graphics_buffers) {
			cmd_infos.push_back({ .command_buffer = cb });
		}
		cmd_infos.push_back({ .command_buffer = graphics_end });
		const submit_info submit{
			.wait_semaphores = main_waits,
			.command_buffers = cmd_infos,
			.signal_semaphores = main_signals,
		};
		m_device->submit(queue_type::graphics, submit,
						 m_sync.in_flight_fence(queue_type::graphics, m_current_frame));
	}

	if (m_swapchain) {
		const handle<gpu::semaphore> render_finished_handle = m_sync.render_finished(m_image_index);
		const std::uint64_t present_id = m_next_present_id++;
		const bool request_timing = !(win && win->attached);

		result present_result;
		{
			trace::scope_guard sg{ trace_id<"end_frame::present">() };
			{
				trace::scope_guard sg_timed{ trace_id<"present::timed">() };
				present_result = m_swapchain->present(render_finished_handle, m_image_index, present_id, request_timing);
			}
			++present_total;
			if (present_result == result::error_present_timing_queue_full) {
				++present_queue_full;
				trace::scope_guard sg_retry{ trace_id<"present::retry_untimed">() };
				present_result = m_swapchain->present(render_finished_handle, m_image_index, present_id, false);
			}
			if (present_result == result::error_device_lost) {
				m_device->report_device_lost(std::format("presentKHR (frame {}, image {})", m_current_frame, m_image_index));
			}
		}

		if (present_result == result::error_out_of_date_khr || present_result == result::suboptimal_khr) {
			recreate_resources(*win);
			m_present_ids_in_flight.fill(0);
		}
		else {
			assert(present_result == result::success, "Failed to present swap chain image!");
			m_present_ids_in_flight[m_current_frame] = present_id;
		}
	}

	m_current_frame = (m_current_frame + 1) % max_frames_in_flight;
	m_frame_in_progress = false;
}

auto gse::gpu::frame::create_sync_objects(device& dev, const swap_chain& sc) -> frame_sync<device> {
	return frame_sync<device>::create(dev, sc.image_count());
}
