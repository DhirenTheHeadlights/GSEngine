module gse.graphics:depth_prepass_renderer_impl;

import std;

import :depth_prepass_renderer;
import :geometry_collector;
import :cull_compute_renderer;
import :physics_transform_renderer;
import :skin_renderer;
import :camera_latch_renderer;
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

namespace gse::renderer::depth_prepass {
	using depth_pass = gpu::specs<
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types>,
		gpu::helpers<"Standard3D/meshlet_common", "Standard3D/depth_velocity", "Standard3D/depth_mesh_output">,
		gpu::depth<true, true, gpu::compare_op::greater>,
		gpu::color_targets<gpu::color_format::hdr>
	>;
}

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
		gpu::bindings<shaders::standard_3d::camera, shaders::meshlet::vertices, shaders::meshlet::meshlets, shaders::meshlet::instances, prev_vertices>,
		gpu::amplification_stage<>,
		gpu::mesh_stage<>,
		gpu::fragment_stage<>,
		gpu::push_constant<push_constants>,
		depth_pass
	>;
}

namespace gse::renderer::depth_prepass::vertex_pulled {
	struct [[= shaders::shader_struct]] push_constants {
		std::uint32_t first_instance;
		std::uint32_t use_prev_vertices;
	};

	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/vertex_depth_only">,
		gpu::bindings<shaders::standard_3d::camera, shaders::meshlet::vertices, shaders::meshlet::instances, meshlet::prev_vertices>,
		gpu::vertex_stage<>,
		gpu::fragment_stage<>,
		gpu::push_constant<push_constants>,
		depth_pass
	>;
}

auto gse::renderer::depth_prepass::init(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<asset::data> assets_s, data& d) -> async::task<> {
	d.vertex_pulled = !gpu_s.device->capabilities().mesh_shaders;
	d.pipeline = d.vertex_pulled
		? gpu::build_graphics_program(*gpu_s.device, vertex_pulled::entry::pod)
		: gpu::build_graphics_program(*gpu_s.device, meshlet::entry::pod);

	return {};
}

auto gse::renderer::depth_prepass::frame(context& ctx, shared_view<gpu::context::data> gpu_s, const data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_read<geometry_collector::render_data> geometry_in, shared_view<geometry_collector::data> gc_r, shared_view<camera_latch::data> latch_s) -> async::task<> {
	const auto& render_items = geometry_in.of<geometry_collector::render_data>();
	if (render_items.empty()) {
		co_return;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto& data = render_items[0];
	const auto frame_index = gpu_s.render_graph->current_frame();

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
		.after<^^camera_latch::frame, ^^cull_compute::frame, ^^physics_transform::frame, ^^skin::deform_pass>();

	rec.set_viewport(ext);
	rec.set_scissor(ext);

	const auto camera_slot = latch_s.camera_buffers[frame_index].slot();
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
				.meshlets_buffer = ml.descriptors,
				.meshlet_vertex_indices = ml.vertices,
				.meshlet_triangles = ml.triangles,
				.meshlet_bounds_buffer = ml.bounds,
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
}