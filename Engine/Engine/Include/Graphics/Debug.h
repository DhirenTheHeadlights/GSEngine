#pragma once
#include <string>
#include "glm/vec3.hpp"


namespace Engine::Debug {
	void setUpImGui();
	void updateImGui();
	void renderImGui();

	void printVector(const std::string& name, const glm::vec3& vec, const char* unit = nullptr);
	void printValue(const std::string& name, const float& value, const char* unit = nullptr);
	void printBoolean(const std::string& name, const bool& value);
}
