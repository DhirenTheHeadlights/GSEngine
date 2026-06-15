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
	struct [[= gse::system_state<"Tonemap">{}, = gse::settings::category<"Graphics">{}]] data {
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
		gpu::bindless_handle sampler;
		gpu::bindless_handle hdr_view;
		gpu::bindless_handle velocity_view;
	};

	[[= gse::system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<bloom::data> bloom_state,
		data& d
	) -> async::task<>;

	[[= gse::system_frame{}]]
	auto frame(
		const context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		shared_view<bloom::data> bloom_state
	) -> async::task<>;
}
