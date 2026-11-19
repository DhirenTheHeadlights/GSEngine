export module gse.graphics:gtao_renderer;

import std;

import :camera_latch_renderer;
import :forward_renderer;
import :lighting_source;

import gse.concurrency;
import gse.core;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;
import gse.math;

export namespace gse::renderer::gtao {
	struct occlusion_pass {};

	struct [[= system_state<"Gtao">{}, = requires_setting<^^lighting::data::lighting_technique, lighting::technique::raster>{}]]
	data {
		gpu::shader_program occlusion_pipeline;
		gpu::shader_program blur_pipeline;

		gpu::image raw_occlusion;
		gpu::image occlusion;
		gpu::bindless_handle depth_view;

	};

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out,
		channel_write<lighting_source::screen_ambient_occlusion> occlusion_out,
		shared_view<camera_latch::data> latch_s,
		shared_view<forward::data> forward_s
	) -> async::task<>;
}
