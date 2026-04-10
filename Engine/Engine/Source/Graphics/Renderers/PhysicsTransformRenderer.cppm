export module gse.graphics:physics_transform_renderer;

import std;

import :geometry_collector;
import gse.platform;
import gse.utility;
import gse.physics;

export namespace gse::renderer::physics_transform {
	struct state {
		resource::handle<shader> shader_handle;
		gpu::pipeline pipeline;
		per_frame_resource<gpu::descriptor_region> descriptors;
		bool initialized = false;
	};

	struct system {
		static auto initialize(const initialize_phase& phase,
			state& s
		) -> void;

		static auto render(
			const render_phase& phase,
			state& s
		) -> void;
	};
}

auto gse::renderer::physics_transform::system::initialize(const initialize_phase& phase, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	s.shader_handle = ctx.get<shader>("Shaders/Compute/physics_instance_transform");
	ctx.instantly_load(s.shader_handle);

	assert(s.shader_handle->is_compute(), std::source_location::current(), "Physics instance transform shader is not loaded as a compute shader");

	s.pipeline = gpu::create_compute_pipeline(ctx.device_ref(), *s.shader_handle, "push_constants");

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		s.descriptors[i] = gpu::allocate_descriptors(ctx.device_ref(), *s.shader_handle);
	}
}

auto gse::renderer::physics_transform::system::render(const render_phase& phase, state& s) -> void {
	const auto& ctx = phase.get<gpu::context>();

	const auto* prs = phase.try_state_of<physics::render_state>();
	if (!prs || !prs->gpu_solver.buffers_created()) {
		return;
	}

	const auto& render_items = phase.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		return;
	}

	const auto& data = render_items.front();
	if (data.physics_mapping_count == 0) {
		return;
	}

	const auto frame_index = ctx.graph().current_frame();
	const auto snapshot_slot = prs->gpu_solver.latest_snapshot_slot();

	const auto* gc = phase.try_state_of<geometry_collector::state>();
	if (!gc) {
		return;
	}

	gpu::descriptor_writer(ctx.device_ref(), s.shader_handle, s.descriptors[frame_index])
		.buffer("body_data", prs->gpu_solver.snapshot_buffer(snapshot_slot), 0, prs->gpu_solver.snapshot_buffer(snapshot_slot).size())
		.buffer("mapping_data", gc->physics_mapping_buffer[frame_index], 0, gc->physics_mapping_buffer[frame_index].size())
		.buffer("instance_data", gc->instance_buffer[frame_index], 0, gc->instance_buffer[frame_index].size())
		.commit();

	auto pc = s.shader_handle->cache_push_block("push_constants");
	pc.set("mapping_count", data.physics_mapping_count);

	const std::uint32_t workgroups = (data.physics_mapping_count + 63) / 64;

	auto pass = ctx.graph().add_pass<state>();
	pass.when(data.physics_mapping_count > 0);

	pass.track(gc->physics_mapping_buffer[frame_index]);

	pass.writes(gpu::storage_write(gc->instance_buffer[frame_index], gpu::pipeline_stage::compute_shader))
		.after<geometry_collector::state>()
		.record([&s, frame_index, workgroups, pc = std::move(pc)](gpu::recording_context& ctx) {
			ctx.bind(s.pipeline);
			ctx.bind_descriptors(s.pipeline, s.descriptors[frame_index]);
			ctx.push(s.pipeline, pc);
			ctx.dispatch(workgroups, 1, 1);
		});
}
