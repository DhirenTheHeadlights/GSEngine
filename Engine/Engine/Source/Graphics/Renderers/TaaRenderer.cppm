export module gse.graphics:taa_renderer;

import std;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.meta;
import gse.math;

export namespace gse::renderer::taa {
	struct system {
		struct [[= gse::settings::category<"Graphics">{}]] data {
			[[
				= gse::settings::describe<"Enable temporal antialiasing. Accumulates jittered frames via motion-vector reprojection.">{},
				= gse::shared
			]]
			bool taa_enabled = true;

			[[
				= gse::settings::describe<"Blend factor for current vs history. 0.1 = retain 90% history; lower = smoother but more ghosting.">{},
				= gse::settings::range<0.02f, 0.5f>{}
			]]
			float blend_alpha = 0.1f;

			gpu::shader_program pipeline;
			gpu::bindless_handle sampler;
			gpu::bindless_handle hdr_view;
			gpu::bindless_handle velocity_view;
			std::array<gpu::bindless_handle, 2> history_views;
			std::uint32_t frames_since_history_invalid = 0;

			[[= gse::shared]] std::array<gpu::image, 2> history;
		};

		static auto run(
			run_context& ctx,
			const gpu::context::data& gpu_s,
			data& d
		) -> async::task<>;

		static auto frame(
			const frame_context& ctx,
			shared_view<gpu::context> gpu_s,
			data& d
		) -> async::task<>;
	};
}
