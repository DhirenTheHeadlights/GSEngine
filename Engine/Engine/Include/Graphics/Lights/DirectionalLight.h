#pragma once

#include <imgui_internal.h>

#include "Light.h"

namespace Engine {
	class DirectionalLight : public Light {
	public:
		DirectionalLight(const glm::vec3& color, const float intensity, const glm::vec3& direction)
			: Light(color, intensity, LightType::Directional), direction(direction) {}

		LightRenderQueueEntry getRenderQueueEntry() const override {
			return { LightType::Directional, color, intensity, direction };
		}

		glm::vec3 direction;

		void showDebugMenu() override {
			Debug::createWindow("Directional Light");
			ImGui::ColorEdit3("Color", &color[0]);
			ImGui::SliderFloat("Intensity", &intensity, 0.0f, 100.0f);
			ImGui::SliderFloat3("Direction", &direction[0], -1.0f, 1.0f);
			ImGui::End();
		}
	};
}