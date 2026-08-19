export module gse.graphics:atmosphere_renderer;

import std;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.meta;
import gse.math;
import gse.gpu_record;

import :camera_system;

export namespace gse::renderer::atmosphere {
	constexpr vec2u transmittance_lut_size{ 256, 64 };
	constexpr vec2u multiscatter_lut_size{ 32, 32 };
	constexpr vec2u sky_view_lut_size{ 384, 216 };

	using atmosphere_length = length_t<float, kilometers>;
	using atmosphere_inverse_length = inverse_length_t<float, per_kilometer>;

	struct transmittance_pass {};
	struct multiscatter_pass {};
	struct sky_view_pass {};
	struct sky_raster_pass {};
	struct ap_compute_pass {};

	struct [[= shaders::shader_struct]] atmosphere_data {
		vec3<atmosphere_inverse_length> rayleigh_scattering;
		atmosphere_length bottom_radius;

		vec3<atmosphere_inverse_length> ozone_absorption;
		atmosphere_length top_radius;

		atmosphere_length rayleigh_scale_height;
		atmosphere_inverse_length mie_scattering;
		atmosphere_inverse_length mie_absorption;
		atmosphere_length mie_scale_height;

		float mie_phase_g;
		atmosphere_length ozone_peak_height;
		atmosphere_length ozone_half_width;
		atmosphere_length max_view_distance;

		vec3f ground_albedo;
		vec3<irradiance> sun_irradiance;

		float star_intensity;
		float star_density;
		float star_fade;
	};

	struct [[= shaders::binding<0, 7>{}]] atmosphere_ubo {
		using element = atmosphere_data;
	};

	struct sun_request {
		angle elevation = degrees(60.f);
		angle azimuth = degrees(45.f);
	};

	struct [[= gse::system_state<"Atmosphere">{}, = gse::settings::category<"Atmosphere">{}]] data {
		[[
			= gse::settings::describe<"Render the atmosphere: scattering LUT updates and the sky draw. Off skips the "
									  "passes and freezes the last-computed sky; the sun direction stays live for "
									  "scene lighting and shadows either way.">{},
			= gse::settings::hot_reloadable
		]]
		bool enabled = true;

		[[
			= gse::settings::describe<"Sun azimuth (degrees from +X around +Y)">{},
			= gse::settings::range<0.f, 360.f>{},
			= gse::settings::hot_reloadable
		]]
		angle sun_azimuth = degrees(45.0f);

		[[
			= gse::settings::describe<"Sun elevation above horizon (degrees)">{},
			= gse::settings::range<-90.f, 90.f>{},
			= gse::settings::hot_reloadable
		]]
		angle sun_elevation = degrees(60.0f);

		[[
			= gse::settings::describe<"Sun radiant intensity (W/m^2)">{},
			= gse::shared,
			= gse::settings::hot_reloadable
		]]
		irradiance sun_intensity = watts_per_square_meter(1.6f);

		[[
			= gse::settings::describe<"Sun color tint applied to direct lighting and the sun disk">{},
			= gse::shared,
			= gse::settings::hot_reloadable
		]]
		vec3f sun_color = { 1.0f, 0.9f, 0.75f };

		[[
			= gse::settings::describe<"Reflectance of the planet surface, used where a view ray passes below the horizon "
									  "and hits the ground. Without it those rays return in-scattered haze with nothing "
									  "behind it, which reads as a bright band under any finite floor.">{},
			= gse::settings::hot_reloadable
		]]
		vec3f ground_albedo = { 0.08f, 0.08f, 0.09f };

		[[
			= gse::settings::describe<"Sun disk angular radius (degrees)">{},
			= gse::settings::range<0.1f, 5.f>{},
			= gse::settings::hot_reloadable
		]]
		angle sun_angular_radius = degrees(1.5f);

		[[
			= gse::settings::describe<"Sun ambient term applied to surfaces not directly lit by the sun.">{},
			= gse::settings::range<0.f, 1.f>{},
			= gse::shared,
			= gse::settings::hot_reloadable
		]]
		float sun_ambient_strength = 0.1f;

		[[
			= gse::settings::describe<"Sun source radius for soft-shadow penumbra calculation.">{},
			= gse::shared
		]]
		length sun_source_radius = meters(0.05f);

		[[= gse::shared]] vec3f sun_direction = { 0.0f, 1.0f, 0.0f };

