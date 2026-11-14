export module gse.graphics:cloud_renderer;

import std;

import :atmosphere_renderer;
import :camera_system;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.meta;
import gse.math;
import gse.gpu_record;

export namespace gse::renderer::cloud {
	constexpr vec3u shape_noise_size{ 128, 128, 128 };
	constexpr vec3u detail_noise_size{ 64, 64, 64 };
	constexpr vec3u weather_map_size{ 128, 128, 32 };

	struct weather_request {
		float phase = 0.f;
	};

	enum class march_quality_level : int {
		standard = 0,
		reference = 1
	};

	struct cloud_raymarch_pass {};
	struct cloud_resolve_pass {};
	struct cloud_shadow_pass {};

	using atmosphere_length = atmosphere::atmosphere_length;
	using atmosphere_inverse_length = atmosphere::atmosphere_inverse_length;

	struct [[= shaders::shader_struct]] cloud_data {
		atmosphere_length cloud_bottom;
		atmosphere_length cloud_top;
		float cloud_coverage;
		float cloud_type;

		atmosphere_inverse_length extinction;
		float albedo;
		float multi_scatter_strength;
		float powder_strength;

		atmosphere_inverse_length shape_scale;
		atmosphere_inverse_length detail_scale;
		float detail_erosion;
		float phase_g_forward;

		float phase_g_back;
		float phase_blend;
		float ambient_strength;
		atmosphere_length max_distance;

		atmosphere_inverse_length weather_scale;
		float weather_phase;
		float weather_contrast;
		float weather_type_influence;

		float multi_scatter_extinction;

		auto operator==(
			const cloud_data&
		) const -> bool = default;
	};

	struct [[= shaders::shader_struct]] cloud_shadow_data {
		vec3f sun_direction;
		float strength;

		vec3<atmosphere_length> wind_offset;
		atmosphere_length extent_km;

		vec2<atmosphere_length> origin_km;
		atmosphere_length plane_altitude;

		auto operator==(
			const cloud_shadow_data&
		) const -> bool = default;
	};

	struct shadow_bindings {
		shaders::uniform_block<cloud_shadow_data> cloud_shadow_ubo;
	};

	struct view_snapshot {
		mat4f view_proj{};
		vec2f jitter_ndc{};
	};

	struct cloud_layer {
		per_frame_resource<gpu::image> images;
		vec2u extent{ 0, 0 };
		vec2u allocated_extent{ 0, 0 };
		bool rendered = false;
	};

	struct [[= system_state<"Cloud">{}, = settings::category<"Clouds">{}]] data {
		[[
			= settings::describe<"Render the volumetric cloud layer. Off skips the raymarch, delta and resolve "
									  "passes entirely — the largest camera-dependent GPU cost in the sky stack.">{},
			= settings::hot_reloadable
		]]
		bool enabled = true;

		[[
			= settings::describe<"Screen divisor that sets how many cloud rays are marched per frame: one ray "
									  "for every block of twice this many screen pixels on a side, so 4 marches one "
									  "ray per 8x8 pixels. Raymarch cost scales with the inverse square. Raising it "
									  "does not lower the resolution the clouds accumulate at, which the resolve "
									  "divisor sets. Every frame's rays move the whole image toward a change in "
									  "weather, sun or view, so broad changes still arrive within a few frames; "
									  "what waits longer is detail finer than the ray spacing, which each pixel "
									  "only gains from its own ray. Changing it stalls the device once to rebuild "
									  "the targets.">{},
			= settings::range<1, 8>{},
			= settings::hot_reloadable
		]]
		int target_divisor = 4;

		[[
			= settings::describe<"Screen divisor for the history the cloud rays accumulate into, which is the "
									  "resolution of the cloud image the sky draw upsamples bilinearly. 1 is full "
									  "resolution. Each history pixel gets its own ray once every (2 x raymarch "
									  "divisor / this) squared frames, 16 at the defaults, so a finer history at the "
									  "same raymarch divisor is sharper once settled and slower to refresh. Applied "
									  "as its greatest common divisor with the raymarch divisor. Changing it stalls "
									  "the device once to rebuild the targets.">{},
			= settings::range<1, 8>{},
			= settings::hot_reloadable
		]]
		int resolve_divisor = 2;

		[[
			= settings::describe<"How the view march trades accuracy for cost. Standard crosses in coarse steps "
									  "the space where the weather field and the height profile rule cloud out, "
									  "grows its steps with distance and fades detail erosion toward the horizon. "
									  "Reference marches the whole layer at the near-field step with none of those "
									  "shortcuts, at a higher cost; it exists as the "
									  "ground truth that cloud_quality_probe compares the standard march against, "
									  "passed as a per-run override, and is not meant for interactive use.">{},
			= settings::hot_reloadable
		]]
		march_quality_level march_quality = march_quality_level::standard;

