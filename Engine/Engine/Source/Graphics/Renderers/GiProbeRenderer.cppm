export module gse.graphics:gi_probe_renderer;

import std;

import :atmosphere_renderer;
import :camera_system;
import :directional_light;
import :geometry_collector;
import :point_light;
import :rt_shadow_renderer;
import :spot_light;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.meta;
import gse.math;
import gse.gpu_record;
import gse.physics;

export namespace gse::renderer::gi_probe {
	constexpr vec3u max_grid_dim{ 32, 8, 32 };
	constexpr std::uint32_t rays_per_probe = 64;
	constexpr std::uint32_t probe_tile_size = 8;

	struct probe_grid_info {
		vec3u dim{ 16, 6, 16 };
	};

	enum class quality_level : int {
		off [[= probe_grid_info{ .dim = { 16, 6, 16 } }]] = 0,
		low [[= probe_grid_info{ .dim = { 16, 6, 16 } }]] = 1,
		medium [[= probe_grid_info{ .dim = { 24, 6, 24 } }]] = 2,
		high [[= probe_grid_info{ .dim = max_grid_dim }]] = 3,
	};

	auto grid_dim_for(
		quality_level quality
	) -> vec3u;

	struct [[= system_state<"GiProbe">{}, = settings::category<"Graphics">{}]] data {
		[[
			= settings::describe<"Probe-based indirect diffuse global illumination. Selects how many probes the grid "
								 "carries, so it sets both the cost and how far the probe volume reaches around the "
								 "camera. The irradiance atlas is sized from this at startup, so a change takes "
								 "effect on the next launch. Off disables probe updates and sampling.">{},
			= shared
		]]
		quality_level quality = quality_level::medium;

		[[
			= settings::describe<"Spacing between probes. This sets how coarsely indirect light is quantised; the "
								 "volume the probes cover is this times the grid the quality level selects, so "
								 "tightening it trades reach for detail.">{},
			= settings::range<0.5f, 8.0f>{},
			= shared
		]]
		length spacing = meters(1.5f);

		[[
			= settings::describe<"Multiplier on indirect diffuse contribution from probes.">{},
			= settings::range<0.0f, 4.0f>{},
			= shared
		]]
		float intensity = 1.0f;

		[[
			= settings::describe<"Maximum ray distance for probe updates.">{},
			= settings::range<5.0f, 200.0f>{}
		]]
		length trace_t_max = meters(50.0f);

		gpu::shader_program update_pipeline;
		gpu::bindless_handle sky_view_sampler;
		[[= shared]] vec3u atlas_grid_dim{};
		per_frame_resource<gpu::bindless_handle> tlas_views;
		per_frame_resource<gpu::device_address> tlas_addresses;
		per_frame_resource<gpu::buffer> light_buffers;
		std::uint32_t frame_counter = 0;

		[[= shared]] gpu::image irradiance_atlas;
		[[= shared]] vec3<position> origin_world{};
	};

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<rt_shadow::data> rt_state,
		shared_view<geometry_collector::data> gc_state,
		data& d
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out,
		channel_read<geometry_collector::render_data> geometry_in,
		shared_view<camera::data> cam_state,
		shared_view<atmosphere::data> atm_state,
		shared_view<geometry_collector::data> gc_r,
		read<directional_light_component> dir_lights,
		read<spot_light_component> spot_lights,
		read<point_light_component> point_lights,
		read<physics::transform_component> transforms
	) -> async::task<>;
}