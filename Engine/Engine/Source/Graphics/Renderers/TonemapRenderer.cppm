export module gse.graphics:tonemap_renderer;

import std;

import :bloom_renderer;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.meta;
import gse.gpu_record;

export namespace gse::renderer::tonemap {
	struct [[= system_state<"Tonemap">{}, = settings::category<"Graphics">{}]] data {
		[[
			= settings::
				describe<"HDR exposure multiplier applied before tonemapping. 1.0 is the neutral default.">{}
		]]
		float exposure = 1.0f;

		[[
			= settings::describe<"Replace the final image with a hue/intensity visualization of the motion-vector buffer.">{}
		]]
		bool show_velocity = false;

		gpu::shader_program pipeline;
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
		shared_view<bloom::data> bloom_state
	) -> async::task<>;
}