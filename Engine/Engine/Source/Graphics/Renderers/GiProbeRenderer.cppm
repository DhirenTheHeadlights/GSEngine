export module gse.graphics:gi_probe_renderer;

import std;

import :atmosphere_renderer;
import :camera_system;
import :geometry_collector;
import :rt_shadow_renderer;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.meta;
import gse.math;
import gse.gpu_record;

export namespace gse::renderer::gi_probe {
	constexpr vec3u grid_dim{ 16, 6, 16 };
	constexpr std::uint32_t rays_per_probe = 64;
	constexpr std::uint32_t probe_tile_size = 8;

	enum class quality_level : int {
		off = 0,
		low = 1,
		medium = 2,
		high = 3,
	};

	struct [[= system_state<"GiProbe">{}, = settings::category<"Graphics">{}]] data {
		[[
			= settings::describe<"Probe-based indirect diffuse global illumination. Off disables probe updates and sampling.">{},
			= shared
		]]
		quality_level quality = quality_level::medium;

		[[
			= settings::describe<"Spacing between probes.">{},
			= settings::range<0.5f, 8.0f>{},
			= shared
		]]
		length spacing = meters(2.0f);

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
		per_frame_resource<gpu::bindless_handle> tlas_views;
		per_frame_resource<gpu::device_address> tlas_addresses;
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
		shared_view<geometry_collector::data> gc_r
	) -> async::task<>;
}