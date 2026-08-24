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
	constexpr std::uint32_t pacing_transition_log_limit = 8;

	std::uint32_t present_total = 0;
	std::uint32_t present_queue_full = 0;
	std::uint32_t pacing_transitions = 0;
	bool pacing_health_reported = false;
	bool pacing_last_healthy = true;
}

int gse::gpu::frame::s_live_count = 0;

auto gse::gpu::frame::create(device& dev, swap_chain* sc) -> std::unique_ptr<frame> {
	assert(
		s_live_count++ == 0,
		"gpu::frame is one-per-device: it owns the device's per-frame command pools and transient lifecycle"
	);

	auto s = sc ? create_sync_objects(dev, *sc) : swapchain_sync<device>::create(dev, max_frames_in_flight);
	auto fences = frame_fences<device>::create(dev);
	return std::unique_ptr<frame>(new frame(std::move(s), std::move(fences), dev, sc));
}

gse::gpu::frame::frame(swapchain_sync<device>&& s, frame_fences<device>&& fences, device& dev, swap_chain* sc)
	: m_fences(std::move(fences)), m_device(&dev) {
	m_targets.push_back(present_target{
		.swapchain = sc,
		.sync = std::move(s),
	});
}

auto gse::gpu::frame::set_present_target(window::window_surface* win) -> void {
	m_targets.front().window = win;
}

auto gse::gpu::frame::add_present_target(const gse::id window_id, swap_chain* sc, window::window_surface* win) -> void {
	assert(sc != nullptr, "a secondary present target needs a swapchain");
	m_targets.push_back(present_target{
		.window_id = window_id,
		.swapchain = sc,
		.window = win,
		.sync = create_sync_objects(*m_device, *sc),
	});
}

auto gse::gpu::frame::remove_present_target(const gse::id window_id) -> void {
	assert(window_id.exists(), "the primary present target cannot be removed");
	std::erase_if(m_targets, [window_id](const present_target& t) {
		return t.window_id == window_id;
	});
}

auto gse::gpu::frame::target(const gse::id window_id) const -> const present_target* {
	const auto it = std::ranges::find(m_targets, window_id, &present_target::window_id);
	return it == m_targets.end() ? nullptr : std::addressof(*it);
}

auto gse::gpu::frame::targets() const -> std::span<const present_target> {
	return m_targets;
}

gse::gpu::frame::~frame() {
	--s_live_count;
}

auto gse::gpu::frame::swapchain() const -> swap_chain* {
	return m_targets.front().swapchain;
}

auto gse::gpu::frame::current_frame() const -> std::uint32_t {
	return m_current_frame;
}

auto gse::gpu::frame::frame_count() const -> std::uint64_t {
	return m_frame_count;
}

auto gse::gpu::frame::queue_fence_signaled(const queue_type queue, const std::uint32_t ring_slot) const -> bool {
	return m_device->wait_for_fence(m_fences.in_flight(queue, ring_slot), 0) == result::success;
}

auto gse::gpu::frame::image_index() const -> std::uint32_t {
	return m_targets.front().image_index;
}

auto gse::gpu::frame::command_buffer(const queue_type queue) const -> command_buffer_handle {
	return command_buffer_handle{ m_command_buffers[static_cast<std::size_t>(queue)] };
}

auto gse::gpu::frame::frame_in_progress() const -> bool {
	return m_frame_in_progress;
}

auto gse::gpu::frame::recreate_resources(present_target& t) -> gpu::expected<void> {
	const window::window_surface& win = *t.window;
	swap_chain* const m_swapchain = t.swapchain;
	const auto requested_size = window::viewport(win);
	const auto requested_mode = gse::enum_from_annotation<present_mode_setting>(win.present_mode_index, present_mode::fifo);
	const auto current_extent = m_swapchain->extent();
	const auto current_mode = m_swapchain->present_mode();

	const bool size_unchanged = current_extent.x() == static_cast<std::uint32_t>(requested_size.x()) &&
		current_extent.y() == static_cast<std::uint32_t>(requested_size.y());

	if (size_unchanged && current_mode != requested_mode) {
		log::println(
			log::category::render,
			"[swapchain] present-mode-only change at {}x{}, no recreate",
			current_extent.x(),
			current_extent.y()
		);
		m_swapchain->set_present_mode(requested_mode);
		return {};
	}

	m_device->wait_idle();

	if (const auto recreated = m_swapchain->recreate(requested_size, requested_mode); !recreated) {
		log::println(log::level::error, log::category::render, "swapchain recreate failed ({}), retrying next frame", std::to_underlying(recreated.error()));
		return std::unexpected(recreated.error());
	}

	const auto granted_extent = m_swapchain->extent();
	log::println(
		log::category::render,
		"[swapchain] recreate_resources: requested={}x{} previous={}x{} granted={}x{} minimized={} pos={},{} frame={}",
		requested_size.x(),
		requested_size.y(),
		current_extent.x(),
		current_extent.y(),
		granted_extent.x(),
		granted_extent.y(),
		window::minimized(win),
		win.position.x(),
		win.position.y(),
		m_frame_count
	);

	t.sync = create_sync_objects(*m_device, *m_swapchain);
	m_swapchain->notify_recreated();
	t.present_ids_in_flight.fill(0);
	m_device->wait_idle();
	return {};
}

