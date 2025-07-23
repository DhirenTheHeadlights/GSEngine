module;

#include <imgui.h>

export module gse.graphics:spot_light;

import std;

import :debug;

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
			debug::add_imgui_callback([this, name, parent_id] {
				ImGui::Begin(std::string("Spot Light Component " + std::string(name) + ": " + std::to_string(parent_id)).c_str());
				ImGui::ColorEdit3("Color", &color.x);
				debug::unit_slider("Intensity", intensity, 0.f, 100.0f);
				debug::unit_slider("Constant", constant, 0.f, 1.0f);
				debug::unit_slider("Linear", linear, 0.f, 1.0f);
				debug::unit_slider("Quadratic", quadratic, 0.f, 1.0f);
				debug::unit_slider<units::degrees>("Cut Off", cut_off, degrees(0.0f), outer_cut_off);
				debug::unit_slider<units::degrees>("Outer Cut Off", outer_cut_off, degrees(0.0f), degrees(90.0f));
				debug::unit_slider("Ambient Strength", ambient_strength, 0.01f, 0.1f);
				ImGui::SliderFloat3("Direction", &direction.x, -1.0f, 1.0f);
				debug::unit_slider<units::meters>("Near Plane", near_plane, meters(0.1f), meters(100.0f));
				debug::unit_slider<units::meters>("Far Plane", far_plane, meters(100.0f), meters(1000.0f));
				ImGui::End();
			});
		}
	};
}