		[[
			= gse::settings::describe<"Brightness of the procedural starfield. Stars are world-fixed, so they hold "
									  "still while the camera pans. 0 disables them.">{},
			= gse::settings::range<0.f, 8.f>{},
			= gse::settings::hot_reloadable
		]]
		float star_intensity = 1.6f;

		[[
			= gse::settings::describe<"Star field cell subdivision. Higher packs more, smaller stars into the sky; "
									  "the star radius scales down with it so they stay sub-pixel-stable.">{},
			= gse::settings::range<20.f, 400.f>{},
			= gse::settings::hot_reloadable
		]]
		float star_density = 140.0f;

		[[
			= gse::settings::describe<"How fast stars wash out as the sun climbs. Stars reach full brightness once "
									  "the sun is this far below the horizon, in units of sin(elevation).">{},
			= gse::settings::range<0.01f, 0.5f>{},
			= gse::settings::hot_reloadable
		]]
		float star_fade = 0.12f;

		[[
			= gse::settings::describe<"Camera altitude above sea level (km)">{},
			= gse::shared
		]]
		atmosphere_length camera_altitude = kilometers(0.0f);

		[[
			= gse::settings::describe<"Planet (ground) radius (km)">{}
		]]
		atmosphere_length bottom_radius = kilometers(6360.0f);

		[[
			= gse::settings::describe<"Top of atmosphere radius (km)">{}
		]]
		atmosphere_length top_radius = kilometers(6460.0f);

		[[
			= gse::settings::describe<"Rayleigh density scale height (km)">{}
		]]
		atmosphere_length rayleigh_scale_height = kilometers(8.0f);

		[[
			= gse::settings::describe<"Mie scattering coefficient (per km)">{}
		]]
		atmosphere_inverse_length mie_scattering = per_kilometer(3.996e-3f);

		[[
			= gse::settings::describe<"Mie absorption coefficient (per km)">{}
		]]
		atmosphere_inverse_length mie_absorption = per_kilometer(0.444e-3f);

		[[
			= gse::settings::describe<"Mie density scale height (km)">{}
		]]
		atmosphere_length mie_scale_height = kilometers(1.2f);

		[[
			= gse::settings::describe<"Mie phase asymmetry g (-1 to 1)">{},
			= gse::settings::range<-1.f, 1.f>{}
		]]
		float mie_phase_g = 0.85f;

		[[
			= gse::settings::describe<"Ozone peak altitude (km)">{}
		]]
		atmosphere_length ozone_peak_height = kilometers(25.0f);

		[[
			= gse::settings::describe<"Ozone layer half-width (km)">{}
		]]
		atmosphere_length ozone_half_width = kilometers(15.0f);

		[[
			= gse::settings::describe<"Aerial perspective max view distance (km)">{}
		]]
		atmosphere_length max_view_distance = kilometers(32.0f);

		vec3<atmosphere_inverse_length> rayleigh_scattering = {
			per_kilometer(5.802e-3f),
			per_kilometer(13.558e-3f),
			per_kilometer(33.1e-3f),
		};
		vec3<atmosphere_inverse_length> ozone_absorption = {
			per_kilometer(0.650e-3f),
			per_kilometer(1.881e-3f),
			per_kilometer(0.085e-3f),
		};

		gpu::shader_program transmittance_pipeline;
		gpu::shader_program multiscatter_pipeline;
		gpu::shader_program sky_view_pipeline;
		gpu::shader_program sky_raster_pipeline;
		gpu::shader_program ap_pipeline;

		[[= gse::shared]] gpu::image transmittance_lut;
		gpu::image multiscatter_lut;
		[[= gse::shared]] gpu::image sky_view_lut;
		[[= gse::shared]] gpu::image ap_volume;
		vec3u ap_volume_extent{ 32, 32, 32 };

		gpu::handle<gpu::sampler> lut_sampler;
		[[= gse::shared]] gpu::bindless_handle lut_sampler_bindless;
		gpu::bindless_handle sky_view_sampler_bindless;

		[[= gse::shared]] gpu::buffer atmosphere_ubo_buffer;

		bool luts_ready = false;
	};

	[[= gse::system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> async::task<>;

	[[= gse::system_frame{}]]
	auto frame(
		const context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out,
		channel_read<sun_request> sun_in,
		shared_view<camera::data> cam_state
	) -> async::task<>;
}
