module gse.graphics;

import std;

import :sdf_grid_renderer;
import :atmosphere_renderer;
import :forward_renderer;
import :camera_system;
import :render_targets;

import gse.os;
import gse.assets;
import gse.gpu;
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

	using sdf_grid_bindings = type_pack<shaders::standard_3d::camera_ubo>;

	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/SdfGrid">,
		gpu::types<shaders::common::shader_types>,
		gpu::bindings<sdf_grid_bindings>,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::color_targets<gpu::color_format::hdr>,
		gpu::depth<true, true, gpu::compare_op::less_or_equal>,
		gpu::blend<gpu::blend_preset::alpha>
	>;
}

auto gse::renderer::sdf_grid::system::run(run_context& ctx, const gpu::context::data& gpu_s, data& d) -> async::task<> {
	d.pipeline =
		gpu::build_graphics_program(
			*gpu_s.device,
			*gpu_s.bindless_heaps,
			entry::pod
		);

	constexpr std::size_t camera_ubo_size = sizeof(shaders::common::camera_data);

	for (std::size_t i = 0; i < per_frame_resource<gpu::bindless_buffer>::frames_in_flight; ++i) {
		d.camera_ubo_buffers[i] = gpu::bindless_buffer::create(
			gpu_s.device->allocator(),
			*gpu_s.bindless_heaps,
			{
				.size = camera_ubo_size,
				.usage = gpu::buffer_flag::uniform
			},
			"sdf_grid.camera_ubo"
		);
	}

	co_return;
}

auto gse::renderer::sdf_grid::system::frame(const frame_context& ctx, shared_view<gpu::context> gpu_s, data& d, shared_view<camera::system> cam_state) -> async::task<> {
	if (!d.enabled) {
		co_return;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();
	const auto view = cam_state.view_matrix;
	const auto proj = cam_state.projection_matrix;

	const shaders::common::camera_data camera{
		.view = view,
		.proj = proj,
		.inv_view = view.inverse(),
		.inv_view_proj = (proj * view).inverse(),
		.prev_view = cam_state.prev_view_matrix,
		.prev_proj = cam_state.prev_projection_matrix,
		.jitter_ndc = cam_state.jitter_ndc,
		.prev_jitter_ndc = cam_state.prev_jitter_ndc,
	};
	d.camera_ubo_buffers[frame_index].buffer().host_write(camera);

	const auto ext = gpu_s.render_graph->extent();

	auto rec = co_await gpu::pass<system>(ctx)
		.pipeline(d.pipeline)
		.color(gpu::load_color(gpu_s.render_graph->framebuffer_image<targets::hdr_color>()))
		.depth(gpu::load_depth())
		.after<forward::system, atmosphere::sky_raster_pass>();

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
			.camera_ubo = d.camera_ubo_buffers[frame_index].slot(),
		}
	);
	rec.draw(3);
}
