export module gse.graphics:scene_snapshot_renderer;

import std;

import gse.gpu;
import gse.core;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.gpu_record;

export namespace gse::renderer::scene_snapshot {
	struct [[= system_state<"SceneSnapshot">{}]] data {
		[[= shared]] per_frame_resource<gpu::image> snapshots;
		[[
			= shared
		]]
		std::array<gpu::bindless_handle, per_frame_resource<gpu::image>::frames_in_flight> slots;
		[[= shared]] bool ready = false;

		vec2u current_extent{ 0, 0 };
		bool enabled = true;
	};

	[[= system_init{}]]
	auto init(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> async::task<>;

	[[= system_run<>{}]]
	auto run(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		const context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out
	) -> async::task<>;
}