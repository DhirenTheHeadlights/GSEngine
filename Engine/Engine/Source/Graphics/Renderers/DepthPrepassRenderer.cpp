module gse.graphics;

import std;

import :depth_prepass_renderer;
import :geometry_collector;
import :cull_compute_renderer;
import :skin_compute_renderer;
import :physics_transform_renderer;
import :camera_system;

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
import gse.shader;

auto gse::renderer::depth_prepass::system::run(run_context& ctx, const gpu::context::state& gpu_s, const asset::state& assets_s, resources& r) -> async::task<> {
	constexpr std::size_t camera_ubo_size = sizeof(shaders::common::camera_data);

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		r.camera_ubo_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = camera_ubo_size,
			.usage = gpu::buffer_flag::uniform
		});
	}

	r.meshlet_pipeline = gpu::build_graphics_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, shaders::meshlet_depth_only::entry::pod);
	r.skinned_pipeline = gpu::build_graphics_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, shaders::skinned_depth_only::entry::pod);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		r.meshlet_descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), std::string_view("meshlet_depth_only"));

		gpu::descriptor_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), std::string_view("meshlet_depth_only"), r.meshlet_descriptors[i])
			.buffer("camera_ubo", r.camera_ubo_buffers[i], 0, camera_ubo_size)
			.commit();

		r.skinned_descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), std::string_view("skinned_depth_only"));

		gpu::descriptor_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), std::string_view("skinned_depth_only"), r.skinned_descriptors[i])
			.buffer("camera_ubo", r.camera_ubo_buffers[i], 0, camera_ubo_size)
			.commit();
	}

	co_return;
}

auto gse::renderer::depth_prepass::system::frame(frame_context& ctx, const gpu::context::state& gpu_s, const resources& r, const geometry_collector::system::resources& gc_r, const camera::system::state& cam_state) -> async::task<> {
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
		.inv_view = mat4f(1.0f),
	};
	gse::memcpy(r.camera_ubo_buffers[frame_index].mapped(), camera);

	const auto ext = gpu_s.render_graph->extent();

	auto meshlet_writer = gpu::descriptor_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), gpu_s.device->descriptor_heap(), std::string_view("meshlet_depth_only"));
	auto skinned_writer = gpu::descriptor_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), gpu_s.device->descriptor_heap(), std::string_view("skinned_depth_only"));

	auto rec = co_await gpu::pass<system>(ctx)
		.depth(gpu::clear_depth(gpu::depth_clear{ 1.0f }))
		.after<cull_compute::system, physics_transform::system>()
		.reads(
			gpu::storage_read(gc_r.instance_buffer[frame_index], gpu::pipeline_stage::vertex_shader),
			gpu::storage_read(gc_r.skin_buffer[frame_index], gpu::pipeline_stage::vertex_shader),
			gpu::indirect_read(gc_r.normal_indirect_commands_buffer[frame_index], gpu::pipeline_stage::draw_indirect),
			gpu::indirect_read(gc_r.skinned_indirect_commands_buffer[frame_index], gpu::pipeline_stage::draw_indirect)
		)
		.tracks(r.camera_ubo_buffers[frame_index], gc_r.instance_buffer[frame_index]);

	rec.set_viewport(ext);
	rec.set_scissor(ext);

	if (!data.normal_batches.empty()) {
		rec.bind(r.meshlet_pipeline);
		rec.bind_descriptors(r.meshlet_pipeline, r.meshlet_descriptors[frame_index]);

		const auto& instance_buf = gc_r.instance_buffer[frame_index];

		for (std::size_t i = 0; i < data.normal_batches.size(); ++i) {
			const auto& batch = data.normal_batches[i];
			const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];
			if (!mesh.has_meshlets()) {
				continue;
			}

			if (!mesh.upload_token().ready()) {
				continue;
			}

			meshlet_writer.begin(frame_index);
			mesh.meshlet_gpu().bind(meshlet_writer);
			meshlet_writer.buffer("instance_data_buffer", instance_buf);
			rec.commit(meshlet_writer.native_writer(), r.meshlet_pipeline, 1);

			const std::uint32_t meshlet_count = mesh.meshlet_count();

			const gpu::typed_push_constants<shaders::meshlet_depth_only::push_constants> pc{
				.data = {
					.meshlet_offset = 0,
					.meshlet_count = meshlet_count,
					.first_instance = batch.first_instance,
				},
				.stages = gpu::stage_flag::task | gpu::stage_flag::mesh | gpu::stage_flag::fragment,
			};
			rec.push(r.meshlet_pipeline, pc);

			rec.draw_mesh_tasks_indirect(
				gc_r.normal_indirect_commands_buffer[frame_index],
				i * sizeof(gpu::draw_mesh_tasks_indirect_command),
				1,
				sizeof(gpu::draw_mesh_tasks_indirect_command)
			);
		}
	}

	if (!data.skinned_batches.empty()) {
		rec.bind(r.skinned_pipeline);
		rec.bind_descriptors(r.skinned_pipeline, r.skinned_descriptors[frame_index]);

		const auto& skin_buf = gc_r.skin_buffer[frame_index];
		const auto& instance_buf = gc_r.instance_buffer[frame_index];

		skinned_writer.begin(frame_index);
		skinned_writer
			.buffer("skin_matrices", skin_buf)
			.buffer("instance_data_buffer", instance_buf);
		rec.commit(skinned_writer.native_writer(), r.skinned_pipeline, 1);

		for (std::size_t i = 0; i < data.skinned_batches.size(); ++i) {
			const auto& batch = data.skinned_batches[i];
			const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

			rec.bind_vertex(mesh.vertex_gpu_buffer());
			rec.bind_index(mesh.index_gpu_buffer());

			rec.draw_indirect(
				gc_r.skinned_indirect_commands_buffer[frame_index],
				i * sizeof(gpu::draw_indexed_indirect_command),
				1,
				0
			);
		}
	}
}
