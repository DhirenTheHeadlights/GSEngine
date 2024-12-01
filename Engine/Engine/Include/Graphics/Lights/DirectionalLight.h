
#pragma once

#include <imgui_internal.h>

#include "Light.h"

namespace Engine {
	class DirectionalLight : public Light {
	public:
		DirectionalLight(const Vec3<>& color, const Unitless& intensity, const Vec3<>& direction, const Unitless& ambientStrength)
			: Light(color, intensity, LightType::Directional), direction(direction), ambientStrength(ambientStrength) {}

		Vec3<> direction;
		Unitless ambientStrength;

		void showDebugMenu() override {
			Debug::addImguiCallback([this] {
				ImGui::Begin("Directional Light");
				Debug::unitSlider("Intensity", intensity, unitless(0.0f), unitless(100.0f));
				Debug::unitSlider("Ambient Strength", ambientStrength, unitless(0.0f), unitless(10.0f));
				ImGui::SliderFloat3("Direction", &direction.asDefaultUnits().x, -1.0f, 1.0f);
				ImGui::End();
				});
		}	
		
		LightRenderQueueEntry getRenderQueueEntry() const override {
			return { LightType::Directional, color, intensity, direction, ambientStrength };
		}
	};
}