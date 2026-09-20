export module gse.graphics:depth_prepass_renderer;

import gse.assets;
import gse.concurrency;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;

import :camera_system;
import :geometry_collector;

export namespace gse::renderer::depth_prepass {
	struct [[= system_state<"DepthPrepass">{}]] data {
		gpu::shader_program meshlet_pipeline;

		per_frame_resource<gpu::buffer> camera_ubo_buffers;
	};

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<asset::data> assets_s,
		data& d
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		const data& d,
		channel_write<gpu::render_pass_request> pass_out,
		channel_read<geometry_collector::render_data> geometry_in,
		shared_view<geometry_collector::data> gc_r,
		shared_view<camera::data> cam_state
	) -> async::task<>;
}