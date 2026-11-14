module gse.graphics:sdf_grid_renderer_impl;

import std;

import :sdf_grid_renderer;
import :depth_prepass_renderer;
import :forward_renderer;
import :camera_latch_renderer;
import :render_targets;
import :sky_renderer;


import gse.os;
import gse.assets;
import gse.gpu;
import gse.gpu_record;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.meta;

namespace gse::renderer::sdf_grid {
	struct [[= shaders::shader_struct]] push_constants {
		vec3f minor_color;
		length minor_spacing;
		vec3f major_color;
		length major_spacing;
		vec3f axis_color;
		float axis_thickness;
		length fade_distance;
		float minor_thickness;
		float major_thickness;
	};

	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/SdfGrid">,
		gpu::bindings<shaders::standard_3d::camera>,
		gpu::fullscreen_pass,
		gpu::push_constant<push_constants>,
		gpu::color_targets<gpu::color_format::hdr, gpu::color_format::hdr>,
		gpu::depth<true, true, gpu::compare_op::greater_or_equal>,
		gpu::blend<gpu::blend_preset::alpha>
	>;
}

auto gse::renderer::sdf_grid::init(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	d.pipeline = gpu::build_graphics_program(*gpu_s.device, entry::pod);

	return {};
}

auto gse::renderer::sdf_grid::frame(const context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, shared_view<camera_latch::data> latch_s) -> async::task<> {
	if (!d.enabled) {
		co_return;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();
	const auto ext = gpu_s.render_graph->extent();

	auto rec = co_await gpu::pass<^^frame>(pass_out)
		.pipeline(d.pipeline)
		.color(gpu::load_color(gpu_s.render_graph->framebuffer_image<targets::hdr_color>()))
		.color(gpu::load_color(gpu_s.render_graph->framebuffer_image<targets::velocity>()))
		.depth(gpu::load_depth())
		.after<^^camera_latch::frame, ^^forward::frame, ^^sky::frame, ^^depth_prepass::frame>();

	rec.set_viewport(ext);
	rec.set_scissor(ext);
	rec.push_bindings<entry>(
		{
			.minor_color = d.minor_color,
			.minor_spacing = d.minor_spacing,
			.major_color = d.major_color,
			.major_spacing = d.major_spacing,
			.axis_color = d.axis_color,
			.axis_thickness = d.axis_thickness,
			.fade_distance = d.fade_distance,
			.minor_thickness = d.minor_thickness,
			.major_thickness = d.major_thickness,
		},
		{
			.camera_ubo = latch_s.camera_buffers[frame_index],
		}
	);
	rec.draw(3);
}