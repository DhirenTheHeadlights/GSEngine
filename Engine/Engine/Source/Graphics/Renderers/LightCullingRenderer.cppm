export module gse.graphics:light_culling_renderer;

import std;

import :point_light;
import :spot_light;
import :directional_light;
import :camera_system;
import :depth_prepass_renderer;
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
		struct state {
			vec2u current_extent{};
		};

		struct resources {
			gpu::pipeline pipeline;
			per_frame_resource<gpu::descriptor_region> descriptors;

			per_frame_resource<gpu::buffer> culling_params_buffers;
			per_frame_resource<gpu::buffer> light_buffers;
			per_frame_resource<gpu::buffer> light_index_list_buffers;
			per_frame_resource<gpu::buffer> tile_light_table_buffers;

			gpu::sampler depth_sampler;
		};

		struct frame_data {};

		static auto run(
			run_context& ctx,
			const gpu::context::state& gpu_s,
			const asset::state& assets_s,
			resources& r,
			frame_data& fd,
			state& s
		) -> async::task<>;

		static auto frame(
			frame_context& ctx,
			const gpu::context::state& gpu_s,
			const resources& r,
			frame_data& fd,
			const state& s,
			const camera::system::state& cam_state
		) -> async::task<>;

	private:
		static auto tile_count(
			const state& s
		) -> vec2u;

		static auto update_depth_descriptor(
			const gpu::context::state& gpu_s,
			resources& r
		) -> void;

		static auto rebuild_tile_buffers(
			const gpu::context::state& gpu_s,
			resources& r,
			state& s
		) -> void;
	};
}
