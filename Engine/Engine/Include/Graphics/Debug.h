#pragma once
#include <string>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include "imgui.h"
#include "Physics/Units/UnitTemplate.h"
#include "Physics/Vector/Vec3.h"

namespace gse::debug {
	void set_up_imgui();
	void update_imgui();
	void render_imgui();
	void save_imgui_state();

	void set_imgui_save_file_path(const std::string& path);

	void print_vector(const std::string& name, const glm::vec3& vec, const char* unit = nullptr);
	void print_value(const std::string& name, const float& value, const char* unit = nullptr);

	template <is_quantity quantity_type = unitless, is_unit unit_type = internal::unitless_unit>
	void unit_slider(const std::string& name, quantity_type& quantity, const quantity_type& min, const quantity_type& max) {
		float value = quantity.template as<unit_type>();

		if (ImGui::SliderFloat((name + " (" + unit_type::template unit_name + ")").c_str(), &value, min.template as<unit_type>(), max.template as<unit_type>())) {
			quantity.template set<unit_type>(value);
		}
	}

	void print_boolean(const std::string& name, const bool& value);

	void add_imgui_callback(const std::function<void()>& callback);
	bool get_imgui_needs_inputs();
}
