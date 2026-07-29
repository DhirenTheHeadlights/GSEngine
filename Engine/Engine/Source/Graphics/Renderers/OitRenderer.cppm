export module gse.graphics:oit_renderer;

import std;

import :geometry_collector;
import :camera_system;
import :atmosphere_renderer;

import gse.math;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.gpu;
import gse.meta;

export namespace gse::renderer::oit {
	struct accumulate_pass {};
	struct composite_pass {};

	struct [[= gse::system_state<"OIT">{}]] data {
		gpu::shader_program accum_pipeline;
		gpu::shader_program composite_pipeline;

		per_frame_resource<gpu::buffer> camera_ubo_buffers;

		gpu::bindless_handle sampler;
		gpu::bindless_handle material_sampler;
		gpu::bindless_handle accum_view;
		gpu::bindless_handle reveal_view;
	};

	[[= gse::system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> async::task<>;

	[[= gse::system_frame{}]]
	auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		shared_view<camera::data> cam_state,
		shared_view<geometry_collector::data> gc_r,
		shared_view<atmosphere::data> atm_state
	) -> async::task<>;
}
