export module gse.graphics:world_text_renderer;

import std;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.gpu_record;

import :camera_system;
import :gui;
import :sdf_grid_renderer;

export namespace gse::renderer::world_text {
	struct [[= system_state<"WorldText">{}]] data {
		gpu::shader_program pipeline;
		gpu::bindless_handle text_sampler;
		per_frame_resource<gpu::buffer> camera_ubo_buffers;
		per_frame_resource<gpu::buffer> vertex_buffers;
		per_frame_resource<std::size_t> vertex_capacities;
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
		data& d,
		channel_write<gpu::render_pass_request> pass_out,
		shared_view<camera::data> cam_state,
		shared_view<gui::data> gui_d,
		shared_view<sdf_grid::data> grid_d
	) -> async::task<>;
}