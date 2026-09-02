export module gse.graphics:taa_renderer;

import std;

import :camera_system;
import :forward_renderer;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.meta;
import gse.math;
import gse.gpu_record;

export namespace gse::renderer::taa {
	struct [[= system_state<"Taa">{}, = settings::category<"Graphics">{}]] data {
		[[
			= settings::describe<"Enable temporal antialiasing. Accumulates jittered frames via motion-vector reprojection.">{},
			= shared
		]]
		bool taa_enabled = true;

		[[
			= settings::describe<"Blend factor for current vs history. 0.1 = retain 90% history; lower = smoother but more ghosting.">{},
			= settings::range<0.02f, 0.5f>{}
		]]
		float blend_alpha = 0.1f;

		gpu::shader_program pipeline;
		gpu::bindless_handle sampler;
		gpu::bindless_handle hdr_view;
		gpu::bindless_handle velocity_view;
		std::array<gpu::bindless_handle, 2> history_views;
		std::uint32_t frames_since_history_invalid = 0;

		[[= shared]] std::array<gpu::image, 2> history;
	};

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> async::task<>;

	[[= gse::system_frame{}]]
	[[= runs_after<^^forward::data>{}]]
	auto frame(
		const context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out,
		channel_write<camera::jitter_request> jitter_out
	) -> async::task<>;
}
