export module gse.graphics:skin_compute_renderer;

import std;

import :geometry_collector;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

export namespace gse::renderer::skin_compute {
	struct state {};

	struct system {
		struct resources {
			resource::handle<shader> shader_handle;
			gpu::pipeline pipeline;
			per_frame_resource<gpu::descriptor_region> descriptors;
		};

		static auto initialize(const init_context& phase,
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

auto gse::renderer::skin_compute::system::initialize(const init_context& phase, resources& r, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	r.shader_handle = ctx.get<shader>("Shaders/Compute/skin_compute");
	ctx.instantly_load(r.shader_handle);

	assert(r.shader_handle->is_compute(), std::source_location::current(), "Skin compute shader is not loaded as a compute shader");

	const auto* gc = phase.try_resources_of<geometry_collector::system::resources>();

	r.pipeline = gpu::create_compute_pipeline(ctx, *r.shader_handle, "push_constants");

	constexpr std::size_t skin_buffer_size = geometry_collector::system::resources::max_skin_matrices * sizeof(mat4f);
	constexpr std::size_t local_pose_size = geometry_collector::system::resources::max_skin_matrices * sizeof(mat4f);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		r.descriptors[i] = gpu::allocate_descriptors(ctx, *r.shader_handle);

		gpu::descriptor_writer(ctx, r.shader_handle, r.descriptors[i])
			.buffer("skeletonData", gc->skeleton_buffer, 0, geometry_collector::system::resources::max_joints * gc->joint_stride)
			.buffer("localPoses", gc->local_pose_buffer[i], 0, local_pose_size)
			.buffer("skinMatrices", gc->skin_buffer[i], 0, skin_buffer_size)
			.commit();
	}
}

auto gse::renderer::skin_compute::system::frame(frame_context& ctx, const resources& r, const state& s, const geometry_collector::state& gc_s) -> async::task<> {
	const auto& gpu = ctx.get<gpu::context>();

	const auto& render_items = ctx.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		co_return;
	}

	const auto& data = render_items.front();
	if (data.pending_compute_instance_count == 0) {
		co_return;
	}

	const auto* gc_r = ctx.try_resources_of<geometry_collector::system::resources>();
	if (!gc_r) {
		co_return;
	}

	const auto frame_index = gpu.graph().current_frame();

	auto skin_pc = r.shader_handle->cache_push_block("push_constants");
	skin_pc.set("joint_count", gc_s.current_joint_count);
	skin_pc.set("instance_count", data.pending_compute_instance_count);
	skin_pc.set("local_pose_stride", gc_s.current_joint_count);
	skin_pc.set("skin_stride", gc_s.current_joint_count);

	auto pass = gpu.graph().add_pass<state>();
	pass.track(gc_r->skeleton_buffer);
	pass.track(gc_r->local_pose_buffer[frame_index]);
	pass.writes(gpu::storage_write(gc_r->skin_buffer[frame_index], gpu::pipeline_stage::compute_shader));

	auto& rec = co_await pass.record();
	rec.bind(r.pipeline);
	rec.bind_descriptors(r.pipeline, r.descriptors[frame_index]);
	rec.push(r.pipeline, skin_pc);
	rec.dispatch(data.pending_compute_instance_count, 1, 1);
}
