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
		struct data {
			vec2u current_extent{};

			gpu::pipeline pipeline;
			per_frame_resource<gpu::descriptor_region> descriptors;

			per_frame_resource<gpu::buffer> culling_params_buffers;
			per_frame_resource<gpu::buffer> light_buffers;
			[[= gse::shared]] per_frame_resource<gpu::buffer> light_index_list_buffers;
			[[= gse::shared]] per_frame_resource<gpu::buffer> tile_light_table_buffers;

			gpu::sampler depth_sampler;
			gpu::bindless_texture_slot depth_slot;
		};

		static auto run(
			run_context& ctx,
			const gpu::context::data& gpu_s,
			const asset::data& assets_s,
			data& d
		) -> async::task<>;

		static auto frame(
			frame_context& ctx,
			shared_view<gpu::context> gpu_s,
			const data& d,
			shared_view<camera::system> cam_state
		) -> async::task<>;

	private:
		static auto tile_count(
			const data& d
		) -> vec2u;

		static auto update_depth_descriptor(
			const gpu::context::data& gpu_s,
			data& d
		) -> void;

		static auto rebuild_tile_buffers(
			const gpu::context::data& gpu_s,
			data& d
		) -> void;
	};
}
