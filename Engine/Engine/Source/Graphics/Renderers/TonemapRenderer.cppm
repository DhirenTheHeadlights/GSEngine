export module gse.graphics:tonemap_renderer;

import std;

import :bloom_renderer;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.meta;

export namespace gse::renderer::tonemap {
	struct system {
		struct [[= gse::settings::category<"Graphics">{}]] data {
			[[= gse::settings::
				  describe<"HDR exposure multiplier applied before tonemapping. 1.0 is the neutral default.">{}]] float exposure = 1.0f;

			gpu::pipeline pipeline;
			gpu::sampler sampler;
			per_frame_resource<gpu::descriptor_region> descriptors;
		};

		static auto run(
			run_context& ctx,
			const gpu::context::data& gpu_s,
			const bloom::system::data& bloom_state,
			data& d
		) -> async::task<>;

		static auto frame(
			const frame_context& ctx,
			shared_view<gpu::context> gpu_s,
			data& d,
			shared_view<bloom::system> bloom_state
		) -> async::task<>;
	};
}
