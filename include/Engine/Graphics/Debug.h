#pragma once

#include <imgui.h>

#include "Engine/Physics/Vector/Vec3.h"

namespace Engine::Debug {
	void setUpImGui();
	void updateImGui();
	void renderImGui();

	template <IsQuantityOrUnit T>
	void printVector(const std::string& text, const Vec3<T> vec) {
		if constexpr (IsUnit<T>) {
			ImGui::InputFloat3(
				text + T().units(),
				&vec.rawVec3()[0]
			);
		}
		else if constexpr (IsQuantity<T>) {
			ImGui::InputFloat3(
				text.c_str() + T().asDefaultUnit().units(),
				&vec.rawVec3()[0]
			);
		}
		else {
			//static_assert(false, "T must be a Quantity or a Unit");
		}
	}
}
