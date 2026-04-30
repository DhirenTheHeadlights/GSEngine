export module gse.graphics:depth_prepass_renderer;

import std;

import :geometry_collector;
import :cull_compute_renderer;
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
export namespace gse::renderer::depth_prepass {
	struct state {};

	struct system {
		struct resources {
			gpu::pipeline meshlet_pipeline;
			per_frame_resource<gpu::descriptor_region> meshlet_descriptors;
			resource::handle<shader> meshlet_shader;

			gpu::pipeline skinned_pipeline;
			per_frame_resource<gpu::descriptor_region> skinned_descriptors;
			resource::handle<shader> skinned_shader;

			std::unordered_map<std::string, per_frame_resource<gpu::buffer>> ubo_allocations;
		};

		static auto initialize(
			const init_context& phase,
			resources& r,
			state& s
		) -> void;

		static auto frame(
			frame_context& ctx,
			const resources& r,
			const state& s,
			const geometry_collector::state& gc_s
		) -> async::task<>;
	};
}

auto gse::renderer::depth_prepass::system::initialize(const init_context& phase, resources& r, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();
	auto& assets = phase.assets<gpu::context>();

	r.meshlet_shader = assets.get<shader>("Shaders/Standard3D/meshlet_depth_only");
	assets.instantly_load(r.meshlet_shader);

	const auto meshlet_camera_ubo = r.meshlet_shader->uniform_block("CameraUBO");
	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		r.ubo_allocations["CameraUBO"][i] = gpu::buffer::create(ctx.allocator(), {
			.size = meshlet_camera_ubo.size,
			.usage = gpu::buffer_flag::uniform
		});
	}

	r.meshlet_pipeline = gpu::create_graphics_pipeline(ctx.device(), ctx.shader_registry(), ctx.bindless_textures(), r.meshlet_shader, {
		.depth = { .test = true, .write = true, .compare = gpu::compare_op::less },
		.color = gpu::color_format::none,
		.push_constant_block = "push_constants"
	});


	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		r.meshlet_descriptors[i] = gpu::allocate_descriptors(ctx.shader_registry(), ctx.descriptor_heap(), r.meshlet_shader);

		gpu::descriptor_writer(ctx.shader_registry(), ctx.device_handle(), r.meshlet_shader, r.meshlet_descriptors[i])
			.buffer("CameraUBO", r.ubo_allocations["CameraUBO"][i], 0, meshlet_camera_ubo.size)
			.commit();
	}

	r.skinned_shader = assets.get<shader>("Shaders/Standard3D/skinned_depth_only");
	assets.instantly_load(r.skinned_shader);

	r.skinned_pipeline = gpu::create_graphics_pipeline(ctx.device(), ctx.shader_registry(), ctx.bindless_textures(), r.skinned_shader, {
		.depth = { .test = true, .write = true, .compare = gpu::compare_op::less },
		.color = gpu::color_format::none
	});


	const auto skinned_camera_ubo = r.skinned_shader->uniform_block("CameraUBO");
	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		r.skinned_descriptors[i] = gpu::allocate_descriptors(ctx.shader_registry(), ctx.descriptor_heap(), r.skinned_shader);

		gpu::descriptor_writer(ctx.shader_registry(), ctx.device_handle(), r.skinned_shader, r.skinned_descriptors[i])
			.buffer("CameraUBO", r.ubo_allocations["CameraUBO"][i], 0, skinned_camera_ubo.size)
			.commit();
	}
}

