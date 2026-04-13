export module gse.graphics:directional_light;

import std;

import gse.math;
import gse.utility;

export namespace gse {
	struct directional_light_data {
		vec3f color = { 1.0f, 1.0f, 1.0f };
		float intensity = 1.0f;
		vec3f direction = { 0.0f, -1.0f, 0.0f };
		float ambient_strength = 1.0f;
		float source_radius = 0.02f;
	};

	struct directional_light_component : component<directional_light_data> {
		directional_light_component(const id owner_id, const directional_light_data& data = {}) : component(owner_id, data) {}

		auto debug_menu(const std::string_view& name, std::uint32_t parent_id) -> void {
			
		}
	};
}