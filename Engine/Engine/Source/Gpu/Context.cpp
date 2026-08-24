module gse.gpu:context_impl;

import std;

import :context;
import :device;
import :swap_chain;
import :frame;
import :transient_pool;
import :render_graph;

import gse.os;
import gse.core;
import gse.concurrency;
import gse.diag;
import gse.log;
import gse.save;

auto gse::gpu::context::init(const std::optional<shared_view<window::data>> window_s, const save::registry* save_reg, data& d) -> async::task<> {
	const auto requested_backend = d.backend;
	d.device = device::create(window_s, d.validation_layers_enabled, d.backend, d.device_settings);
	if (save_reg && d.backend != requested_backend) {
		save_reg->save_now();
	}

	if (window_s) {
		d.swapchain = swap_chain::create(
			d.device->boot_surface(),
			window::viewport(*window_s),
			gse::enum_from_annotation<present_mode_setting>((*window_s).current_present_mode_index, present_mode::fifo),
			*d.device
		);
	}

	d.frame = frame::create(*d.device, d.swapchain.get());
	d.render_graph = std::make_unique<gpu::render_graph>(*d.device, *d.frame);
	if (d.swapchain) {
		d.render_graph->set_swapchain_clear(
			d.dark_background ? color_clear{ .r = 0.05f, .g = 0.05f, .b = 0.06f, .a = 1.0f } : color_clear{}
		);
	}

	return {};
}

auto gse::gpu::context::create_presentation(data& d, const window_opened& win) -> window_presentation* {
	const gpu::surface surface = d.device->create_surface(win.handle);

	auto presentation = std::make_unique<window_presentation>();
	presentation->window = win.id;
	presentation->surface = surface;
	presentation->swapchain = swap_chain::create(
		surface,
		win.size,
		gse::enum_from_annotation<present_mode_setting>(win.present_mode_index, present_mode::fifo),
		*d.device
	);
	window_presentation* raw = presentation.get();
	d.secondaries.push_back(std::move(presentation));
	return raw;
}

auto gse::gpu::context::sync_present_targets(data& d, window::data& windows) -> void {
	std::vector<gse::id> closed;
	for (const present_target& t : d.frame->targets()) {
		if (t.window_id.exists() && !window::find_surface(windows, t.window_id)) {
			closed.push_back(t.window_id);
		}
	}
	for (const gse::id window : closed) {
		d.frame->remove_present_target(window);
	}

	for (const auto& presentation : d.secondaries) {
		if (d.frame->target(presentation->window)) {
			continue;
		}
		if (window::window_surface* surface = window::find_surface(windows, presentation->window)) {
			d.frame->add_present_target(presentation->window, presentation->swapchain.get(), surface);
		}
	}
}

auto gse::gpu::context::destroy_presentation(data& d, const gse::id window) -> void {
	const auto it = std::ranges::find_if(d.secondaries, [window](const auto& held) {
		return held->window == window;
	});
	if (it == d.secondaries.end()) {
		return;
	}

	d.device->wait_idle();

	d.frame->remove_present_target(window);
	(*it)->swapchain.reset();
	d.device->destroy_surface((*it)->surface);

	d.secondaries.erase(it);
}

auto gse::gpu::context::find_presentation(data& d, const gse::id window) -> window_presentation* {
	const auto it = std::ranges::find_if(d.secondaries, [window](const auto& held) {
		return held->window == window;
	});
	return it == d.secondaries.end() ? nullptr : it->get();
}

auto gse::gpu::context::on_swap_chain_recreate(const shared_view<data> d, swap_chain_recreate_callback callback) -> void {
	d.swapchain->on_recreate(std::move(callback));
}

auto gse::gpu::context::wait_idle(const data& d) -> void {
	d.device->wait_idle();
}

auto gse::gpu::context::run(gse::context& ctx, data& d, const channel_read<gpu_resume_request, window_opened, window_closed> resume_in) -> async::task<> {
	for (const auto& req : resume_in.of<gpu_resume_request>()) {
		if (req.handle && req.out_state) {
			*req.out_state = &d;
			req.handle.resume();
		}
	}

	for (const auto& req : resume_in.of<window_opened>()) {
		(void)create_presentation(d, req);
	}

	for (const auto& req : resume_in.of<window_closed>()) {
		destroy_presentation(d, req.id);
	}

	return {};
}

auto gse::gpu::context::shutdown(data& d) -> void {
	if (!d.device) {
		return;
	}

	d.device->wait_idle();

	for (const auto& presentation : d.secondaries) {
		d.frame->remove_present_target(presentation->window);
		presentation->swapchain.reset();
		d.device->destroy_surface(presentation->surface);
	}
	d.secondaries.clear();

	d.render_graph.reset();
	d.frame.reset();
	d.swapchain.reset();
	d.device.reset();
}

auto gse::gpu::context::begin_frame(data& d, window::window_surface* window_s) -> std::expected<frame_token, frame_status> {
	d.device->collect_garbage();
	d.frame->set_present_target(window_s);

	auto result = d.frame->begin();

	return result;
}

auto gse::gpu::context::end_frame(data& d) -> void {
	auto aux_subs = d.render_graph->take_aux_submissions();
	auto graphics_waits = d.render_graph->take_graphics_extra_waits();
	auto graphics_buffers = d.render_graph->take_graphics_buffers();
	auto graphics_signals = d.render_graph->take_graphics_extra_signals();

	auto& transient_graphics = d.device->transient().queue(queue_id::graphics);
	if (const auto transient_value = transient_graphics.pending_value(); transient_value > 0) {
		graphics_waits.push_back({
			.semaphore = transient_graphics.timeline_handle(),
			.value = transient_value,
			.stages = pipeline_stage_flag::all_commands,
		});
	}

	d.frame->end(aux_subs, graphics_waits, graphics_buffers, graphics_signals);
}

