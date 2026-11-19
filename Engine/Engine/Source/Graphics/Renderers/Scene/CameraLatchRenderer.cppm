export module gse.graphics:camera_latch_renderer;

import gse.concurrency;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;

import :camera_system;

export namespace gse::renderer::camera_latch {
	struct [[= system_state<"CameraLatch">{}]] data {
		gpu::shader_program pipeline;
		gpu::buffer mouse_totals;
		gpu::buffer history;
		per_frame_resource<gpu::buffer> camera_in_buffers;

		[[= shared]] per_frame_resource<gpu::buffer> camera_buffers;
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
		const data& d,
		channel_write<gpu::render_pass_request> pass_out,
		shared_view<camera::data> cam_state
	) -> async::task<>;

	[[= system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;
}
