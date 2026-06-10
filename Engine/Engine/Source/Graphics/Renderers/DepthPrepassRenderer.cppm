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
import gse.math;

export namespace gse::renderer::depth_prepass {
	struct system {
		struct data {
			gpu::shader_program meshlet_pipeline;

			per_frame_resource<gpu::buffer> camera_ubo_buffers;
		};

		static auto init(
			context& ctx,
			shared_view<gpu::context> gpu_s,
			shared_view<asset::registry> assets_s,
			data& d
		) -> async::task<>;

		static auto frame(
			context& ctx,
			shared_view<gpu::context> gpu_s,
			const data& d,
			shared_view<geometry_collector::system> gc_r,
			shared_view<camera::system> cam_state
		) -> async::task<>;
	};
}
