export module gse.graphics:directional_light;

import gse.physics.math;
import gse.utility;

export namespace gse {
	struct directional_light_data {
		unitless::vec3 color = { 1.0f, 1.0f, 1.0f };
		float intensity = 1.0f;
		unitless::vec3 direction = { 0.0f, -1.0f, 0.0f };
		float ambient_strength = 1.0f;
	};

	struct directional_light_component : component<directional_light_data> {
		directional_light_component(id owner_id, const directional_light_data& data = {}) : component(owner_id, data) {}

		auto debug_menu(const std::string_view& name, std::uint32_t parent_id) -> void {
			
		}
	};
}