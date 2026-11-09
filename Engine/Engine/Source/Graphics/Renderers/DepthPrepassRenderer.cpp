module gse.graphics:depth_prepass_renderer_impl;

import std;

import :depth_prepass_renderer;
import :geometry_collector;
import :cull_compute_renderer;
import :physics_transform_renderer;
import :grass_renderer;
import :skin_renderer;
import :camera_system;
import :render_targets;


import gse.os;
import gse.assets;
import gse.gpu;
import gse.gpu_record;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;

namespace gse::renderer::depth_prepass::meshlet {
	struct prev_vertices {
		shaders::ssbo_readonly<shaders::forward::vertex> prev_vertices_buffer;
	};

	struct [[= shaders::shader_struct]] push_constants {
		std::uint32_t meshlet_offset;
		std::uint32_t meshlet_count;
		std::uint32_t first_instance;
		std::uint32_t skip_meshlet_cull;
		std::uint32_t use_prev_vertices;
	};

	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/meshlet_depth_only">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types>,
		gpu::bindings<shaders::standard_3d::camera, shaders::meshlet::vertices, shaders::meshlet::meshlets, shaders::meshlet::instances, prev_vertices>,
		gpu::helpers<"Standard3D/meshlet_common", "Standard3D/depth_velocity", "Standard3D/depth_mesh_output">,
		gpu::amplification_stage<"as_main">,
		gpu::mesh_stage<"ms_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<push_constants>,
		gpu::depth<true, true, gpu::compare_op::greater>,
		gpu::color_targets<gpu::color_format::hdr>
	>;
}

namespace gse::renderer::depth_prepass::vertex_pulled {
	struct [[= shaders::shader_struct]] push_constants {
		std::uint32_t first_instance;
		std::uint32_t use_prev_vertices;
	};

	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/vertex_depth_only">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types>,
		gpu::bindings<shaders::standard_3d::camera, shaders::meshlet::vertices, shaders::meshlet::instances, meshlet::prev_vertices>,
		gpu::helpers<"Standard3D/meshlet_common", "Standard3D/depth_velocity", "Standard3D/depth_mesh_output">,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<push_constants>,
		gpu::depth<true, true, gpu::compare_op::greater>,
		gpu::color_targets<gpu::color_format::hdr>
	>;
}

namespace gse::renderer::depth_prepass::grass_draw {
	struct [[= shaders::shader_struct]] push_constants {
		std::uint32_t view;
	};

	using entry = gpu::graphics_entry<
		gpu::body_path<"Grass/grass_depth">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types, grass::shader_types>,
		gpu::bindings<shaders::standard_3d::camera, shaders::meshlet::instances, grass::draw_bindings>,
		gpu::helpers<"Standard3D/depth_velocity", "Grass/grass_shape", "Grass/grass_geometry">,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::depth<true, true, gpu::compare_op::greater>,
		gpu::color_targets<gpu::color_format::hdr>
	>;
}

auto gse::renderer::depth_prepass::init(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<asset::data> assets_s, data& d) -> async::task<> {
	d.vertex_pulled = !gpu_s.device->capabilities().mesh_shaders;
	d.pipeline = d.vertex_pulled
		? gpu::build_graphics_program(*gpu_s.device, vertex_pulled::entry::pod)
		: gpu::build_graphics_program(*gpu_s.device, meshlet::entry::pod);
	d.grass_pipeline = gpu::build_graphics_program(*gpu_s.device, grass_draw::entry::pod);

	constexpr std::size_t camera_ubo_size = sizeof(shaders::common::camera_data);

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		d.camera_ubo_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = camera_ubo_size,
				.stride = sizeof(shaders::common::camera_data),
				.usage = gpu::buffer_flag::storage,
			},
			"depth_prepass.camera_ubo"
		);
	}

	return {};
}

