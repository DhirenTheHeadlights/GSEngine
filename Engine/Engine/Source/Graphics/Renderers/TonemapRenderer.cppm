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
			[[
				= gse::settings::
					describe<"HDR exposure multiplier applied before tonemapping. 1.0 is the neutral default.">{}
			]]
			float exposure = 1.0f;

			[[
				= gse::settings::describe<"Replace the final image with a hue/intensity visualization of the motion-vector buffer.">{}
			]]
			bool show_velocity = false;

			gpu::shader_program pipeline;
			gpu::bindless_sampler sampler;
			gpu::bindless_image_view hdr_view;
			gpu::bindless_image_view velocity_view;
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
