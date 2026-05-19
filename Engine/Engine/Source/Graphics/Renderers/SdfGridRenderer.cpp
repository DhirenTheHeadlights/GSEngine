module gse.graphics;

import std;

import :sdf_grid_renderer;
import :forward_renderer;
import :camera_system;

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
	struct[[= shaders::shader_struct]] push_constants {
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
		gpu::layout<"standard_3d">,
		gpu::types<shaders::common::shader_types>,
		gpu::bindings<shaders::standard_3d::shader_binding_types>,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::depth<true, true, gpu::compare_op::less_or_equal>,
		gpu::blend<gpu::blend_preset::alpha>>;
}

auto gse::renderer::sdf_grid::system::run(run_context& ctx, const gpu::context::data& gpu_s, data& d) -> async::task<> {
	d.pipeline =
		gpu::build_graphics_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, entry::pod);

	constexpr std::size_t camera_ubo_size = sizeof(shaders::common::camera_data);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		d.camera_ubo_buffers[i] = gpu::buffer::create(
			gpu_s.device->allocator(),
			{ .size = camera_ubo_size, .usage = gpu::buffer_flag::uniform }
		);
		d.descriptors[i] =
			gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), entry::pod);

		gpu::descriptor_writer(gpu::context::device_handle(*gpu_s.device), d.descriptors[i])
			.buffer<shaders::standard_3d::camera_ubo>(d.camera_ubo_buffers[i], 0, camera_ubo_size)
			.commit();
	}

	co_return;
}

auto gse::renderer::sdf_grid::system::frame(
	const frame_context& ctx,
	shared_view<gpu::context> gpu_s,
	data& d,
	shared_view<camera::system> cam_state
) -> async::task<> {
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
	};
	d.camera_ubo_buffers[frame_index].host_write(camera);

	const auto ext = gpu_s.render_graph->extent();

	const gpu::typed_push_constants<push_constants> pc{
		.data = {
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
		.stages = gpu::stage_flag::vertex | gpu::stage_flag::fragment,
	};

	auto rec = co_await gpu::pass<system>(ctx)
				   .pipeline(d.pipeline)
				   .color(gpu::load_color())
				   .depth(gpu::load_depth())
				   .after<forward::system>();

	rec.set_viewport(ext);
	rec.set_scissor(ext);
	rec.bind_descriptors(d.pipeline, d.descriptors[frame_index]);
	rec.push(d.pipeline, pc);
	rec.draw(3);
}
