export module gse.graphics:depth_prepass_renderer;

import std;

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

export namespace gse::renderer::depth_prepass {
	struct system {
		struct resources {
			gpu::pipeline meshlet_pipeline;
			per_frame_resource<gpu::descriptor_region> meshlet_descriptors;

			gpu::pipeline skinned_pipeline;
			per_frame_resource<gpu::descriptor_region> skinned_descriptors;

			per_frame_resource<gpu::buffer> camera_ubo_buffers;
		};

		static auto run(
			run_context& ctx,
			const gpu::context::state& gpu_s,
			const asset::state& assets_s,
			resources& r
		) -> async::task<>;

		static auto frame(
			frame_context& ctx,
			const gpu::context::state& gpu_s,
			const resources& r,
			const geometry_collector::system::resources& gc_r,
			const camera::system::state& cam_state
		) -> async::task<>;
	};
}
