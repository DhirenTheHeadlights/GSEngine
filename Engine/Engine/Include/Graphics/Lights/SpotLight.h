#pragma once

#include "Light.h"

namespace Engine {
	class SpotLight : public Light {
	public:
		SpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, const float intensity, const float constant, const float linear, const float quadratic, const float cutOff, const float outerCutOff, const float ambientStrength)
			: Light(color, intensity, LightType::Spot), constant(constant), linear(linear), quadratic(quadratic),
			  cutOff(cutOff), outerCutOff(outerCutOff), position(position), direction(direction), ambientStrength(ambientStrength) {}

		float constant;
		float linear;
		float quadratic;
		float cutOff;
		float outerCutOff;
		glm::vec3 position;
		glm::vec3 direction;
		float ambientStrength;

		void showDebugMenu() override {
			ImGui::Begin("Spot Light");
			ImGui::ColorEdit3("Color", &color[0]);
			ImGui::SliderFloat("Intensity", &intensity, 0.0f, 10.0f);
			ImGui::SliderFloat("Ambient Strength", &ambientStrength, 0.0f, 1.0f);
			ImGui::SliderFloat("Constant", &constant, 0.0f, 1.0f);
			ImGui::SliderFloat("Linear", &linear, 0.0f, 1.0f);
			ImGui::SliderFloat("Quadratic", &quadratic, 0.0f, 1.0f);
			ImGui::SliderFloat("Cut Off", &cutOff, -0.01f, 0.01f);
			ImGui::SliderFloat("Outer Cut Off", &outerCutOff, 0.0f, 1.0f);
			ImGui::End();
		}

		LightRenderQueueEntry getRenderQueueEntry() const override {
			return { LightType::Spot, color, intensity, position, direction, constant, linear, quadratic, cutOff, outerCutOff, ambientStrength };
		}
	};
}