export module gse.graphics:raster_shadow_renderer;

import std;

import :atmosphere_renderer;
import :camera_system;
import :directional_light;
import :forward_renderer;
import :geometry_collector;
import :lighting_source;
import :point_light;
import :spot_light;

import gse.concurrency;
import gse.containers;
import gse.core;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;
import gse.math;
import gse.physics;

export namespace gse::renderer::raster_shadow {
	constexpr std::uint32_t cascade_count = 4;
	constexpr std::uint32_t cascade_extent = 2048;
	constexpr std::uint32_t cascade_atlas_extent = 4096;
	constexpr std::uint32_t local_tile_extent = 512;
	constexpr std::uint32_t local_atlas_extent = 4096;
	constexpr std::uint32_t local_tiles_per_row = local_atlas_extent / local_tile_extent;
	constexpr std::uint32_t local_tile_count = local_tiles_per_row * local_tiles_per_row;
	constexpr std::uint32_t max_tiles = cascade_count + local_tile_count;
	constexpr std::size_t max_lights = 1024;

	struct [[= shaders::shader_struct]]
	tile_request {
		std::uint32_t light_index;
		std::uint32_t face;
		vec2u atlas_offset;
		std::uint32_t atlas_extent;
	};

	struct local_candidate {
		length distance;
		std::uint32_t light_index = 0;
	};

	struct cascade_pass {};

	struct setup_pass {};

	struct [[= system_state<"RasterShadow">{}]]
	data {
		gpu::shader_program setup_pipeline;
		gpu::shader_program depth_pipeline;
		bool vertex_pulled = false;

		gpu::image cascade_atlas;
		gpu::image local_atlas;

		per_frame_resource<gpu::buffer> camera_ubo_buffers;
		per_frame_resource<gpu::buffer> light_buffers;
		per_frame_resource<gpu::buffer> request_buffers;
		per_frame_resource<gpu::buffer> tile_buffers;
		per_frame_resource<gpu::buffer> light_tile_buffers;

		linear_vector<shaders::forward::light> packed_lights;
		linear_vector<tile_request> requests;
		linear_vector<vec2u> light_tiles;
		linear_vector<local_candidate> local_candidates;
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
		data& d,
		channel_write<gpu::render_pass_request> pass_out,
		channel_write<lighting_source::raster_shadows> shadows_out,
		channel_read<geometry_collector::render_data> geometry_in,
		shared_view<geometry_collector::data> gc_r,
		shared_view<camera::data> cam_state,
		shared_view<atmosphere::data> atm_state,
		shared_view<forward::data> forward_s,
		read<directional_light_component> dir_lights,
		read<spot_light_component> spot_lights,
		read<point_light_component> point_lights,
		read<physics::transform_component> transforms
	) -> async::task<>;
}
