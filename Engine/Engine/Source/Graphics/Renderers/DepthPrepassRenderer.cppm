export module gse.graphics:depth_prepass_renderer;

import std;

import :geometry_collector;
import :cull_compute_renderer;
import :camera_system;

import gse.platform;
import gse.utility;

export namespace gse::renderer::depth_prepass {
	struct state {
		gpu::pipeline meshlet_pipeline;
		per_frame_resource<gpu::descriptor_region> meshlet_descriptors;
		resource::handle<shader> meshlet_shader;

		gpu::pipeline skinned_pipeline;
		per_frame_resource<gpu::descriptor_region> skinned_descriptors;
		resource::handle<shader> skinned_shader;

		std::unordered_map<std::string, per_frame_resource<gpu::buffer>> ubo_allocations;

		state() = default;
	};

	struct system {
		static auto initialize(const initialize_phase& phase,
			state& s
		) -> void;

		static auto render(
			const render_phase& phase,
			const state& s
		) -> void;
	};
}

auto gse::renderer::depth_prepass::system::initialize(const initialize_phase& phase, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	s.meshlet_shader = ctx.get<shader>("Shaders/Standard3D/meshlet_depth_only");
	ctx.instantly_load(s.meshlet_shader);

	const auto meshlet_camera_ubo = s.meshlet_shader->uniform_block("CameraUBO");
	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		s.ubo_allocations["CameraUBO"][i] = gpu::create_buffer(ctx.device_ref(), {
			.size = meshlet_camera_ubo.size,
			.usage = gpu::buffer_flag::uniform
		});
	}

	s.meshlet_pipeline = gpu::create_graphics_pipeline(ctx.device_ref(), *s.meshlet_shader, {
		.depth = { .test = true, .write = true, .compare = gpu::compare_op::less },
		.color = gpu::color_format::none,
		.push_constant_block = "push_constants"
	});


	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		s.meshlet_descriptors[i] = gpu::allocate_descriptors(ctx.device_ref(), *s.meshlet_shader);

		gpu::descriptor_writer(ctx.device_ref(), s.meshlet_shader, s.meshlet_descriptors[i])
			.buffer("CameraUBO", s.ubo_allocations["CameraUBO"][i], 0, meshlet_camera_ubo.size)
			.commit();
	}

	s.skinned_shader = ctx.get<shader>("Shaders/Standard3D/skinned_depth_only");
	ctx.instantly_load(s.skinned_shader);

	s.skinned_pipeline = gpu::create_graphics_pipeline(ctx.device_ref(), *s.skinned_shader, {
		.depth = { .test = true, .write = true, .compare = gpu::compare_op::less },
		.color = gpu::color_format::none
	});


	const auto skinned_camera_ubo = s.skinned_shader->uniform_block("CameraUBO");
	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		s.skinned_descriptors[i] = gpu::allocate_descriptors(ctx.device_ref(), *s.skinned_shader);

		gpu::descriptor_writer(ctx.device_ref(), s.skinned_shader, s.skinned_descriptors[i])
			.buffer("CameraUBO", s.ubo_allocations["CameraUBO"][i], 0, skinned_camera_ubo.size)
			.commit();
	}
}

