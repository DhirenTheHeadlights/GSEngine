
#pragma once

#include <imgui.h>

#include "Light.h"

namespace Engine {
	class PointLight : public Light {
	public:
		PointLight(const Vec3<>& color, const Unitless& intensity, const Vec3<Length>& position, const Unitless& constant, const Unitless& linear, const Unitless& quadratic, const Unitless& ambientStrength)
			: Light(color, intensity, LightType::Point), position(position), constant(constant), linear(linear), quadratic(quadratic), ambientStrength(ambientStrength) {}

		Vec3<Length> position;
		Unitless constant;
		Unitless linear;
		Unitless quadratic;
		Unitless ambientStrength;

		void showDebugMenu() override {
			Debug::addImguiCallback([this] {
				ImGui::Begin("Point Light");
				ImGui::ColorEdit3("Color", &color.asDefaultUnits().x);
				Debug::unitSlider("Intensity", intensity, unitless(0.0f), unitless(100.0f));
				Debug::unitSlider("Constant", constant, unitless(0.0f), unitless(1.0f));
				Debug::unitSlider("Linear", linear, unitless(0.0f), unitless(0.1f));
				Debug::unitSlider("Quadratic", quadratic, unitless(0.0f), unitless(0.1f));
				Debug::unitSlider("Ambient Strength", ambientStrength, unitless(0.0f), unitless(10.0f));
				ImGui::SliderFloat3("Position", &position.asDefaultUnits().x, -100.0f, 100.0f);
				ImGui::End();
				});
		}

		LightRenderQueueEntry getRenderQueueEntry() const override {
			return { LightType::Point, color, intensity, position, constant, linear, quadratic, ambientStrength };
		}
	};
}