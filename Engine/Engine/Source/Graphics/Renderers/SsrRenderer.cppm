export module gse.graphics:ssr_renderer;

import std;

import :camera_system;
import :forward_renderer;
import :lighting_source;
import :taa_renderer;

import gse.concurrency;
import gse.core;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;
import gse.math;

export namespace gse::renderer::ssr {
	struct [[= system_state<"Ssr">{}]]
	data {
		gpu::shader_program pipeline;

		gpu::image radiance;
		gpu::bindless_handle depth_view;
		gpu::bindless_handle history_sampler;

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
		channel_write<lighting_source::screen_reflections> reflections_out,
		shared_view<camera::data> cam_state,
		shared_view<forward::data> forward_s,
		shared_view<taa::data> taa_s
	) -> async::task<>;
}