auto gse::gpu::frame::recreate_surface(present_target& t, const std::string_view reason) -> gpu::expected<void> {
	const window::window_surface& win = *t.window;
	swap_chain* const m_swapchain = t.swapchain;
	log::println(log::level::warning, log::category::render, "{}, rebuilding surface and swapchain", reason);

	const auto requested_size = window::viewport(win);
	const auto requested_mode = gse::enum_from_annotation<present_mode_setting>(win.present_mode_index, present_mode::fifo);

	m_swapchain->replace_surface(m_device->recreate_surface(win, m_swapchain->current_handle()));

	if (const auto recreated = m_swapchain->recreate_detached(requested_size, requested_mode); !recreated) {
		log::println(log::level::error, log::category::render, "swapchain rebuild after surface loss failed ({}), retrying next frame", std::to_underlying(recreated.error()));
		return std::unexpected(recreated.error());
	}

	const auto granted_extent = m_swapchain->extent();
	log::println(
		log::category::render,
		"[swapchain] recreate_surface: requested={}x{} granted={}x{} minimized={} frame={}",
		requested_size.x(),
		requested_size.y(),
		granted_extent.x(),
		granted_extent.y(),
		window::minimized(win),
		m_frame_count
	);

	t.sync = create_sync_objects(*m_device, *m_swapchain);
	m_swapchain->notify_recreated();
	t.present_ids_in_flight.fill(0);
	m_device->wait_idle();
	return {};
}

