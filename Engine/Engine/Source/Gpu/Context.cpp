module gse.gpu:context_impl;

import std;

import :context;
import :device;
import :swap_chain;
import :frame;
import :transient_pool;
import :render_graph;
import :render_pass;

import gse.os;
import gse.core;
import gse.concurrency;
import gse.diag;
import gse.log;

auto gse::gpu::context::init(const std::optional<shared_view<window::data>> window_s, data& d) -> async::task<> {
	d.device = device::create(window_s, d.validation_layers_enabled, d.device_settings);

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
		d.render_graph->set_swapchain_clear(d.swapchain_clear);
	}

	return {};
}

auto gse::gpu::context::on_swap_chain_recreate(const shared_view<data> d, swap_chain_recreate_callback callback) -> void {
	d.swapchain->on_recreate(std::move(callback));
}

auto gse::gpu::context::wait_idle(const data& d) -> void {
	d.device->wait_idle();
}

auto gse::gpu::context::run(gse::context& ctx, data& d) -> async::task<> {
	for (const auto& req : ctx.read_channel<gpu_resume_request>()) {
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
	d.frame->end(window_s, aux_subs, graphics_waits);
}

namespace gse::gpu {
	auto to_color_output_info(
		const color_attachment& a
	) -> gpu::color_output_info;

	auto to_depth_output_info(
		const depth_attachment& a
	) -> gpu::depth_output_info;

	auto to_pass_data(
		render_pass_request req
	) -> gpu::render_pass_data;
}

auto gse::gpu::to_color_output_info(const color_attachment& a) -> gpu::color_output_info {
	return {
		.is_swapchain = a.target == nullptr && !a.transient_target,
		.custom_target = a.target,
		.transient_target = a.transient_target,
		.op = a.op,
		.clear_value = a.clear,
	};
}

auto gse::gpu::to_depth_output_info(const depth_attachment& a) -> gpu::depth_output_info {
	return {
		.is_swapchain = a.target == nullptr && !a.transient_target,
		.custom_target = a.target,
		.transient_target = a.transient_target,
		.op = a.op,
		.clear_value = a.clear,
	};
}

auto gse::gpu::to_pass_data(render_pass_request req) -> gpu::render_pass_data {
	gpu::render_pass_data p{
		.pass_type = req.desc.pass_kind,
		.queue = req.desc.queue,
		.primary_pipeline = req.desc.primary_pipeline,
		.after_passes = std::move(req.desc.after_deps),
		.chain_id = req.desc.chain_id,
		.record_handle = req.record_handle,
		.record_ctx_slot = req.record_ctx_slot,
	};

	p.color_outputs.reserve(req.desc.colors.size());
	for (const auto& c : req.desc.colors) {
		p.color_outputs.push_back(to_color_output_info(c));
	}

	if (req.desc.depth) {
		p.depth_output = to_depth_output_info(*req.desc.depth);
	}

	return p;
}

auto gse::gpu::context::execute_frame(data& d, scheduler& s) -> void {
	d.render_graph->execute(frame_request_drain{
		.drain_passes = [&s] {
			auto requests = s.drain_channel<render_pass_request>();
			std::vector<render_pass_data> passes;
			passes.reserve(requests.size());
			for (auto& req : requests) {
				passes.push_back(to_pass_data(std::move(req)));
			}
			return passes;
		},
		.drain_images = [&s] {
			return s.drain_channel<transient_image_request>();
		},
		.drain_buffers = [&s] {
			return s.drain_channel<transient_buffer_request>();
		},
	});
}

