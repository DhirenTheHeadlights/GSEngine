export module gse.graphics:forward_renderer;

import std;

import :atmosphere_renderer;
import :geometry_collector;
import :depth_prepass_renderer;
import :gi_probe_renderer;
import :rt_shadow_renderer;
import :light_culling_renderer;
import :cull_compute_renderer;
import :camera_system;
import :texture;
import :point_light;
import :spot_light;
import :directional_light;
import :settings;

import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.save;
import gse.meta;

export namespace gse::renderer::forward {
	constexpr std::size_t max_lights = 1024;

	enum class shadow_quality_level : int {
		off = 0,
		hard = 1,
		low = 2,
		medium = 3,
		high = 4
	};

	enum class ao_quality_level : int {
		off = 0,
		low = 1,
		medium = 2,
		high = 3,
		ultra = 4
	};

	enum class reflection_quality_level : int {
		off = 0,
		low = 1,
		medium = 2,
		high = 3
	};

	struct system {
		struct [[= gse::settings::category<"Graphics">{}]] data {
			[[
				= gse::settings::describe<"Shadow map resolution and filtering quality. Off disables shadow rendering "
										  "entirely.">{}
			]]
			shadow_quality_level shadow_quality = shadow_quality_level::medium;

			[[
				= gse::settings::describe<"Screen-space ambient occlusion sample count and blur quality.">{}
			]]
			ao_quality_level ao_quality = ao_quality_level::medium;

			[[
				= gse::settings::
					describe<"Screen-space and ray-traced reflection quality. Higher levels trace more rays "
							 "per pixel.">{}
			]]
			reflection_quality_level reflection_quality = reflection_quality_level::medium;

			gpu::shader_program pipeline;

			per_frame_resource<gpu::buffer> camera_ubo_buffers;
			per_frame_resource<gpu::buffer> light_buffers;
			per_frame_resource<gpu::bindless_handle> tlas_slots;

			gpu::bindless_handle gi_sampler;

			linear_vector<std::byte> light_staging;
		};

		static auto init(
			context& ctx,
			shared_view<gpu::context> gpu_s,
			shared_view<asset::registry> assets_s,
			shared_view<rt_shadow::system> rt_state,
			shared_view<light_culling::system> lc_r,
			shared_view<atmosphere::system> atm_state,
			shared_view<gi_probe::system> gi_state,
			shared_view<geometry_collector::system> gc_state,
			data& d
		) -> async::task<>;

		static auto frame(
			context& ctx,
			shared_view<gpu::context> gpu_s,
			data& d,
			shared_view<camera::system> cam_state,
			shared_view<geometry_collector::system> gc_r,
			shared_view<light_culling::system> lc_r,
			shared_view<atmosphere::system> atm_state,
			shared_view<gi_probe::system> gi_state,
			read<directional_light_component> dir_lights,
			read<spot_light_component> spot_lights,
			read<point_light_component> point_lights
		) -> async::task<>;
	};
}
