export module gse.graphics:bloom_renderer;

import std;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.meta;
import gse.math;

export namespace gse::renderer::bloom {
	constexpr std::uint32_t max_mip_count = 7;
	constexpr std::uint32_t min_mip_extent = 8;

	enum class quality_level : int {
		off = 0,
		low = 1,
		medium = 2,
		high = 3,
	};

	struct downsample_pass {};
	struct upsample_pass {};

	struct system {
		struct [[= gse::settings::category<"Graphics">{}]] data {
			[[
				= gse::settings::describe<"Bloom mip count: low=4, medium=6, high=7. Off disables bloom entirely.">{},
				= gse::shared
			]]
			quality_level bloom_quality = quality_level::medium;

			[[
				= gse::settings::
					describe<"Bloom intensity. Additive scale applied when compositing bloom into the HDR target.">{},
				= gse::shared
			]]
			float bloom_intensity = 0.04f;

			[[
				= gse::settings::describe<
					"Upsample tent filter radius in source-texel units. 1.0 is the canonical Sledgehammer value."
				>{}
			]]
			float bloom_radius = 1.0f;

			gpu::pipeline downsample_pipeline;
			gpu::pipeline upsample_pipeline;
			gpu::sampler bloom_sampler;

			std::array<gpu::image, max_mip_count> mips_down;
			[[= gse::shared]] std::array<gpu::image, max_mip_count> mips_up;
			std::array<vec2u, max_mip_count> mip_extents{};
			[[= gse::shared]] std::uint32_t active_mip_count = 0;

			std::array<per_frame_resource<gpu::descriptor_region>, max_mip_count> downsample_descriptors;
			std::array<per_frame_resource<gpu::descriptor_region>, max_mip_count> upsample_descriptors;
		};

		static auto run(run_context& ctx, const gpu::context::data& gpu_s, data& d) -> async::task<>;

		static auto frame(const frame_context& ctx, shared_view<gpu::context> gpu_s, data& d) -> async::task<>;
	};
}
