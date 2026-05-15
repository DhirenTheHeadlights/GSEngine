module gse.gpu;

import std;

import :context;
import :vulkan_device;
import :device;
import :swap_chain;
import :frame;
import :render_graph;
import :render_pass;
import :bindless;
import :shader_registry;

import gse.os;
import gse.core;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.log;

auto gse::gpu::context::run(run_context& ctx, const window::data& window_s, data& d) -> async::task<> {
	d.device = device::create(window_s, d.validation_layers_enabled, d.device_settings);
	d.shader_registry = std::make_unique<gpu::shader_registry>(*d.device);
	d.swapchain = swap_chain::create(window::viewport(window_s), *d.device);
	d.frame = frame::create(*d.device, *d.swapchain);
	d.bindless_textures = std::make_unique<bindless_texture_set>(d.device->vulkan_device(), d.device->descriptor_heap());
	d.render_graph = std::make_unique<vulkan::render_graph>(*d.device, *d.swapchain, *d.frame, d.bindless_textures.get());

	d.device->transient().recorder().pre_frame([graph = d.render_graph.get()](handle<command_buffer> cmd) {
		vulkan::transition_image_layout(
			graph->depth_image(),
			cmd,
			image_layout::general,
			image_aspect_flag::depth,
			pipeline_stage_flag::top_of_pipe,
			{},
			pipeline_stage_flag::early_fragment_tests | pipeline_stage_flag::late_fragment_tests,
			access_flag::depth_stencil_attachment_write | access_flag::depth_stencil_attachment_read
		);
	});

	while (true) {
		for (const auto& req : ctx.read_channel<gpu_resume_request>()) {
			if (req.handle && req.out_state) {
				*req.out_state = &d;
				req.handle.resume();
			}
		}
		co_await ctx.next_tick();
	}
}

auto gse::gpu::context::shutdown(shutdown_context&, data& d) -> void {
	if (!d.device) {
		return;
	}

	d.device->wait_idle();
	d.swapchain->clear_depth_image();

	d.bindless_textures.reset();
	d.render_graph.reset();
	d.frame.reset();
	d.swapchain.reset();
	d.shader_registry.reset();
	d.device.reset();
}

auto gse::gpu::context::begin_frame(data& d, window::data& window_s) -> std::expected<frame_token, frame_status> {
	auto result = d.frame->begin(window_s);

	if (result) {
		d.device->descriptor_heap().begin_frame(result->frame_index);
		d.bindless_textures->begin_frame(result->frame_index);
	}

	return result;
}

auto gse::gpu::context::end_frame(data& d, window::data& window_s) -> void {
	auto compute_subs = d.render_graph->take_compute_submissions();
	auto graphics_waits = d.render_graph->take_graphics_extra_waits();
	d.frame->end(window_s, compute_subs, graphics_waits);
}

namespace gse::gpu {
	auto to_color_output_info(
		const color_attachment& a
	) -> vulkan::color_output_info;

	auto to_depth_output_info(
		const depth_attachment& a
	) -> vulkan::depth_output_info;

	auto to_pass_data(
		render_pass_request req,
		const vulkan::render_graph& graph
	) -> vulkan::render_pass_data;
}

auto gse::gpu::to_color_output_info(const color_attachment& a) -> vulkan::color_output_info {
	auto vk_op = vulkan::load_op::dont_care;
	switch (a.op) {
		case load_op::clear:
			vk_op = vulkan::load_op::clear_color;
			break;
		case load_op::load:
			vk_op = vulkan::load_op::load;
			break;
		case load_op::dont_care:
			vk_op = vulkan::load_op::dont_care;
			break;
	}
	return {
		.is_swapchain = true,
		.custom_target = nullptr,
		.op = vk_op,
		.clear_value = a.clear,
	};
}

auto gse::gpu::to_depth_output_info(const depth_attachment& a) -> vulkan::depth_output_info {
	auto vk_op = vulkan::load_op::dont_care;
	switch (a.op) {
		case load_op::clear:
			vk_op = vulkan::load_op::clear_depth;
			break;
		case load_op::load:
			vk_op = vulkan::load_op::load;
			break;
		case load_op::dont_care:
			vk_op = vulkan::load_op::dont_care;
			break;
	}
	return {
		.op = vk_op,
		.clear_value = a.clear,
	};
}

auto gse::gpu::to_pass_data(render_pass_request req, const vulkan::render_graph& graph) -> vulkan::render_pass_data {
	vulkan::render_pass_data p{
		.pass_type = req.desc.pass_kind,
		.queue = req.desc.queue,
		.reads = std::move(req.desc.reads),
		.writes = std::move(req.desc.writes),
		.tracked_buffers = std::move(req.desc.tracked_buffers),
		.after_passes = std::move(req.desc.after_deps),
		.record_handle = req.record_handle,
		.record_ctx_slot = req.record_ctx_slot,
	};

	if (req.desc.color) {
		p.color_output = to_color_output_info(*req.desc.color);
	}

	if (req.desc.depth) {
		p.depth_output = to_depth_output_info(*req.desc.depth);
		if (req.desc.depth->op == load_op::load) {
			p.reads.push_back({
				.resource = {
					.ptr = std::addressof(graph.depth_image()),
					.type = vulkan::resource_type::image,
				},
				.stage = pipeline_stage_flag::early_fragment_tests,
				.access = access_flag::depth_stencil_attachment_read,
			});
		}
		else {
			p.writes.push_back(vulkan::attachment(graph.depth_image(), pipeline_stage_flag::late_fragment_tests));
		}
	}

	return p;
}

auto gse::gpu::context::execute_frame(data& d, std::vector<render_pass_request> requests) -> void {
	std::vector<vulkan::render_pass_data> passes;
	passes.reserve(requests.size());
	for (auto& req : requests) {
		passes.push_back(to_pass_data(std::move(req), *d.render_graph));
	}
	d.render_graph->execute(std::move(passes));
}

auto gse::gpu::context::on_swap_chain_recreate(const data& d, swap_chain_recreate_callback callback) -> void {
	d.swapchain->on_recreate(std::move(callback));
}

auto gse::gpu::context::wait_idle(const data& d) -> void {
	d.device->wait_idle();
}

auto gse::gpu::context::device_handle(const gpu::device& device) -> handle<vulkan::device> {
	return device.vulkan_device().device_handle();
}
