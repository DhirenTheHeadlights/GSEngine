module;

#include <imgui.h>

export module gse.graphics:directional_light;

import :debug;

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
		directional_light_component(const id& owner_id, const directional_light_data& data = {}) : component(owner_id, data) {}

		auto debug_menu(const std::string_view& name, std::uint32_t parent_id) -> void {
			debug::add_imgui_callback([this, name, parent_id] {
				ImGui::Begin(std::string("Directional Light Component " + std::string(name) + ": " + std::to_string(parent_id)).c_str());
				ImGui::ColorEdit3("Color", &color.x);
				debug::unit_slider("Intensity", intensity, 0.f, 100.0f);
				debug::unit_slider("Ambient Strength", ambient_strength, 0.f, 10.0f);
				ImGui::SliderFloat3("Direction", &direction.x, -1.0f, 1.0f);
				ImGui::End();
			});
		}
	};
}