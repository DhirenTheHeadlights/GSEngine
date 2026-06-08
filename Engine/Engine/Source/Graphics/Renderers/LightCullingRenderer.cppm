export module gse.graphics:light_culling_renderer;

import std;

import :camera_system;
import :atmosphere_renderer;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

export namespace gse::renderer::light_culling {
	constexpr std::uint32_t tile_size = 16;
	constexpr std::uint32_t max_lights_per_tile = 64;
	constexpr std::size_t max_lights = 1024;

	struct system {
		struct data {
			std::uint32_t current_width = 0;
			std::uint32_t current_height = 0;

			gpu::shader_program pipeline;

			per_frame_resource<gpu::buffer> culling_params_buffers;
			per_frame_resource<gpu::buffer> light_buffers;
			[[= gse::shared]] per_frame_resource<gpu::buffer> light_index_list_buffers;
			[[= gse::shared]] per_frame_resource<gpu::buffer> tile_light_table_buffers;

			gpu::bindless_handle depth_sampler;
			gpu::bindless_handle depth_view;
		};

		static auto init(
			run_context& ctx,
			const gpu::context::data& gpu_s,
			const asset::data& assets_s,
			data& d
		) -> async::task<>;

		static auto frame(
			frame_context& ctx,
			shared_view<gpu::context> gpu_s,
			const data& d,
			shared_view<camera::system> cam_state,
			shared_view<atmosphere::system> atm_state
		) -> async::task<>;
	};
}
