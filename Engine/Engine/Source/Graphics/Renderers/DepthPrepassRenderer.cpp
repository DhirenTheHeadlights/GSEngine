module gse.graphics:depth_prepass_renderer_impl;

import std;

import :depth_prepass_renderer;
import :geometry_collector;
import :cull_compute_renderer;
import :physics_transform_renderer;
import :camera_system;
import :render_targets;


import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;

namespace gse::renderer::depth_prepass::meshlet {
	struct [[= shaders::binding<0, 0>{}]] camera_ubo {
		using element = shaders::common::camera_data;
	};

	struct [[
		= shaders::binding<1, 5>{},
		= shaders::ssbo_readonly
	]] instance_data_buffer {
		using element = shaders::common::instance_data;
	};

	using shader_binding_types = type_pack<camera_ubo, shaders::meshlet::vertices_buffer, shaders::meshlet::meshlets_buffer, shaders::meshlet::meshlet_vertex_indices, shaders::meshlet::meshlet_triangles, shaders::meshlet::meshlet_bounds_buffer, instance_data_buffer>;

	struct [[= shaders::shader_struct]] push_constants {
		std::uint32_t meshlet_offset;
		std::uint32_t meshlet_count;
		std::uint32_t first_instance;
	};

	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/meshlet_depth_only">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"Standard3D/meshlet_common">,
		gpu::amplification_stage<"as_main">,
		gpu::mesh_stage<"ms_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<push_constants>,
		gpu::depth<true, true, gpu::compare_op::less>,
		gpu::color_targets<gpu::color_format::hdr>
	>;
}

auto gse::renderer::depth_prepass::system::init(context& ctx, const shared_view<gpu::context> gpu_s, const shared_view<asset::registry> assets_s, data& d) -> async::task<> {
	d.meshlet_pipeline = gpu::build_graphics_program(*gpu_s.device, meshlet::entry::pod);

	constexpr std::size_t camera_ubo_size = sizeof(shaders::common::camera_data);

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		d.camera_ubo_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = camera_ubo_size,
				.usage = gpu::buffer_flag::uniform,
				.bindless = true
			},
			"depth_prepass.camera_ubo"
		);
	}

	return {};
}

auto gse::renderer::depth_prepass::system::frame(context& ctx, shared_view<gpu::context> gpu_s, const data& d, shared_view<geometry_collector::system> gc_r, shared_view<camera::system> cam_state) -> async::task<> {
	const auto& render_items = ctx.read_channel<geometry_collector::render_data>();
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

	auto rec = co_await gpu::pass<system>(ctx)
		.pipeline(d.meshlet_pipeline)
		.color(
			gpu::clear_color(
				gpu::color_clear{ 0.0f, 0.0f, 0.0f, 0.0f },
				gpu_s.render_graph->framebuffer_image<targets::velocity>()
			)
		)
		.depth(gpu::clear_depth(gpu::depth_clear{ 1.0f }))
		.after<cull_compute::system, physics_transform::system>();

	rec.set_viewport(ext);
	rec.set_scissor(ext);

	if (data.normal_batches.empty()) {
		co_return;
	}

	const auto camera_slot = d.camera_ubo_buffers[frame_index].slot();
	const auto instance_slot = gc_r.instance_buffer[frame_index].slot();

	for (std::size_t i = 0; i < data.normal_batches.size(); ++i) {
		const auto& batch = data.normal_batches[i];
		const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];
		if (!mesh.has_meshlets()) {
			continue;
		}

		if (!mesh.upload_token().ready()) {
			continue;
		}

		const auto& ml = mesh.meshlet_gpu();
		const std::uint32_t meshlet_count = mesh.meshlet_count();

		rec.push_bindings<meshlet::entry>(
			{
				.meshlet_offset = 0,
				.meshlet_count = meshlet_count,
				.first_instance = batch.first_instance,
			},
			{
				.camera_ubo = camera_slot,
				.vertices_buffer = ml.vertex_storage.slot(),
				.meshlets_buffer = ml.descriptors.slot(),
				.meshlet_vertex_indices = ml.vertices.slot(),
				.meshlet_triangles = ml.triangles.slot(),
				.meshlet_bounds_buffer = ml.bounds.slot(),
				.instance_data_buffer = instance_slot,
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
