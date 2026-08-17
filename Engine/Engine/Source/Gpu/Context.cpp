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
			window::viewport(*window_s),
			gse::enum_from_annotation<present_mode_setting>((*window_s).current_present_mode_index, present_mode::fifo),
			*d.device
		);
	}

	d.frame = frame::create(*d.device, d.swapchain.get());
	d.render_graph = std::make_unique<gpu::render_graph>(*d.device, d.swapchain.get(), *d.frame);
	if (d.swapchain) {
		d.render_graph->set_swapchain_clear(
			d.dark_background ? color_clear{ .r = 0.05f, .g = 0.05f, .b = 0.06f, .a = 1.0f } : color_clear{}
		);
	}

	return {};
}

auto gse::gpu::context::on_swap_chain_recreate(const shared_view<data> d, swap_chain_recreate_callback callback) -> void {
	d.swapchain->on_recreate(std::move(callback));
}

auto gse::gpu::context::wait_idle(const data& d) -> void {
	d.device->wait_idle();
}

auto gse::gpu::context::run(gse::context& ctx, data& d, const channel_read<gpu_resume_request> resume_in) -> async::task<> {
	for (const auto& req : resume_in.of<gpu_resume_request>()) {
		if (req.handle && req.out_state) {
			*req.out_state = &d;
			req.handle.resume();
		}
	}

	return {};
}

auto gse::gpu::context::shutdown(data& d) -> void {
	if (!d.device) {
		return;
	}

	d.device->wait_idle();

	d.render_graph.reset();
	d.frame.reset();
	d.swapchain.reset();
	d.device.reset();
}

auto gse::gpu::context::begin_frame(data& d, window::data* window_s) -> std::expected<frame_token, frame_status> {
	d.device->collect_garbage();

	auto result = d.frame->begin(window_s);

	return result;
}

auto gse::gpu::context::end_frame(data& d, window::data* window_s) -> void {
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

	d.frame->end(window_s, aux_subs, graphics_waits, graphics_buffers, graphics_signals);
}