auto gse::gpu::frame::begin() -> std::expected<frame_token, frame_status> {
	present_target& primary = m_targets.front();
	window::window_surface* win = primary.window;
	swap_chain* const m_swapchain = primary.swapchain;
	m_frame_in_progress = false;

	const bool minimized_now = win && window::minimized(*win);
	if (minimized_now != primary.minimized_last) {
		log::println(
			log::category::render,
			"[swapchain] minimized {} -> {} after {} frames: swapchain_extent={}x{} viewport={}x{}",
			primary.minimized_last,
			minimized_now,
			primary.minimized_frames,
			m_swapchain ? m_swapchain->extent().x() : 0u,
			m_swapchain ? m_swapchain->extent().y() : 0u,
			win ? window::viewport(*win).x() : 0,
			win ? window::viewport(*win).y() : 0
		);
		primary.restore_pending = !minimized_now;
		primary.minimized_last = minimized_now;
		primary.minimized_frames = 0;
	}
	++primary.minimized_frames;

	if (minimized_now) {
		return std::unexpected(frame_status::minimized);
	}

	{
		trace::scope_guard _{ trace_id<"begin_frame::wait_fence">() };
		for (std::size_t i = 0; i < queue_type_count; ++i) {
			const auto queue = static_cast<queue_type>(i);
			const auto queue_tag = queue == queue_type::graphics
				? trace_id<"begin_frame::wait_fence::graphics">()
				: queue == queue_type::compute
					? trace_id<"begin_frame::wait_fence::compute">()
					: trace_id<"begin_frame::wait_fence::video_encode">();
			trace::scope_guard _{ queue_tag };
			const auto fence_result =
				m_device->wait_for_fence(
					m_fences.in_flight(queue, m_current_frame)
				);
			if (fence_result == result::error_device_lost) {
				m_device->report_device_lost(std::format("begin_frame waitForFences (frame {})", m_current_frame));
				return std::unexpected(frame_status::device_lost);
			}
			assert(fence_result == result::success, "Failed to wait for in-flight fence!");
		}
	}

	if (m_swapchain && !(win && win->attached)) {
		trace::scope_guard _{ trace_id<"begin_frame::present_feedback">() };
		const auto refresh = m_swapchain->refresh_interval();
		m_primary_pacer.observe(m_swapchain->past_presentation_timing(), refresh);
		system_clock::submit_refresh_interval(quantity_cast<system_clock::internal_time>(refresh));
		if (m_primary_pacer.has_feedback() && m_primary_pacer.healthy()) {
			system_clock::submit_display_interval(m_primary_pacer.frame_delta());
		}

		if (m_primary_pacer.healthy() != pacing_last_healthy) {
			pacing_last_healthy = m_primary_pacer.healthy();
			++pacing_transitions;
			if (pacing_transitions <= pacing_transition_log_limit) {
				log::println(
					log::category::render,
					"present pacing {} after {} presents (samples_seen={} samples_used={}): dt now driven by {}{}",
					pacing_last_healthy ? "recovered" : "degraded",
					present_total,
					m_primary_pacer.samples_seen(),
					m_primary_pacer.samples_used(),
					pacing_last_healthy ? "display timing" : "snapped CPU loop timing",
					pacing_transitions == pacing_transition_log_limit ? " — pacing is flapping, further transitions suppressed" : ""
				);
			}
		}

		if (!pacing_health_reported && present_total >= pacing_health_check_frame) {
			pacing_health_reported = true;
			if (!m_primary_pacer.has_feedback() || !m_primary_pacer.healthy()) {
				log::println(
					log::level::warning,
					log::category::render,
					"present pacing degraded after {} presents: feedback={} healthy={} samples_seen={} samples_used={} queue_full_retries={}. "
					"dt is falling back to snapped CPU loop timing until presentation feedback recovers",
					present_total,
					m_primary_pacer.has_feedback(),
					m_primary_pacer.healthy(),
					m_primary_pacer.samples_seen(),
					m_primary_pacer.samples_used(),
					present_queue_full
				);
			}
		}
	}

	{
		trace::scope_guard _{ trace_id<"begin_frame::transient">() };
		m_device->transient().begin_frame();
	}

	for (present_target& t : m_targets) {
		t.acquired = false;
		if (!t.swapchain) {
			continue;
		}

		const bool is_primary = std::addressof(t) == std::addressof(m_targets.front());

		if (!t.swapchain->current_handle()) {
			(void)recreate_surface(t, "swapchain handle gone");
			if (is_primary) {
				return std::unexpected(frame_status::swapchain_out_of_date);
			}
			continue;
		}

		if (t.restore_pending) {
			t.restore_pending = false;
			(void)window::frame_buffer_resized(*t.window);
			(void)recreate_surface(t, "window restored from minimize");
			if (is_primary) {
				return std::unexpected(frame_status::swapchain_out_of_date);
			}
			continue;
		}

		if (window::frame_buffer_resized(*t.window)) {
			(void)recreate_resources(t);
			if (is_primary) {
				return std::unexpected(frame_status::swapchain_out_of_date);
			}
			continue;
		}

		result acquire_status = result::error_unknown;
		std::uint32_t acquired_image_index = 0;
		{
			trace::scope_guard _{ trace_id<"begin_frame::acquire">() };
			const auto acquired = t.swapchain->acquire(t.sync.image_available(m_current_frame));
			acquire_status = acquired.result;
			acquired_image_index = acquired.image_index;
		}

		if (acquire_status == result::error_device_lost) {
			m_device->report_device_lost(std::format("acquireNextImage2KHR (frame {})", m_current_frame));
			return std::unexpected(frame_status::device_lost);
		}

		if (acquire_status == result::error_surface_lost_khr) {
			(void)recreate_surface(t, "surface lost on acquire");
			if (is_primary) {
				return std::unexpected(frame_status::swapchain_out_of_date);
			}
			continue;
		}

		if (acquire_status != result::success && acquire_status != result::suboptimal_khr) {
			(void)recreate_resources(t);
			if (is_primary) {
				return std::unexpected(frame_status::swapchain_out_of_date);
			}
			continue;
		}

		t.image_index = acquired_image_index;

		const auto release_fence = t.swapchain->release_fence(t.image_index);
		const auto release_result = m_device->wait_for_fence(release_fence);
		if (release_result == result::error_device_lost) {
			m_device->report_device_lost(std::format("begin_frame release fence (frame {}, image {})", m_current_frame, t.image_index));
			return std::unexpected(frame_status::device_lost);
		}
		assert(release_result == result::success, "Failed to wait for swapchain release fence!");

		t.acquired = true;
	}

	m_device->reset_fence(m_fences.in_flight(queue_type::graphics, m_current_frame));

	trace::scope_guard _{ trace_id<"begin_frame::setup">() };

	m_device->reset_worker_command_pools(m_current_frame);

	for (std::size_t i = 0; i < queue_type_count; ++i) {
		m_command_buffers[i] = m_device->frame_command_buffer(static_cast<queue_type>(i), m_current_frame).value;
	}

	const auto cmd_main = command_buffer_handle{ m_command_buffers[static_cast<std::size_t>(queue_type::graphics)] };
	m_device->cmd_reset(cmd_main);
	m_device->cmd_begin(cmd_main);

	for (const present_target& t : m_targets) {
		if (!t.acquired) {
			continue;
		}
		const image_barrier acquire_barrier{
			.src_stages = pipeline_stage_flag::top_of_pipe,
			.src_access = {},
			.dst_stages = pipeline_stage_flag::color_attachment_output,
			.dst_access = { access_flag::color_attachment_write, access_flag::color_attachment_read },
			.discard_contents = true,
			.image = t.swapchain->image(t.image_index),
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

	++m_frame_count;
	m_frame_in_progress = true;

	return frame_token{
		.frame_index = m_current_frame,
		.image_index = m_targets.front().image_index,
	};
}

auto gse::gpu::frame::end(std::span<const queue_submission> aux_submissions, std::span<const semaphore_submit_info> extra_graphics_waits, std::span<const command_buffer_handle> graphics_buffers, std::span<const semaphore_submit_info> extra_graphics_signals) -> void {
	const auto graphics_begin = command_buffer_handle{ m_command_buffers[static_cast<std::size_t>(queue_type::graphics)] };
	{
		trace::scope_guard _{ trace_id<"end_frame::cmd_end">() };
		m_device->cmd_end(graphics_begin);
	}

	const auto graphics_end = m_device->acquire_worker_command_buffer(queue_type::graphics, 0, m_current_frame);
	m_device->cmd_begin(graphics_end);
	m_device->transient().recorder().run_post_frame(graphics_end);
	for (const present_target& t : m_targets) {
		if (!t.acquired) {
			continue;
		}
		m_device->cmd_release_swapchain_to_present(
			graphics_end,
			t.swapchain->image(t.image_index),
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
			m_device->reset_fence(m_fences.in_flight(static_cast<queue_type>(qi), m_current_frame));
		}
	}

	{
		trace::scope_guard _{ trace_id<"end_frame::submit_aux">() };
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
					last_for_queue ? m_fences.in_flight(sub.queue, m_current_frame)
								   : handle<gpu::fence>{}
				);
			}
		}
	}

	std::vector<semaphore_submit_info> main_waits;
	for (const present_target& t : m_targets) {
		if (!t.acquired) {
			continue;
		}
		main_waits.push_back({
			.semaphore = t.sync.image_available(m_current_frame),
			.value = 0,
			.stages = pipeline_stage_flag::top_of_pipe,
		});
	}

	for (const auto& w : extra_graphics_waits) {
		main_waits.push_back(w);
	}

	std::vector<semaphore_submit_info> main_signals;
	for (const present_target& t : m_targets) {
		if (!t.acquired) {
			continue;
		}
		main_signals.push_back({
			.semaphore = t.sync.render_finished(t.image_index),
			.value = 0,
			.stages = pipeline_stage_flag::bottom_of_pipe,
		});
	}
	for (const auto& s : extra_graphics_signals) {
		main_signals.push_back(s);
	}

	{
		trace::scope_guard _{ trace_id<"end_frame::submit">() };
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
			m_fences.in_flight(queue_type::graphics, m_current_frame));
	}

	for (present_target& t : m_targets) {
		if (!t.acquired) {
			continue;
		}
		const handle<gpu::semaphore> render_finished_handle = t.sync.render_finished(t.image_index);
		const std::uint64_t present_id = t.next_present_id++;
		const bool request_timing = !(t.window && t.window->attached);

		result present_result;
		{
			trace::scope_guard _{ trace_id<"end_frame::present">() };
			{
				trace::scope_guard _{ trace_id<"present::timed">() };
				present_result = t.swapchain->present(render_finished_handle, t.image_index, present_id, request_timing);
			}
			++present_total;
			if (present_result == result::error_present_timing_queue_full) {
				++present_queue_full;
				trace::scope_guard _{ trace_id<"present::retry_untimed">() };
				present_result = t.swapchain->present(render_finished_handle, t.image_index, present_id, false);
			}
			if (present_result == result::error_device_lost) {
				m_device->report_device_lost(std::format("presentKHR (frame {}, image {})", m_current_frame, t.image_index));
			}
		}

		if (present_result == result::error_surface_lost_khr) {
			(void)recreate_surface(t, "surface lost on present");
		}
		else if (present_result == result::error_out_of_date_khr || present_result == result::suboptimal_khr) {
			(void)recreate_resources(t);
			t.present_ids_in_flight.fill(0);
		}
		else if (present_result != result::success) {
			log::println(log::level::error, log::category::render, "present failed ({}), rebuilding swapchain", std::to_underlying(present_result));
			(void)recreate_resources(t);
			t.present_ids_in_flight.fill(0);
		}
		else {
			t.present_ids_in_flight[m_current_frame] = present_id;
		}
	}

	m_current_frame = (m_current_frame + 1) % max_frames_in_flight;
	m_frame_in_progress = false;
}

auto gse::gpu::frame::create_sync_objects(device& dev, const swap_chain& sc) -> swapchain_sync<device> {
	return swapchain_sync<device>::create(dev, sc.image_count());
}