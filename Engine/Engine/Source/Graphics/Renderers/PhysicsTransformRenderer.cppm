export module gse.graphics:physics_transform_renderer;

import std;

import :geometry_collector;
import gse.platform;
import gse.utility;
import gse.physics;

export namespace gse::renderer::physics_transform {
	struct state {};

	struct system {
		struct resources {
			resource::handle<shader> shader_handle;
			gpu::pipeline pipeline;
			bool initialized = false;
		};

		struct frame_data {
			per_frame_resource<gpu::descriptor_region> descriptors;
		};

		static auto initialize(
			const init_context& phase,
			resources& r,
			frame_data& fd
		) -> void;

		static auto frame(
			frame_context& ctx,
			const resources& r,
			frame_data& fd,
			const state& s
		) -> async::task<>;
	};
}

auto gse::renderer::physics_transform::system::initialize(const init_context& phase, resources& r, frame_data& fd) -> void {
	auto& ctx = phase.get<gpu::context>();

	r.shader_handle = ctx.get<shader>("Shaders/Compute/physics_instance_transform");
	ctx.instantly_load(r.shader_handle);

	assert(r.shader_handle->is_compute(), std::source_location::current(), "Physics instance transform shader is not loaded as a compute shader");

	r.pipeline = gpu::create_compute_pipeline(ctx.device_ref(), *r.shader_handle, "push_constants");

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		fd.descriptors[i] = gpu::allocate_descriptors(ctx.device_ref(), *r.shader_handle);
	}

	r.initialized = true;
}

auto gse::renderer::physics_transform::system::frame(frame_context& ctx, const resources& r, frame_data& fd, const state& s) -> async::task<> {
	co_await ctx.after<geometry_collector::state>();

	const auto& gpu = ctx.get<gpu::context>();

	const auto& solver_infos = ctx.read_channel<physics::gpu_solver_frame_info>();
	if (solver_infos.empty() || !solver_infos[0].solver || !solver_infos[0].solver->buffers_created()) {
		co_return;
	}
	const auto* gpu_solver = solver_infos[0].solver;

	const auto& render_items = ctx.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		co_return;
	}

	const auto& data = render_items.front();
	if (data.physics_mapping_count == 0) {
		co_return;
	}

	const auto frame_index = gpu.graph().current_frame();
	const auto snapshot_slot = gpu_solver->latest_snapshot_slot();

	const auto* gc_r = ctx.try_resources_of<geometry_collector::system::resources>();
	if (!gc_r) {
		co_return;
	}

	gpu::descriptor_writer(gpu.device_ref(), r.shader_handle, fd.descriptors[frame_index])
		.buffer("body_data", gpu_solver->snapshot_buffer(snapshot_slot), 0, gpu_solver->snapshot_buffer(snapshot_slot).size())
		.buffer("mapping_data", gc_r->physics_mapping_buffer[frame_index], 0, gc_r->physics_mapping_buffer[frame_index].size())
		.buffer("instance_data", gc_r->instance_buffer[frame_index], 0, gc_r->instance_buffer[frame_index].size())
		.commit();

	auto pc = r.shader_handle->cache_push_block("push_constants");
	pc.set("mapping_count", data.physics_mapping_count);

	const std::uint32_t workgroups = (data.physics_mapping_count + 63) / 64;

	auto pass = gpu.graph().add_pass<state>();
	pass.when(data.physics_mapping_count > 0);

	pass.track(gc_r->physics_mapping_buffer[frame_index]);

	pass.writes(gpu::storage_write(gc_r->instance_buffer[frame_index], gpu::pipeline_stage::compute_shader))
		.after<geometry_collector::state>()
		.record([&r, &fd, frame_index, workgroups, pc = std::move(pc)](gpu::recording_context& rec) {
			rec.bind(r.pipeline);
			rec.bind_descriptors(r.pipeline, fd.descriptors[frame_index]);
			rec.push(r.pipeline, pc);
			rec.dispatch(workgroups, 1, 1);
		});
}
