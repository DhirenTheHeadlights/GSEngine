export module gse.graphics:spot_light;

import std;

import gse.physics.math;
import gse.utility;

export namespace gse {
	struct spot_light_data {
		unitless::vec3 color;
		float intensity = 1.0f;
		vec3<length> position;
		unitless::vec3 direction;
		float constant = 1.0f;
		float linear = 0.09f;
		float quadratic = 0.032f;
		angle cut_off;
		angle outer_cut_off;
		float ambient_strength = 0.025f;
		length near_plane = meters(0.1f);
		length far_plane = meters(10000.f);
		std::vector<id> ignore_list_ids;
	};

	struct spot_light_component : component<spot_light_data> {
		spot_light_component(const id& owner_id, const spot_light_data& data = {}) : component(owner_id, data) {}

		auto debug_menu(const std::string_view& name, std::uint32_t parent_id) -> void {
			
		}
	};
}