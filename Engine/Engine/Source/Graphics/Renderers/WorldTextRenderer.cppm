export module gse.graphics:world_text_renderer;

import std;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;

import :camera_system;
import :gui;
import :sdf_grid_renderer;

export namespace gse::renderer::world_text {
	struct system {
		struct data {
			gpu::shader_program pipeline;
			per_frame_resource<gpu::bindless_buffer> camera_ubo_buffers;
			per_frame_resource<gpu::buffer> vertex_buffers;
			per_frame_resource<std::size_t> vertex_capacities;
		};

		static auto run(
			run_context& ctx,
			const gpu::context::data& gpu_s,
			data& d
		) -> async::task<>;

		static auto frame(
			const frame_context& ctx,
			shared_view<gpu::context> gpu_s,
			data& d,
			shared_view<camera::system> cam_state,
			shared_view<gui::system> gui_d,
			shared_view<sdf_grid::system> grid_d
		) -> async::task<>;
	};
}