auto gse::renderer::depth_prepass::frame(context& ctx, shared_view<gpu::context::data> gpu_s, const data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_read<geometry_collector::render_data> geometry_in, shared_view<geometry_collector::data> gc_r, shared_view<camera::data> cam_state, const shared_view<grass::data> grass_r) -> async::task<> {
	const auto& render_items = geometry_in.of<geometry_collector::render_data>();
	if (render_items.empty()) {
		co_return;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto& data = render_items[0];
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
	d.camera_ubo_buffers[frame_index].host_write(camera);

	const auto ext = gpu_s.render_graph->extent();

	auto rec = co_await gpu::pass<^^frame>(pass_out)
		.pipeline(d.pipeline)
		.color(
			gpu::clear_color(
				gpu::color_clear{ 0.0f, 0.0f, 0.0f, 0.0f },
				gpu_s.render_graph->framebuffer_image<targets::velocity>()
			)
		)
		.depth(gpu::clear_depth(gpu::depth_clear{ 0.0f }))
		.after<^^cull_compute::frame, ^^physics_transform::frame, ^^skin::deform_pass, ^^grass::cull_pass>();

	rec.set_viewport(ext);
	rec.set_scissor(ext);

	const auto camera_slot = d.camera_ubo_buffers[frame_index].slot();
	const auto instance_slot = gc_r.instance_buffer[frame_index].slot();

	for (std::size_t i = 0; i < data.normal_batches.size(); ++i) {
		const auto& batch = data.normal_batches[i];
		const auto& mesh = batch.key.resolve_mesh();
		if (!mesh.has_meshlets()) {
			continue;
		}

		if (!mesh.upload_token().ready()) {
			continue;
		}

		const auto& ml = mesh.meshlet_gpu();
		const std::uint32_t meshlet_count = mesh.meshlet_count();

		const auto vertices_slot = batch.deformed_vertices.valid()
			? batch.deformed_vertices
			: ml.vertex_storage.slot();
		const bool use_prev_vertices = batch.prev_deformed_vertices.valid();
		const auto prev_vertices_slot = use_prev_vertices ? batch.prev_deformed_vertices : vertices_slot;

		if (d.vertex_pulled) {
			rec.push_bindings<vertex_pulled::entry>(
				{
					.first_instance = batch.first_instance,
					.use_prev_vertices = use_prev_vertices ? 1u : 0u,
				},
				{
					.camera_ubo = camera_slot,
					.vertices_buffer = vertices_slot,
					.instance_data_buffer = instance_slot,
					.prev_vertices_buffer = prev_vertices_slot,
				}
			);
			rec.bind_index(mesh.index_gpu_buffer());
			rec.draw_indexed(static_cast<std::uint32_t>(mesh.indices().size()), batch.instance_count);
			continue;
		}

		rec.push_bindings<meshlet::entry>(
			{
				.meshlet_offset = 0,
				.meshlet_count = meshlet_count,
				.first_instance = batch.first_instance,
				.skip_meshlet_cull = batch.deformed_vertices.valid() ? 1u : 0u,
				.use_prev_vertices = use_prev_vertices ? 1u : 0u,
			},
			{
				.camera_ubo = camera_slot,
				.vertices_buffer = vertices_slot,
				.meshlets_buffer = ml.descriptors.slot(),
				.meshlet_vertex_indices = ml.vertices.slot(),
				.meshlet_triangles = ml.triangles.slot(),
				.meshlet_bounds_buffer = ml.bounds.slot(),
				.instance_data_buffer = instance_slot,
				.prev_vertices_buffer = prev_vertices_slot,
			}
		);

		rec.draw_mesh_tasks_indirect(
			gc_r.normal_indirect_commands_buffer[frame_index],
			i * sizeof(gpu::draw_mesh_tasks_indirect_command),
			1,
			sizeof(gpu::draw_mesh_tasks_indirect_command)
		);
	}

	if (std::ranges::none_of(grass_r.fields | std::views::values, &grass::field_render::culled)) {
		co_return;
	}

	rec.bind(d.grass_pipeline);
	for (const auto& field : grass_r.fields | std::views::values) {
		if (!field.culled) {
			continue;
		}
		rec.push_bindings<grass_draw::entry>(
			{
				.view = grass::views.grass_camera_view,
			},
			{
				.camera_ubo = camera_slot,
				.instance_data_buffer = field.instance[frame_index].slot(),
				.grass_params_in = field.params[frame_index],
				.grass_pool = field.pool,
				.grass_tiles = field.tiles,
				.grass_visible = field.visible.slot(),
			}
		);
		grass::record_draw(rec, grass_r.indices, field, grass::views.grass_camera_view);
	}
}