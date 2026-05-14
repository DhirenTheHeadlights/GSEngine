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
	struct system {
		struct data {
			gpu::pipeline pipeline;
			per_frame_resource<gpu::descriptor_region> descriptors;
		};

		static auto run(
			run_context& ctx,
			const gpu::context::data& gpu_s,
			const asset::data& assets_s,
			const geometry_collector::system::data& gc,
			data& d
		) -> async::task<>;

		static auto frame(
			frame_context& ctx,
			shared_view<gpu::context> gpu_s,
			const data& d,
			shared_view<geometry_collector::system> gc_r
		) -> async::task<>;
	};
}
