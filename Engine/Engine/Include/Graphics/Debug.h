#pragma once
#include <string>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include "imgui.h"
#include "Physics/Units/UnitTemplate.h"
#include "Physics/Vector/Vec3.h"

namespace Engine::Debug {
	void setUpImGui();
	void updateImGui();
	void renderImGui();
	void saveImGuiState();

	void setImguiSaveFilePath(const std::string& path);

	void printVector(const std::string& name, const glm::vec3& vec, const char* unit = nullptr);
	void printValue(const std::string& name, const float& value, const char* unit = nullptr);

	template <IsQuantity Quantity = Unitless, IsUnit Unit = UnitlessUnit>
	void unitSlider(const std::string& name, Quantity& quantity, const Quantity& min, const Quantity& max) {
		float value = quantity.template as<Unit>();

		if (ImGui::SliderFloat((name + " " + Unit::template UnitName).c_str(), &value, min.template as<Unit>(), max.template as<Unit>())) {
			quantity.template set<Unit>(value);
		}
	}

	void printBoolean(const std::string& name, const bool& value);

	void addImguiCallback(const std::function<void()>& callback);
	bool getImGuiNeedsInputs();
}
