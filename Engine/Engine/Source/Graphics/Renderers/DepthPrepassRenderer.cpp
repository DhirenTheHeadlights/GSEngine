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
namespace gse::renderer::depth_prepass {
	struct [[= shaders::binding<0, 0>{}]] meshlet_camera_ubo {
		using element = shaders::common::camera_data;
	};

	struct [[= shaders::binding<1, 0>{}, = shaders::ssbo_readonly]] meshlet_vertices_buffer {
		using element = shaders::forward::vertex;
	};

	struct [[= shaders::binding<1, 1>{}, = shaders::ssbo_readonly]] meshlet_meshlets_buffer {
		using element = shaders::forward::meshlet_descriptor;
	};

	struct [[= shaders::binding<1, 2>{}, = shaders::ssbo_readonly]] meshlet_vertex_indices {
		using element = std::uint32_t;
	};

	struct [[= shaders::binding<1, 3>{}, = shaders::byte_address_buffer]] meshlet_triangles {};

	struct [[= shaders::binding<1, 4>{}, = shaders::ssbo_readonly]] meshlet_bounds_buffer {
		using element = shaders::forward::meshlet_bounds;
	};

	struct [[= shaders::binding<1, 5>{}, = shaders::ssbo_readonly]] meshlet_instance_data_buffer {
		using element = shaders::common::instance_data;
	};

	using meshlet_shader_binding_types = type_pack<
		meshlet_camera_ubo,
		meshlet_vertices_buffer,
		meshlet_meshlets_buffer,
		meshlet_vertex_indices,
		meshlet_triangles,
		meshlet_bounds_buffer,
		meshlet_instance_data_buffer
	>;

	struct [[= shaders::shader_struct]] meshlet_push_constants {
		std::uint32_t meshlet_offset;
		std::uint32_t meshlet_count;
		std::uint32_t first_instance;
	};

	using meshlet_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/meshlet_depth_only">,
		gpu::layout<"meshlet_depth_only">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types>,
		gpu::bindings<meshlet_shader_binding_types>,
		gpu::helpers<"Standard3D/meshlet_common">,
		gpu::amplification_stage<"as_main">,
		gpu::mesh_stage<"ms_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<meshlet_push_constants>,
		gpu::depth<true, true, gpu::compare_op::less>,
		gpu::color_target<gpu::color_format::none>
	>;

	struct [[= shaders::binding<0, 0>{}]] skinned_camera_ubo {
		using element = shaders::common::camera_data;
	};

	struct [[= shaders::binding<1, 0>{}, = shaders::ssbo_readonly]] skinned_skin_matrices {
		using element = mat4f;
	};

	struct [[= shaders::binding<1, 1>{}, = shaders::ssbo_readonly]] skinned_instance_data_buffer {
		using element = shaders::common::instance_data;
	};

	using skinned_shader_binding_types = type_pack<
		skinned_camera_ubo,
		skinned_skin_matrices,
		skinned_instance_data_buffer
	>;

	using skinned_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/skinned_depth_only">,
		gpu::layout<"skinned_depth_only">,
		gpu::types<shaders::common::shader_types>,
		gpu::bindings<skinned_shader_binding_types>,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::depth<true, true, gpu::compare_op::less>,
		gpu::color_target<gpu::color_format::none>
	>;
}

auto gse::renderer::depth_prepass::system::run(run_context& ctx, const gpu::context::state& gpu_s, const asset::state& assets_s, resources& r) -> async::task<> {
	gpu_s.shader_registry->register_family("meshlet_depth_only", shaders::build_family_sets(meshlet_shader_binding_types{}));
	gpu_s.shader_registry->register_family("skinned_depth_only", shaders::build_family_sets(skinned_shader_binding_types{}));
	constexpr std::size_t camera_ubo_size = sizeof(shaders::common::camera_data);

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		r.camera_ubo_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = camera_ubo_size,
			.usage = gpu::buffer_flag::uniform
		});
	}

	r.meshlet_pipeline = gpu::build_graphics_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, meshlet_entry::pod);
	r.skinned_pipeline = gpu::build_graphics_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, skinned_entry::pod);

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

			const gpu::typed_push_constants<meshlet_push_constants> pc{
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