		[[
			= settings::describe<"Cloud layer bottom altitude (km)">{},
			= settings::hot_reloadable
		]]
		atmosphere_length cloud_bottom = kilometers(1.5f);

		[[
			= settings::describe<"Cloud layer top altitude (km)">{},
			= settings::hot_reloadable
		]]
		atmosphere_length cloud_top = kilometers(6.0f);

		[[
			= settings::describe<"Global cloud coverage. 0 is a clear sky; raising it lowers the weather "
									  "threshold, so more of the weather field carries cloud. 1 fills every weather "
									  "cell but is not a solid overcast, because only the upper part of the shape "
									  "noise forms cloud.">{},
			= settings::range<0.f, 1.f>{},
			= shared,
			= settings::hot_reloadable
		]]
		float cloud_coverage = 0.5f;

		[[
			= settings::describe<"Cloud type bias (0 = stratus, 1 = cumulus)">{},
			= settings::range<0.f, 1.f>{},
			= shared,
			= settings::hot_reloadable
		]]
		float cloud_type = 0.8f;

		[[
			= settings::describe<"Extinction of fully dense cloud. One coefficient serves the view march, the "
									  "sun march and the ground shadow map, since all three integrate the same "
									  "medium. Real cumulus sits in the tens per kilometre; low values give haze.">{},
			= settings::range<per_kilometer(0.f), per_kilometer(120.f)>{},
			= settings::hot_reloadable
		]]
		atmosphere_inverse_length extinction = per_kilometer(40.f);

		[[
			= settings::describe<"Single-scattering albedo: the fraction of extinction that scatters rather "
									  "than absorbs. Water cloud is very close to 1.">{},
			= settings::range<0.f, 1.f>{},
			= settings::hot_reloadable
		]]
		float albedo = 0.99f;

		[[
			= settings::describe<"Weight carried into each successive multiple-scattering octave. Higher "
									  "values brighten the dense interior and soften the sun-side contrast.">{},
			= settings::range<0.f, 1.f>{},
			= settings::hot_reloadable
		]]
		float multi_scatter_strength = 0.5f;

		[[
			= settings::describe<"Fraction of optical depth each successive multiple-scattering octave still sees. "
								  "Lower values let deep octaves carry light into thick cloud, whitening dense interiors.">{},
			= settings::range<0.f, 1.f>{},
			= settings::hot_reloadable
		]]
		float multi_scatter_extinction = 0.25f;

		[[
			= settings::describe<"Darkening of thin cloud edges facing the sun, which gives the bright rims "
									  "and dark creases of piled cumulus. 0 disables it.">{},
			= settings::range<0.f, 1.f>{},
			= settings::hot_reloadable
		]]
		float powder_strength = 0.5f;

		[[
			= settings::describe<"Shape noise sampling scale (1/km). Sets the size of individual billows.">{},
			= settings::hot_reloadable
		]]
		atmosphere_inverse_length shape_scale = per_kilometer(0.3f);

		[[
			= settings::describe<"Detail noise sampling scale (1/km)">{},
			= settings::hot_reloadable
		]]
		atmosphere_inverse_length detail_scale = per_kilometer(0.6f);

		[[
			= settings::describe<"How far detail noise eats into billow edges: wispy at the base, "
									  "billowy at the top.">{},
			= settings::range<0.f, 1.f>{},
			= settings::hot_reloadable
		]]
		float detail_erosion = 0.35f;

		[[
			= settings::describe<"Henyey-Greenstein forward asymmetry">{},
			= settings::range<0.f, 1.f>{},
			= settings::hot_reloadable
		]]
		float phase_g_forward = 0.80f;

		[[
			= settings::describe<"Henyey-Greenstein back asymmetry">{},
			= settings::range<-1.f, 0.f>{},
			= settings::hot_reloadable
		]]
		float phase_g_back = -0.30f;

		[[
			= settings::describe<"Dual-lobe phase blend (0 = back only, 1 = forward only)">{},
			= settings::range<0.f, 1.f>{},
			= settings::hot_reloadable
		]]
		float phase_blend = 0.65f;

		[[
			= settings::describe<"Ambient sky contribution into clouds">{},
			= settings::range<0.f, 2.f>{},
			= settings::hot_reloadable
		]]
		float ambient_strength = 1.0f;

		[[
			= settings::describe<"Maximum cloud raymarch distance (km)">{},
			= settings::hot_reloadable
		]]
		atmosphere_length max_distance = kilometers(80.0f);

		[[
			= settings::describe<"Offset added to the world positions the cloud shape, detail and weather fields are "
									  "sampled at, which slides the whole cloud field and its ground shadows. It is a "
									  "fixed displacement, not a velocity: nothing advances it over time.">{},
			= settings::hot_reloadable
		]]
		vec3<atmosphere_length> wind_offset = { kilometers(0.0f), kilometers(0.0f), kilometers(0.0f) };

