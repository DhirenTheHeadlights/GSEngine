#pragma once

#include <imgui.h>

#include "Light.h"

namespace Engine {
	class PointLight : public Light {
	public:
		PointLight(const glm::vec3& position, const glm::vec3& color, const float intensity, const float constant, const float linear, const float quadratic, const float ambientStrength)
			: Light(color, intensity, LightType::Point), position(position), constant(constant), linear(linear), quadratic(quadratic), ambientStrength(ambientStrength) {}

		float constant;
		float linear;
		float quadratic;
		float ambientStrength;
		glm::vec3 position;

		void showDebugMenu() override {
			ImGui::Begin("Point Light");
			ImGui::Text("Position: (%.2f, %.2f, %.2f)", position.x, position.y, position.z);
			ImGui::ColorEdit3("Color", &color[0]);
			ImGui::SliderFloat("Intensity", &intensity, 0.0f, 100.0f);
			//ImGui::SliderFloat("Ambient Strength", &ambientStrength, 0.0f, 1.0f);
			ImGui::SliderFloat("Constant", &constant, 0.0f, 1.0f);
			ImGui::SliderFloat("Linear", &linear, 0.0f, 1.0f);
			ImGui::SliderFloat("Quadratic", &quadratic, 0.0f, 1.0f);
			ImGui::End();
		}

		LightRenderQueueEntry getRenderQueueEntry() const override {
			return { LightType::Point, color, intensity, position, constant, linear, quadratic, ambientStrength };
		}
	};
}