export module gse.graphics:forward_renderer;

import std;

import :geometry_collector;
import :depth_prepass_renderer;
import :rt_shadow_renderer;
import :light_culling_renderer;
import :skin_compute_renderer;
import :cull_compute_renderer;
import :camera_system;
import :texture;
import :point_light;
import :spot_light;
import :directional_light;
import :settings;

import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.save;
import gse.meta;

export namespace gse::renderer::forward {
	constexpr std::size_t max_lights = 1024;
	constexpr std::size_t max_materials = 1024;

	enum class shadow_quality_level : int {
		off = 0,
		hard = 1,
		low = 2,
		medium = 3,
		high = 4
	};

	enum class ao_quality_level : int {
		off = 0,
		low = 1,
		medium = 2,
		high = 3,
		ultra = 4
	};

	enum class reflection_quality_level : int {
		off = 0,
		low = 1,
		medium = 2,
		high = 3
	};

	struct system {
		struct settings {
			static constexpr std::string_view category = "Graphics";

			[[=gse::settings::describe{}]]
			shadow_quality_level shadow_quality = shadow_quality_level::medium;

			[[=gse::settings::describe{}]]
			ao_quality_level ao_quality = ao_quality_level::medium;

			[[=gse::settings::describe{}]]
			reflection_quality_level reflection_quality = reflection_quality_level::medium;
		};

		struct resources {
			gpu::pipeline pipeline;
			per_frame_resource<gpu::descriptor_region> descriptors;
			resource::handle<shader> shader_handle;

			gpu::pipeline skinned_pipeline;
			per_frame_resource<gpu::descriptor_region> skinned_descriptors;
			resource::handle<shader> skinned_shader;

			per_frame_resource<gpu::buffer> light_buffers;
			per_frame_resource<gpu::buffer> material_palette_buffers;

			resource::handle<texture> blank_texture;

			std::unordered_map<std::string, per_frame_resource<gpu::buffer>> ubo_allocations;
		};

		struct frame_data {
			linear_vector<std::byte> light_staging;
			linear_vector<std::byte> material_staging;
		};

		static auto initialize(
			const init_context& phase,
			const gpu::context::state& gpu_s,
			const rt_shadow::system::state& rt_state,
			const light_culling::system::resources& lc_r,
			settings& cfg,
			resources& r,
			frame_data& fd
		) -> void;

		static auto frame(
			frame_context& ctx,
			const gpu::context::state& gpu_s,
			const settings& cfg,
			const resources& r,
			frame_data& fd,
			const camera::system::state& cam_state,
			const geometry_collector::system::resources& gc_r,
			const light_culling::system::resources& lc_r
		) -> async::task<>;
	};
}
