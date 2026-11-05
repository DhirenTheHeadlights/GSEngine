export module gse.graphics:gtao_renderer;

import std;

import :camera_system;
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

	struct [[= system_state<"Gtao">{}]]
	data {
		gpu::shader_program occlusion_pipeline;
		gpu::shader_program blur_pipeline;

		gpu::image raw_occlusion;
		gpu::image occlusion;
		gpu::bindless_handle depth_view;

		per_frame_resource<gpu::buffer> camera_ubo_buffers;
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
		shared_view<camera::data> cam_state,
		shared_view<forward::data> forward_s
	) -> async::task<>;
}
