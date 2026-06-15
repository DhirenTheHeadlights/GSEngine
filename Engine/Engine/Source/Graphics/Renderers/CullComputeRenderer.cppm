export module gse.graphics:cull_compute_renderer;

import std;

import :geometry_collector;
import :camera_system;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

export namespace gse::renderer::cull_compute {
	struct [[= gse::system_state<"CullCompute">{}]] data {
		bool enabled = true;

		gpu::shader_program pipeline;
		per_frame_resource<gpu::buffer> frustum_buffer;
		per_frame_resource<gpu::buffer> batch_info_buffer;
	};

	[[= gse::system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<asset::data> assets_s,
		shared_view<geometry_collector::data> gc_r,
		data& d
	) -> async::task<>;

	[[= gse::system_frame{}]]
	auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<geometry_collector::data> gc_r,
		const data& d
	) -> async::task<>;
}