auto gse::renderer::depth_prepass::system::frame(frame_context& ctx, const resources& r, const state& s, const geometry_collector::state& gc_s) -> async::task<> {

	auto& gpu = ctx.get<gpu::context>();

	const auto& render_items = ctx.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		co_return;
	}

	if (!gpu.graph().frame_in_progress()) {
		co_return;
	}

	const auto& data = render_items[0];
	const auto frame_index = gpu.graph().current_frame();

	const auto* gc_r = ctx.try_resources_of<geometry_collector::system::resources>();
	if (!gc_r) {
		co_return;
	}

	const auto* cam_state = ctx.try_state_of<camera::state>();
	const auto view = cam_state ? cam_state->view_matrix : view_matrix{};
	const auto proj = cam_state ? cam_state->projection_matrix : projection_matrix{};

	const auto& cam_alloc = r.ubo_allocations.at("CameraUBO")[frame_index];
	r.meshlet_shader->set_uniform(cam_alloc.bytes(), "CameraUBO.view", view);
	r.meshlet_shader->set_uniform(cam_alloc.bytes(), "CameraUBO.proj", proj);
	r.skinned_shader->set_uniform(cam_alloc.bytes(), "CameraUBO.view", view);
	r.skinned_shader->set_uniform(cam_alloc.bytes(), "CameraUBO.proj", proj);

	const auto ext = gpu.graph().extent();

	auto meshlet_writer = gpu::descriptor_writer(gpu.shader_registry(), gpu.device_handle(), gpu.descriptor_heap(), r.meshlet_shader);
	auto skinned_writer = gpu::descriptor_writer(gpu.shader_registry(), gpu.device_handle(), gpu.descriptor_heap(), r.skinned_shader);

	auto pass = gpu.graph().add_pass<state>();
	pass.track(r.ubo_allocations.at("CameraUBO")[frame_index]);
	pass.track(gc_r->instance_buffer[frame_index]);

	pass.after<cull_compute::state>()
		.after<physics_transform::state>()
		.reads(
			gpu::storage_read(gc_r->instance_buffer[frame_index], gpu::pipeline_stage::vertex_shader),
			gpu::storage_read(gc_r->skin_buffer[frame_index], gpu::pipeline_stage::vertex_shader),
			gpu::indirect_read(gc_r->skinned_indirect_commands_buffer[frame_index], gpu::pipeline_stage::draw_indirect)
		)
		.depth_output(gpu::depth_clear{ 1.0f });

	auto& rec = co_await pass.record();

	rec.set_viewport(ext);
	rec.set_scissor(ext);

	if (!data.normal_batches.empty()) {
		rec.bind(r.meshlet_pipeline);
		rec.bind_descriptors(r.meshlet_pipeline, r.meshlet_descriptors[frame_index]);

		const auto& instance_buf = gc_r->instance_buffer[frame_index];

		for (const auto& batch : data.normal_batches) {
			const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];
			if (!mesh.has_meshlets()) {
				continue;
			}

			meshlet_writer.begin(frame_index);
			mesh.meshlet_gpu().bind(meshlet_writer);
			meshlet_writer.buffer("instanceData", instance_buf);
			rec.commit(meshlet_writer.native_writer(), r.meshlet_pipeline, 1);

			const std::uint32_t meshlet_count = mesh.meshlet_count();
			const std::uint32_t task_groups = (meshlet_count + 31) / 32;

			auto pc = gpu::cache_push_block(r.meshlet_shader, "push_constants");
			pc.set("meshlet_offset", static_cast<std::uint32_t>(0));
			pc.set("meshlet_count", meshlet_count);
			pc.set("first_instance", batch.first_instance);
			rec.push(r.meshlet_pipeline, pc);

			rec.draw_mesh_tasks(task_groups, batch.instance_count, 1);
		}
	}

	if (!data.skinned_batches.empty()) {
		rec.bind(r.skinned_pipeline);
		rec.bind_descriptors(r.skinned_pipeline, r.skinned_descriptors[frame_index]);

		const auto& skin_buf = gc_r->skin_buffer[frame_index];
		const auto& instance_buf = gc_r->instance_buffer[frame_index];

		skinned_writer.begin(frame_index);
		skinned_writer
			.buffer("skinMatrices", skin_buf)
			.buffer("instanceData", instance_buf);
		rec.commit(skinned_writer.native_writer(), r.skinned_pipeline, 1);

		for (std::size_t i = 0; i < data.skinned_batches.size(); ++i) {
			const auto& batch = data.skinned_batches[i];
			const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

			rec.bind_vertex(mesh.vertex_gpu_buffer());
			rec.bind_index(mesh.index_gpu_buffer());

			rec.draw_indirect(
				gc_r->skinned_indirect_commands_buffer[frame_index],
				i * sizeof(gpu::draw_indexed_indirect_command),
				1,
				0
			);
		}
	}
}
