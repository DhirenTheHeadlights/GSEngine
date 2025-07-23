module;

#include <imgui.h>
#include <string>

export module gse.graphics:point_light;

import :debug;
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
		point_light_component(const id& owner_id, const point_light_data& data = {}) : component(owner_id, data) {}

		auto debug_menu(const std::string_view& name, std::uint32_t parent_id) -> void {
			debug::add_imgui_callback([this, name, parent_id] {
				ImGui::Begin(std::string("Point Light Component " + std::string(name) + ": " + std::to_string(parent_id)).c_str());
				ImGui::ColorEdit3("Color", &color.x);
				debug::unit_slider("Intensity", intensity, 0.f, 100.0f);
				debug::unit_slider("Constant", constant, 0.f, 1.0f);
				debug::unit_slider("Linear", linear, 0.f, 0.1f);
				debug::unit_slider("Quadratic", quadratic, 0.f, 0.1f);
				debug::unit_slider("Ambient Strength", ambient_strength, 0.f, 10.0f);
				ImGui::End();
			});
		}

		cube_map map;
	};
}