auto gse::renderer::depth_prepass::system::render(const render_phase& phase, const state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	const auto& render_items = phase.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		return;
	}

	if (!ctx.graph().frame_in_progress()) {
		return;
	}

	const auto& data = render_items[0];
	const auto frame_index = data.frame_index;

	const auto* gc_state = phase.try_state_of<geometry_collector::state>();
	if (!gc_state) {
		return;
	}

	const auto* cam_state = phase.try_state_of<camera::state>();
	const auto view = cam_state ? cam_state->view_matrix : view_matrix{};
	const auto proj = cam_state ? cam_state->projection_matrix : projection_matrix{};

	const auto& cam_alloc = s.ubo_allocations.at("CameraUBO")[frame_index];
	s.meshlet_shader->set_uniform("CameraUBO.view", view, cam_alloc);
	s.meshlet_shader->set_uniform("CameraUBO.proj", proj, cam_alloc);
	s.skinned_shader->set_uniform("CameraUBO.view", view, cam_alloc);
	s.skinned_shader->set_uniform("CameraUBO.proj", proj, cam_alloc);

	const auto ext = ctx.graph().extent();
	const auto ext_w = ext.x();
	const auto ext_h = ext.y();

	auto meshlet_writer = gpu::create_push_writer(ctx.device_ref(), s.meshlet_shader);
	auto skinned_writer = gpu::create_push_writer(ctx.device_ref(), s.skinned_shader);

	auto pass = ctx.graph().add_pass<state>();
	pass.track(s.ubo_allocations.at("CameraUBO")[frame_index]);
	pass.track(gc_state->instance_buffer[frame_index]);

	pass.after<cull_compute::state>()
		.reads(
			gpu::storage_read(gc_state->skin_buffer[frame_index], gpu::pipeline_stage::vertex_shader),
			gpu::indirect_read(gc_state->skinned_indirect_commands_buffer[frame_index], gpu::pipeline_stage::draw_indirect)
		)
		.depth_output(gpu::depth_clear{ 1.0f })
		.record([&s, gc_state, &data, frame_index, ext_w, ext_h,
			meshlet_writer = std::move(meshlet_writer), skinned_writer = std::move(skinned_writer)](gpu::recording_context& ctx) mutable {
			const vec2u ext_size{ ext_w, ext_h };
			ctx.set_viewport(ext_size);
			ctx.set_scissor(ext_size);

			if (!data.normal_batches.empty()) {
				ctx.bind(s.meshlet_pipeline);
				ctx.bind_descriptors(s.meshlet_pipeline, s.meshlet_descriptors[frame_index]);

				const auto& instance_buf = gc_state->instance_buffer[frame_index];

				for (const auto& batch : data.normal_batches) {
					const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];
					if (!mesh.has_meshlets()) {
						continue;
					}

					meshlet_writer.begin(frame_index);
					mesh.meshlet_gpu().bind(meshlet_writer);
					meshlet_writer.buffer("instanceData", instance_buf);
					ctx.commit(meshlet_writer.native_writer(), s.meshlet_pipeline, 1);

					const std::uint32_t meshlet_count = mesh.meshlet_count();
					for (std::uint32_t inst = 0; inst < batch.instance_count; ++inst) {
						auto pc = s.meshlet_shader->cache_push_block("push_constants");
						pc.set("meshlet_offset", static_cast<std::uint32_t>(0));
						pc.set("meshlet_count", meshlet_count);
						pc.set("instance_index", batch.first_instance + inst);
						ctx.push(s.meshlet_pipeline, pc);

						const std::uint32_t task_groups = (meshlet_count + 31) / 32;
						ctx.draw_mesh_tasks(task_groups, 1, 1);
					}
				}
			}

			if (!data.skinned_batches.empty()) {
				ctx.bind(s.skinned_pipeline);
				ctx.bind_descriptors(s.skinned_pipeline, s.skinned_descriptors[frame_index]);

				const auto& skin_buf     = gc_state->skin_buffer[frame_index];
				const auto& instance_buf = gc_state->instance_buffer[frame_index];

				skinned_writer.begin(frame_index);
				skinned_writer
					.buffer("skinMatrices", skin_buf)
					.buffer("instanceData", instance_buf);
				ctx.commit(skinned_writer.native_writer(), s.skinned_pipeline, 1);

				for (std::size_t i = 0; i < data.skinned_batches.size(); ++i) {
					const auto& batch = data.skinned_batches[i];
					const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

					ctx.bind_vertex(mesh.vertex_gpu_buffer());
					ctx.bind_index(mesh.index_gpu_buffer());

					ctx.draw_indirect(
						gc_state->skinned_indirect_commands_buffer[frame_index],
						i * sizeof(gpu::draw_indexed_indirect_command),
						1,
						0
					);
				}
			}
		});
}
