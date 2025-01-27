module;

#include "imgui.h"

export module gse.graphics.debug;

import std;
import glm;

import gse.physics.math.units;
import gse.physics.math.units.quantity;
import gse.physics.math.vector;

export namespace gse::debug {
	auto set_up_imgui() -> void;
	auto update_imgui() -> void;
	auto render_imgui() -> void;
	auto save_imgui_state() -> void;

	auto set_imgui_save_file_path(const std::string& path) -> void;

	auto print_vector(const std::string& name, const glm::vec3& vec, const char* unit = nullptr) -> void;
	auto print_value(const std::string& name, const float& value, const char* unit = nullptr) -> void;

	template <is_unit UnitType = internal::unitless_unit, is_quantity QuantityType>
	auto unit_slider(const std::string& name, QuantityType& quantity, const QuantityType& min, const QuantityType& max) -> void {
		float value = quantity.template as<UnitType>();

		const std::string slider_label = name + " (" + UnitType::unit_name + ")";
		if (ImGui::SliderFloat(slider_label.c_str(),
			&value,
			min.template as<UnitType>(),
			max.template as<UnitType>()))
		{
			quantity.template set<UnitType>(value);
		}
	}

	auto print_boolean(const std::string& name, const bool& value) -> void;

	auto add_imgui_callback(const std::function<void()>& callback) -> void;
	auto get_imgui_needs_inputs() -> bool;
}
