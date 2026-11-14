export module gse.graphics:light_packing;

import std;

import :atmosphere_renderer;
import :directional_light;
import :point_light;
import :shared_shaders;
import :spot_light;

import gse.core;
import gse.ecs;
import gse.math;
import gse.physics;

export namespace gse::renderer::light_packing {
	auto pack(
		std::span<shaders::forward::light> out,
		const view_matrix& view,
		shared_view<atmosphere::data> atmosphere,
		const read<directional_light_component>& dir_lights,
		const read<spot_light_component>& spot_lights,
		const read<point_light_component>& point_lights,
		const read<physics::transform_component>& transforms
	) -> std::size_t;
}
