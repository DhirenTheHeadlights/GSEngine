export module gse.graphics:point_light;

import std;

import :cube_map;

import gse.physics.math;
import gse.utility;

export namespace gse {
	struct point_light_data {
		unitless::vec3 color;
		float intensity = 1.0f;
		vec3<length> position;
		float constant = 1.0f; 
		float linear = 0.09f; 
		float quadratic = 0.032f; 
		float ambient_strength = 0.025f;
		id ignore_list;
		length near_plane = meters(0.1f);
		length far_plane = meters(10000.f);
	};

	struct point_light_component : component<point_light_data> {
		point_light_component(id owner_id, const point_light_data& data = {}) : component(owner_id, data) {}

		auto debug_menu(const std::string_view& name, std::uint32_t parent_id) -> void {

		}
	};
}