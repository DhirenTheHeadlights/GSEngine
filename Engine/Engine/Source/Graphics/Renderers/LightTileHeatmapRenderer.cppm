export module gse.graphics:light_tile_heatmap_renderer;

import std;

import :light_culling_renderer;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.meta;
import gse.gpu_record;

export namespace gse::renderer::light_tile_heatmap {
	struct [[= system_state<"LightTileHeatmap">{}, = settings::category<"Light Culling">{}]] data {
		[[
			= settings::describe<"Overlay a per-tile heatmap of the Forward+ light culling result on the final image. "
								 "Blue tiles hold few lights, red tiles are at the per-tile cap.">{}
		]]
		bool show_tile_heatmap = false;

		[[
			= settings::describe<"How strongly the heatmap is blended over the scene.">{},
			= settings::range<0.f, 1.f>{}
		]]
		float heatmap_opacity = 0.85f;

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
		shared_view<light_culling::data> lc_r
	) -> async::task<>;
}
