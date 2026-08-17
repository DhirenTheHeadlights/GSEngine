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
	constexpr vec3u detail_noise_size{ 32, 32, 32 };

	struct cloud_raymarch_pass {};
	struct cloud_composite_pass {};

	using atmosphere_length = atmosphere::atmosphere_length;
	using atmosphere_inverse_length = atmosphere::atmosphere_inverse_length;

	struct [[= shaders::shader_struct]] cloud_data {
		atmosphere_length cloud_bottom;
		atmosphere_length cloud_top;
		float cloud_coverage;
		float cloud_type;

		float density_multiplier;
		float view_extinction;
		float light_extinction;
		atmosphere_inverse_length shape_scale;

		atmosphere_inverse_length detail_scale;
		float detail_strength;
		float phase_g_forward;
		float phase_g_back;

		float phase_blend;
		float ambient_strength;
		atmosphere_length max_distance;
	};

	struct [[= shaders::binding<0, 5>{}]] cloud_ubo {
		using element = cloud_data;
	};

	struct [[= gse::system_state<"Cloud">{}, = gse::settings::category<"Clouds">{}]] data {
		[[
			= gse::settings::describe<"Render the volumetric cloud layer. Off skips the raymarch and composite "
									  "passes entirely — the largest camera-dependent GPU cost in the sky stack.">{},
			= gse::settings::hot_reloadable
		]]
		bool enabled = true;

		[[
			= gse::settings::describe<"Screen divisor for the raymarch target. Cost scales with the square of this: "
									  "2 is quarter the screen pixels, 4 is a sixteenth. Raising it blurs cloud "
									  "edges against geometry and coarsens the per-pixel jitter into visible "
									  "blocks, since the composite is a plain bilinear upsample. Changing it "
									  "stalls the device once to rebuild the target.">{},
			= gse::settings::range<1, 8>{},
			= gse::settings::hot_reloadable
		]]
		int target_divisor = 2;

		[[
			= gse::settings::describe<"Cloud layer bottom altitude (km)">{},
			= gse::settings::hot_reloadable
		]]
		atmosphere_length cloud_bottom = kilometers(1.5f);

		[[
			= gse::settings::describe<"Cloud layer top altitude (km)">{},
			= gse::settings::hot_reloadable
		]]
		atmosphere_length cloud_top = kilometers(6.0f);

		[[
			= gse::settings::describe<"Global cloud coverage (0 = clear sky, 1 = overcast)">{},
			= gse::settings::range<0.f, 1.f>{},
			= gse::shared,
			= gse::settings::hot_reloadable
		]]
		float cloud_coverage = 0.55f;

		[[
			= gse::settings::describe<"Cloud type bias (0 = stratus, 1 = cumulus)">{},
			= gse::settings::range<0.f, 1.f>{},
			= gse::shared,
			= gse::settings::hot_reloadable
		]]
		float cloud_type = 0.7f;

		[[
			= gse::settings::describe<"Cloud density multiplier">{},
			= gse::settings::range<0.f, 5.f>{},
			= gse::settings::hot_reloadable
		]]
		float density_multiplier = 1.0f;

		[[
			= gse::settings::describe<"View-ray Beer's law extinction coefficient">{},
			= gse::settings::range<0.f, 1.f>{},
			= gse::settings::hot_reloadable
		]]
		float view_extinction = 0.15f;

		[[
			= gse::settings::describe<"Light-ray Beer's law extinction coefficient. The sun march covers only "
									  "6 x 5% of the layer thickness, so this needs to be far larger than the "
									  "view coefficient to produce visible self-shadowing.">{},
			= gse::settings::range<0.f, 8.f>{},
			= gse::settings::hot_reloadable
		]]
		float light_extinction = 3.0f;

		[[
			= gse::settings::describe<"Shape noise sampling scale (1/km)">{},
			= gse::settings::hot_reloadable
		]]
		atmosphere_inverse_length shape_scale = per_kilometer(0.4f);

		[[
			= gse::settings::describe<"Detail noise sampling scale (1/km)">{},
			= gse::settings::hot_reloadable
		]]
		atmosphere_inverse_length detail_scale = per_kilometer(3.5f);

		[[
			= gse::settings::describe<"Detail erosion strength">{},
			= gse::settings::range<0.f, 1.f>{},
			= gse::settings::hot_reloadable
		]]
		float detail_strength = 0.35f;

		[[
			= gse::settings::describe<"Henyey-Greenstein forward asymmetry">{},
			= gse::settings::range<0.f, 1.f>{},
			= gse::settings::hot_reloadable
		]]
		float phase_g_forward = 0.80f;

		[[
			= gse::settings::describe<"Henyey-Greenstein back asymmetry">{},
			= gse::settings::range<-1.f, 0.f>{},
			= gse::settings::hot_reloadable
		]]
		float phase_g_back = -0.30f;

		[[
			= gse::settings::describe<"Dual-lobe phase blend (0 = back only, 1 = forward only)">{},
			= gse::settings::range<0.f, 1.f>{},
			= gse::settings::hot_reloadable
		]]
		float phase_blend = 0.65f;

		[[
			= gse::settings::describe<"Ambient sky contribution into clouds">{},
			= gse::settings::range<0.f, 2.f>{},
			= gse::settings::hot_reloadable
		]]
		float ambient_strength = 1.0f;

		[[
			= gse::settings::describe<"Maximum cloud raymarch distance (km)">{},
			= gse::settings::hot_reloadable
		]]
		atmosphere_length max_distance = kilometers(80.0f);

		[[
			= gse::settings::describe<"Cloud wind velocity in world space (km/s)">{},
			= gse::settings::hot_reloadable
		]]
		vec3<atmosphere_length> wind_offset = { kilometers(0.0f), kilometers(0.0f), kilometers(0.0f) };

		gpu::shader_program shape_bake_pipeline;
		gpu::shader_program detail_bake_pipeline;
		gpu::shader_program raymarch_pipeline;
		gpu::shader_program composite_pipeline;

		gpu::image shape_noise;
		gpu::image detail_noise;
		gpu::image cloud_target;

		gpu::bindless_handle noise_sampler;
		gpu::bindless_handle atmosphere_lut_sampler;
		gpu::bindless_handle sky_view_sampler;
		gpu::bindless_handle composite_sampler;

		gpu::buffer cloud_ubo_buffer;

		vec2u cloud_target_extent{ 0, 0 };
		std::uint32_t frame_counter = 0;
		int applied_target_divisor = 0;
		bool noises_ready = false;
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
		shared_view<atmosphere::data> atm_state,
		shared_view<camera::data> cam_state
	) -> async::task<>;
}
