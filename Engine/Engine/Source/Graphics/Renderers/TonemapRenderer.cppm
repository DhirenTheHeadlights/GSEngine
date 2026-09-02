export module gse.graphics:tonemap_renderer;

import std;

import :atmosphere_renderer;
import :bloom_renderer;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.meta;
import gse.gpu_record;

export namespace gse::renderer::tonemap {
	constexpr std::uint32_t histogram_bins = 256;

	struct histogram_pass {};
	struct exposure_pass {};

	struct [[= system_state<"Tonemap">{}, = settings::category<"Graphics">{}]] data {
		[[
			= settings::describe<"Derive exposure from the scene each frame and adapt it over time, the way a camera "
									  "or an eye does. Off uses the fixed exposure below.">{}
		]]
		bool auto_exposure = true;

		[[
			= settings::describe<"In daylight, meter from the light falling on the scene (sun and sky irradiance) so "
									  "an 18% grey card under the current light lands at middle grey no matter what is "
									  "in frame. Off meters from the frame's luminance histogram alone, which brightens "
									  "dark framings and darkens bright ones.">{}
		]]
		bool incident_metering = true;

		[[
			= settings::describe<"Fixed HDR exposure multiplier used when auto exposure is off. With physical sun "
									  "irradiance (1361 W/m^2) around 0.003 renders a sunlit scene at middle grey; a "
									  "lamp-lit night needs a few hundred times more.">{}
		]]
		float exposure = 0.003f;

		[[
			= settings::describe<"Stops added on top of the automatic exposure. Positive brightens.">{},
			= settings::range<-5.f, 5.f>{}
		]]
		float exposure_compensation = 0.f;

		[[
			= settings::describe<"Dimmest scene luminance auto exposure adapts to. Below it the picture is left dark "
									  "rather than amplified toward middle grey, so a lamp-lit night still reads as night. "
									  "It is also the daylight level below which incident metering hands over to the "
									  "histogram.">{}
		]]
		irradiance min_luminance = watts_per_square_meter(0.1f);

		[[
			= settings::describe<"Brightest scene luminance auto exposure adapts to.">{}
		]]
		irradiance max_luminance = watts_per_square_meter(5000.f);

		[[
			= settings::describe<"Fraction of the darkest pixels left out of the histogram average, so unlit floor "
									  "and sky do not drag a night scene's exposure up.">{},
			= settings::range<0.f, 1.f>{}
		]]
		float low_percentile = 0.5f;

		[[
			= settings::describe<"Fraction of pixels below which the histogram average stops, so the sun disc and "
									  "specular highlights do not drag exposure down.">{},
			= settings::range<0.f, 1.f>{}
		]]
		float high_percentile = 0.95f;

		[[
			= settings::describe<"Time constant for adapting to a brighter scene.">{}
		]]
		time adaptation_time_bright = seconds(0.4f);

		[[
			= settings::describe<"Time constant for adapting to a darker scene.">{}
		]]
		time adaptation_time_dark = seconds(1.5f);

		[[
			= settings::describe<"Replace the final image with a hue/intensity visualization of the motion-vector buffer.">{}
		]]
		bool show_velocity = false;

		gpu::shader_program pipeline;
		gpu::shader_program histogram_pipeline;
		gpu::shader_program exposure_pipeline;
		per_frame_resource<gpu::buffer> histogram_buffers;
		gpu::buffer exposure_buffer;
		gpu::bindless_handle sampler;
		gpu::bindless_handle hdr_view;
		gpu::bindless_handle velocity_view;
	};

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<bloom::data> bloom_state,
		data& d
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		const context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out,
		shared_view<bloom::data> bloom_state,
		shared_view<atmosphere::data> atm_state
	) -> async::task<>;
}
