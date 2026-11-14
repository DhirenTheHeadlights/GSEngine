export module gse.graphics:sky_renderer;

import gse.concurrency;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;
import gse.math;
import gse.meta;

import :atmosphere_renderer;
import :camera_latch_renderer;
import :camera_system;
import :cloud_renderer;

export namespace gse::renderer::sky {
	struct [[= system_state<"Sky">{}, = settings::category<"Sky">{}]]
	data {
		[[
			= settings::describe<"Sun disk angular radius (degrees)">{},
			= settings::range<0.1f, 5.f>{},
			= settings::hot_reloadable
		]]
		angle sun_angular_radius = degrees(1.5f);

		[[
			= settings::describe<"Brightness of the procedural starfield. Stars are world-fixed, so they hold "
									  "still while the camera pans. 0 disables them.">{},
			= settings::range<0.f, 8.f>{},
			= settings::hot_reloadable
		]]
		float star_intensity = 1.6f;

		[[
			= settings::describe<"Star field cell subdivision. Higher packs more, smaller stars into the sky; "
									  "the star radius scales down with it so they stay sub-pixel-stable.">{},
			= settings::range<20.f, 400.f>{},
			= settings::hot_reloadable
		]]
		float star_density = 140.0f;

		[[
			= settings::describe<"How fast stars wash out as the sun climbs. Stars reach full brightness once "
									  "the sun is this far below the horizon, in units of sin(elevation).">{},
			= settings::range<0.01f, 0.5f>{},
			= settings::hot_reloadable
		]]
		float star_fade = 0.12f;

		gpu::shader_program pipeline;
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
		const data& d,
		channel_write<gpu::render_pass_request> pass_out,
		shared_view<atmosphere::data> atm_state,
		shared_view<cloud::data> cloud_state,
		shared_view<camera::data> cam_state,
		shared_view<camera_latch::data> latch_s
	) -> async::task<>;
}
