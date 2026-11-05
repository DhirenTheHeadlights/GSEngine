export module gse.graphics:forward_renderer;

import gse.assets;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;
import gse.math;
import gse.meta;
import gse.os;
import gse.physics;
import gse.save;
import gse.time;
import std;

import :atmosphere_renderer;
import :camera_system;
import :cloud_renderer;
import :directional_light;
import :geometry_collector;
import :gi_probe_renderer;
import :light_culling_renderer;
import :lighting_source;
import :point_light;
import :spot_light;
import :texture;

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

	struct [[= system_state<"Forward">{}, = settings::category<"Graphics">{}]] data {
		[[
			= settings::describe<"Shadow map resolution and filtering quality. Off disables shadow rendering "
									  "entirely.">{}
		]]
		[[= shared]] shadow_quality_level shadow_quality = shadow_quality_level::medium;

		[[
			= settings::describe<"Trace a shadow ray only for lights whose attenuated irradiance at the surface "
									  "reaches this. Dimmer lights still light the surface, they are just left "
									  "unshadowed, so the error is bounded by their own contribution. The trace runs "
									  "once per light per pixel and dominates scenes with many lights. Zero shadows "
									  "every light, which is the behaviour this setting replaced.">{},
			= settings::range<watts_per_square_meter(0.f), watts_per_square_meter(1.f)>{}
		]]
		irradiance shadow_cutoff = watts_per_square_meter(0.1f);

		[[
			= settings::describe<"Screen-space ambient occlusion sample count and blur quality.">{}
		]]
		[[= shared]] ao_quality_level ao_quality = ao_quality_level::medium;

		[[
			= settings::
				describe<"Screen-space and ray-traced reflection quality. Higher levels trace more rays "
						 "per pixel.">{}
		]]
		[[= shared]] reflection_quality_level reflection_quality = reflection_quality_level::medium;

		gpu::shader_program pipeline;
		bool vertex_pulled = false;

		per_frame_resource<gpu::buffer> camera_ubo_buffers;
		per_frame_resource<gpu::buffer> light_buffers;

		std::optional<lighting_source::ray_traced_scene> ray_traced_scene;
		std::optional<lighting_source::raster_shadows> raster_shadows;
		std::optional<lighting_source::screen_ambient_occlusion> ambient_occlusion;
		std::optional<lighting_source::screen_reflections> reflections;

		gpu::bindless_handle gi_sampler;
		gpu::bindless_handle material_sampler;
		gpu::image material_fallback;

		linear_vector<std::byte> light_staging;
		std::uint32_t frame_counter = 0;
	};

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<asset::data> assets_s,
		shared_view<lighting::data> lighting_s,
		shared_view<light_culling::data> lc_r,
		shared_view<atmosphere::data> atm_state,
		shared_view<geometry_collector::data> gc_state,
		data& d
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out,
		channel_read<geometry_collector::render_data> geometry_in,
		shared_view<camera::data> cam_state,
		shared_view<geometry_collector::data> gc_r,
		shared_view<light_culling::data> lc_r,
		shared_view<atmosphere::data> atm_state,
		shared_view<cloud::data> cloud_state,
		std::optional<shared_view<gi_probe::data>> gi_s,
		read<directional_light_component> dir_lights,
		read<spot_light_component> spot_lights,
		read<point_light_component> point_lights,
		read<physics::transform_component> transforms,
		channel_read<camera::temporal_reset_request> reset_in,
		shared_view<lighting::data> lighting_s,
		channel_read<lighting_source::ray_traced_scene, lighting_source::raster_shadows, lighting_source::screen_ambient_occlusion, lighting_source::screen_reflections> lighting_in
	) -> async::task<>;
}