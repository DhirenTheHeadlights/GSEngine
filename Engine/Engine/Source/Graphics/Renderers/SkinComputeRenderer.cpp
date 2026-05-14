module gse.graphics;

import std;

import :skin_compute_renderer;
import :geometry_collector;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.shader;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

auto gse::renderer::skin_compute::system::run(run_context& ctx, const gpu::context::state& gpu_s, const asset::state& assets_s, const geometry_collector::system::resources& gc, resources& r) -> async::task<> {
	assert(static_cast<bool>(gc.skeleton_buffer), "skin_compute::initialize: gc.skeleton_buffer is null - geometry_collector::initialize did not run before skin_compute::initialize");

	r.pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, shaders::skin_compute::entry::pod);

	constexpr std::size_t skin_buffer_size = geometry_collector::system::resources::max_skin_matrices * sizeof(mat4f);
	constexpr std::size_t local_pose_size = geometry_collector::system::resources::max_skin_matrices * sizeof(mat4f);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		r.descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), std::string_view("skin_compute"));

		gpu::descriptor_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), std::string_view("skin_compute"), r.descriptors[i])
			.buffer("skeleton_data", gc.skeleton_buffer, 0, geometry_collector::system::resources::max_joints * sizeof(shaders::skin_compute::joint_data))
			.buffer("local_poses", gc.local_pose_buffer[i], 0, local_pose_size)
			.buffer("skin_matrices", gc.skin_buffer[i], 0, skin_buffer_size)
			.commit();
	}

	co_return;
}

auto gse::renderer::skin_compute::system::frame(frame_context& ctx, const gpu::context::state& gpu_s, const resources& r, const geometry_collector::system::state& gc_s, const geometry_collector::system::resources& gc_r) -> async::task<> {
	const auto& render_items = ctx.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		co_return;
	}

	const auto& data = render_items.front();
	if (data.pending_compute_instance_count == 0) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();

	const gpu::typed_push_constants<shaders::skin_compute::push_constants> skin_pc{
		.data = {
			.joint_count = gc_s.current_joint_count,
			.instance_count = data.pending_compute_instance_count,
			.local_pose_stride = gc_s.current_joint_count,
			.skin_stride = gc_s.current_joint_count,
		},
		.stages = gpu::stage_flag::compute,
	};

	auto rec = co_await gpu::pass<system>(ctx)
		.writes(gpu::storage_write(gc_r.skin_buffer[frame_index], gpu::pipeline_stage::compute_shader))
		.tracks(gc_r.skeleton_buffer, gc_r.local_pose_buffer[frame_index]);

	rec.bind(r.pipeline);
	rec.bind_descriptors(r.pipeline, r.descriptors[frame_index]);
	rec.push(r.pipeline, skin_pc);
	rec.dispatch(data.pending_compute_instance_count, 1, 1);
}
