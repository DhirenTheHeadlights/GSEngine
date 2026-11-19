export module gse.graphics:light_packing;

import std;

import :atmosphere_renderer;
import :directional_light;
import :point_light;
import :shared_shaders;
import :spot_light;

import gse.core;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;
import gse.math;
import gse.physics;

export namespace gse::renderer::light_packing {
	struct view_bindings {
		shaders::uniform_block<shaders::common::camera_data> camera_ubo;
		shaders::ssbo_readwrite<shaders::forward::light> lights;
	};

	struct [[= shaders::shader_struct]] view_push_constants {
		std::uint32_t light_count;
	};

	using view_entry = gpu::compute_entry<
		gpu::body_path<"Compute/light_view_transform">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types>,
		gpu::bindings<view_bindings>,
		gpu::threads<64>,
		gpu::push_constant<view_push_constants>
	>;

	auto pack(
		std::span<shaders::forward::light> out,
		shared_view<atmosphere::data> atmosphere,
		const read<directional_light_component>& dir_lights,
		const read<spot_light_component>& spot_lights,
		const read<point_light_component>& point_lights,
		const read<physics::transform_component>& transforms
	) -> std::size_t;
}
