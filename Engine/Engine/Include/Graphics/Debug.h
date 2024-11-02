#pragma once
#include <string>
#include <glm/glm.hpp>

#include "imgui.h"

namespace Engine::Debug {
	void setUpImGui();
	void updateImGui();
	void renderImGui();
	void saveImGuiState();

	void createWindow(const std::string& name, const ImVec2& size = { 500.f, 500.f }, const ImVec2& position = { 500.f, 500.f }, bool open = true);

	void printVector(const std::string& name, const glm::vec3& vec, const char* unit = nullptr);
	void printValue(const std::string& name, const float& value, const char* unit = nullptr);
	void printBoolean(const std::string& name, const bool& value);
}
