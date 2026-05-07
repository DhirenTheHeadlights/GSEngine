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
		struct resources {
			resource::handle<shader> shader_handle;
			gpu::pipeline pipeline;
			per_frame_resource<gpu::descriptor_region> descriptors;
		};

		static auto run(
			run_context& ctx,
			const gpu::context::state& gpu_s,
			const asset::state& assets_s,
			const geometry_collector::system::resources& gc,
			resources& r
		) -> async::task<>;

		static auto frame(
			frame_context& ctx,
			const gpu::context::state& gpu_s,
			const resources& r,
			const geometry_collector::system::state& gc_s,
			const geometry_collector::system::resources& gc_r
		) -> async::task<>;
	};
}
