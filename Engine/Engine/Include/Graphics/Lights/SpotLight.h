#pragma once

#include "Light.h"

namespace Engine {
	class SpotLight : public Light {
	public:
		SpotLight(const Vec3<Length>& position, const Vec3<>& direction, const Vec3<>& color, const Unitless& intensity, const Unitless& constant, const Unitless& linear, const Unitless& quadratic, const Angle& cutOff, const Angle& outerCutOff, const Unitless& ambientStrength)
			: Light(color, intensity, LightType::Spot), constant(constant), linear(linear), quadratic(quadratic),
			  cutOff(cutOff), outerCutOff(outerCutOff), position(position), direction(direction), ambientStrength(ambientStrength) {}

		Unitless constant;
		Unitless linear;
		Unitless quadratic;
		Angle cutOff;
		Angle outerCutOff;
		Vec3<Length> position;
		Vec3<> direction;
		Unitless ambientStrength;

		void showDebugMenu() override {
			Debug::addImguiCallback([this] {
				ImGui::Begin("Spot Light");
				Debug::unitSlider("Intensity", intensity, unitless(0.0f), unitless(100.0f));
				Debug::unitSlider("Constant", constant, unitless(0.0f), unitless(1.0f));
				Debug::unitSlider("Linear", linear, unitless(0.0f), unitless(1.0f));
				Debug::unitSlider("Quadratic", quadratic, unitless(0.0f), unitless(1.0f));
				Debug::unitSlider<Angle, Degrees>("Cut Off", cutOff, degrees(-270.0f), degrees(180.0f));
				Debug::unitSlider<Angle, Degrees>("Outer Cut Off", outerCutOff, degrees(0.0f), degrees(90.0f));
				Debug::unitSlider("Ambient Strength", ambientStrength, unitless(0.0f), unitless(10.0f));
				ImGui::SliderFloat3("Direction", &direction.asDefaultUnits().x, -1.0f, 1.0f);
				ImGui::SliderFloat3("Position", &position.asDefaultUnits().x, -100.0f, 100.0f);
				ImGui::End();
				});
		}

		LightRenderQueueEntry getRenderQueueEntry() const override {
			return { LightType::Spot, color, intensity, position, direction, constant, linear, quadratic, cutOff, outerCutOff, ambientStrength };
		}
	};
}