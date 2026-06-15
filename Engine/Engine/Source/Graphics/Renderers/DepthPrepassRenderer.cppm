export module gse.graphics:depth_prepass_renderer;

import std;

import :geometry_collector;
import :cull_compute_renderer;
import :physics_transform_renderer;
import :camera_system;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;

export namespace gse::renderer::depth_prepass {
	struct [[= gse::system_state<"DepthPrepass">{}]] data {
		gpu::shader_program meshlet_pipeline;

		per_frame_resource<gpu::buffer> camera_ubo_buffers;
	};

	[[= gse::system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<asset::data> assets_s,
		data& d
	) -> async::task<>;

	[[= gse::system_frame{}]]
	auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		const data& d,
		shared_view<geometry_collector::data> gc_r,
		shared_view<camera::data> cam_state
	) -> async::task<>;
}
