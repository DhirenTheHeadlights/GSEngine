export module gse.graphics:cull_compute_renderer;

import std;

import :geometry_collector;
import :camera_system;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

export namespace gse::renderer::cull_compute {
	struct system {
		struct data {
			bool enabled = true;

			gpu::pipeline pipeline;
			per_frame_resource<gpu::descriptor_region> normal_descriptors;
			per_frame_resource<gpu::buffer> frustum_buffer;
			per_frame_resource<gpu::buffer> batch_info_buffer;
		};

		static auto run(
			run_context& ctx,
			const gpu::context::data& gpu_s,
			const asset::data& assets_s,
			const geometry_collector::system::data& gc_r,
			data& d
		) -> async::task<>;

		static auto frame(
			frame_context& ctx,
			shared_view<gpu::context> gpu_s,
			shared_view<geometry_collector::system> gc_r,
			const data& d
		) -> async::task<>;
	};
}
