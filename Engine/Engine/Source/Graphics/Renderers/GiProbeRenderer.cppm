export module gse.graphics:gi_probe_renderer;

import std;

import :atmosphere_renderer;
import :camera_system;
import :geometry_collector;
import :rt_shadow_renderer;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.meta;
import gse.math;

export namespace gse::renderer::gi_probe {
	constexpr vec3u grid_dim{ 16, 6, 16 };
	constexpr std::uint32_t rays_per_probe = 64;
	constexpr std::uint32_t probe_tile_size = 8;

	enum class quality_level : int {
		off = 0,
		low = 1,
		medium = 2,
		high = 3,
	};

	struct system {
		struct [[= gse::settings::category<"Graphics">{}]] data {
			[[
				= gse::settings::describe<"Probe-based indirect diffuse global illumination. Off disables probe updates and sampling.">{},
				= gse::shared
			]]
			quality_level quality = quality_level::medium;

			[[
				= gse::settings::describe<"Spacing between probes.">{},
				= gse::settings::range<meters(0.5f), meters(8.0f)>{},
				= gse::shared
			]]
			length spacing = meters(2.0f);

			[[
				= gse::settings::describe<"Multiplier on indirect diffuse contribution from probes.">{},
				= gse::settings::range<0.0f, 4.0f>{},
				= gse::shared
			]]
			float intensity = 1.0f;

			[[
				= gse::settings::describe<"Maximum ray distance for probe updates.">{},
				= gse::settings::range<meters(5.0f), meters(200.0f)>{}
			]]
			length trace_t_max = meters(50.0f);

			gpu::shader_program update_pipeline;
			per_frame_resource<gpu::bindless_handle> tlas_views;
			std::uint32_t frame_counter = 0;

			[[= gse::shared]] gpu::image irradiance_atlas;
			[[= gse::shared]] vec3<position> origin_world{};
		};

		static auto init(
			context& ctx,
			shared_view<gpu::context> gpu_s,
			shared_view<rt_shadow::system> rt_state,
			shared_view<geometry_collector::system> gc_state,
			data& d
		) -> async::task<>;

		static auto frame(
			context& ctx,
			shared_view<gpu::context> gpu_s,
			data& d,
			shared_view<camera::system> cam_state,
			shared_view<atmosphere::system> atm_state,
			shared_view<geometry_collector::system> gc_r
		) -> async::task<>;
	};
}
