export module gse.graphics:light_culling_renderer;

import std;

import :camera_system;
import :geometry_collector;
import :atmosphere_renderer;
import :point_light;
import :spot_light;
import :directional_light;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.gpu_record;
import gse.physics;

export namespace gse::renderer::light_culling {
	constexpr std::uint32_t tile_size = 16;
	constexpr std::uint32_t max_lights_per_tile = 512;
	constexpr std::size_t max_lights = 1024;

	struct [[= shaders::shader_constant_block]] light_culling_limits {
		std::uint32_t tile_size = light_culling::tile_size;
		std::uint32_t max_lights_per_tile = light_culling::max_lights_per_tile;
		irradiance cull_threshold = watts_per_square_meter(0.01f);
	};

	constexpr light_culling_limits limits{};

	struct [[= system_state<"LightCulling">{}]] data {
		[[= shared]] std::uint32_t current_width = 0;
		[[= shared]] std::uint32_t current_height = 0;

		gpu::shader_program pipeline;

		per_frame_resource<gpu::buffer> culling_params_buffers;
		per_frame_resource<gpu::buffer> light_buffers;
		[[= shared]] per_frame_resource<gpu::handle<gpu::buffer>> light_index_list_buffers;
		[[= shared]] per_frame_resource<gpu::handle<gpu::buffer>> tile_light_table_buffers;

		gpu::bindless_handle depth_sampler;
		gpu::bindless_handle depth_view;
	};

	auto tile_count(
		std::uint32_t width,
		std::uint32_t height
	) -> vec2u;

	[[= system_init{}]] auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<asset::data> assets_s,
		data& d
	) -> async::task<>;

	[[= system_frame{}]] auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		const data& d,
		channel_write<gpu::render_pass_request> pass_out,
		channel_read<geometry_collector::render_data> geometry_in,
		shared_view<camera::data> cam_state,
		shared_view<atmosphere::data> atm_state,
		read<directional_light_component> dir_lights,
		read<spot_light_component> spot_lights,
		read<point_light_component> point_lights,
		read<physics::transform_component> transforms
	) -> async::task<>;
}