		[[
			= settings::describe<"Horizontal scale of the weather field (1/km). This is what groups cloud into "
									  "masses with clear sky between them, so it wants to be far coarser than the "
									  "shape noise: 0.015 gives roughly a 66 km pattern.">{},
			= settings::hot_reloadable
		]]
		atmosphere_inverse_length weather_scale = per_kilometer(0.015f);

		[[
			= settings::describe<"Position in the weather cycle. Cloud masses build and dissolve in place as "
									  "this advances, rather than merely drifting. Wraps at 1. Drive it from a "
									  "scenario the way sun elevation is driven, so captures stay reproducible.">{},
			= settings::range<0.f, 1.f>{},
			= shared,
			= settings::hot_reloadable
		]]
		float weather_phase = 0.0f;

		[[
			= settings::describe<"Slope of the weather field about the coverage threshold, which is 1 minus "
									  "the global coverage. Higher values push the field toward all-or-nothing, "
									  "tightening the masses and widening the clear gaps, which is what makes cloud "
									  "shadows read as moving shapes instead of even grey. 1 leaves the field as "
									  "baked only at a global coverage of 0.5.">{},
			= settings::range<0.5f, 6.f>{},
			= settings::hot_reloadable
		]]
		float weather_contrast = 2.2f;

		[[
			= settings::describe<"How far the weather field is allowed to override the global cloud type, so "
									  "flat stratus and piled cumulus can coexist in one sky. 0 keeps the single "
									  "global type everywhere.">{},
			= settings::range<0.f, 1.f>{},
			= settings::hot_reloadable
		]]
		float weather_type_influence = 0.6f;

		[[
			= settings::describe<"How strongly the cloud layer darkens direct sunlight on scene geometry. "
									  "0 disables the shadow march entirely and costs nothing.">{},
			= settings::range<0.f, 1.f>{},
			= settings::hot_reloadable
		]]
		float shadow_strength = 1.0f;

		[[
			= settings::describe<"Edge resolution of the square cloud shadow map. Cost scales with the square "
									  "of this. Cloud shadows are soft and low frequency, so 512 over the default "
									  "extent puts a texel at about 78 m, far finer than any shadow edge. The map "
									  "is refreshed a quarter at a time, one texel of every 2x2 block per frame.">{},
			= settings::range<128, 2048>{},
			= settings::hot_reloadable
		]]
		int shadow_map_resolution = 512;

		[[
			= settings::describe<"World size covered by the cloud shadow map, centred on the camera. A cloud at "
									  "6 km casts its shadow 34 km downsun at 10 degrees elevation, so low sun needs "
									  "a wide extent or the shadows fall outside the map and vanish.">{},
			= settings::hot_reloadable
		]]
		atmosphere_length shadow_extent = kilometers(40.0f);

		gpu::shader_program shape_bake_pipeline;
		gpu::shader_program detail_bake_pipeline;
		gpu::shader_program raymarch_pipeline;
		gpu::shader_program delta_pipeline;
		gpu::shader_program resolve_pipeline;
		gpu::shader_program shadow_pipeline;
		gpu::shader_program weather_bake_pipeline;

		gpu::image shape_noise;
		gpu::image detail_noise;
		gpu::image weather_map;
		gpu::image cloud_target;
		gpu::image cloud_delta;

		[[= shared]] cloud_layer layer;

		[[= shared]] gpu::image shadow_map;

		gpu::bindless_handle noise_sampler;
		gpu::bindless_handle atmosphere_lut_sampler;
		gpu::bindless_handle sky_view_sampler;
		gpu::bindless_handle resolve_sampler;
		gpu::bindless_handle depth_sampler;
		gpu::bindless_handle depth_view;

		[[= shared]] gpu::bindless_handle shadow_sampler;

		gpu::buffer cloud_ubo_buffer;

		[[= shared]] gpu::buffer shadow_ubo_buffer;

		vec2u cloud_target_allocated_extent{ 0, 0 };
		view_snapshot previous_view{};
		std::uint32_t frames_since_history_invalid = 0;
		std::uint32_t frame_counter = 0;
		int applied_target_divisor = 0;
		int applied_resolve_divisor = 0;
		int applied_shadow_resolution = 0;
		bool noises_ready = false;

		cloud_data shadow_cloud_inputs{};
		cloud_shadow_data shadow_plane_inputs{};
		std::uint32_t shadow_bake_counter = 0;
		std::uint32_t shadow_phases_pending = 0;
		bool shadow_map_filled = false;
	};

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		const context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out,
		channel_read<weather_request> weather_in,
		shared_view<atmosphere::data> atm_state,
		shared_view<camera::data> cam_state,
		channel_read<camera::temporal_reset_request> reset_in
	) -> async::task<>;
}