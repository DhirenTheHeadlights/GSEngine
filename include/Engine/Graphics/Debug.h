#pragma once

#include <imgui.h>
#include <string>

#include "glm/vec3.hpp"


namespace Engine::Debug {
	void setUpImGui();
	void updateImGui();
	void renderImGui();

	void printVector(const std::string& name, const glm::vec3& vec, const char* unit = nullptr);
}
