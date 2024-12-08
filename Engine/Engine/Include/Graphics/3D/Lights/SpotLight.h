#pragma once

#include "Light.h"

namespace Engine {
	class SpotLight : public Light {
	public:
		SpotLight(const Vec3<Length>& position, const Vec3<>& direction, const Vec3<>& color, const Unitless& intensity, const Unitless& constant, const Unitless& linear, const Unitless& quadratic, const Angle& cutOff, const Angle& outerCutOff, const Unitless& ambientStrength)
			: Light(color, intensity, LightType::Spot), constant(constant), linear(linear), quadratic(quadratic),
			  cutOff(cutOff), outerCutOff(outerCutOff), position(position), direction(direction), ambientStrength(ambientStrength) {}

		void showDebugMenu(const std::shared_ptr<ID>& lightID) override {
			Debug::addImguiCallback([this, lightID] {
				ImGui::Begin(std::string("Spot Light " + lightID->tag).c_str());
				Debug::unitSlider("Intensity", intensity, unitless(0.0f), unitless(1000.0f));
				Debug::unitSlider("Constant", constant, unitless(0.0f), unitless(1.0f));
				Debug::unitSlider("Linear", linear, unitless(0.0f), unitless(1.0f));
				Debug::unitSlider("Quadratic", quadratic, unitless(0.0f), unitless(1.0f));
				Debug::unitSlider<Angle, Degrees>("Cut Off", cutOff, degrees(0.0f), degrees(90.0f));
				Debug::unitSlider<Angle, Degrees>("Outer Cut Off", outerCutOff, degrees(0.0f), degrees(90.0f));
				Debug::unitSlider("Ambient Strength", ambientStrength, unitless(0.0f), unitless(1.0f));
				ImGui::SliderFloat3("Direction", &direction.asDefaultUnits().x, -1.0f, 1.0f);
				ImGui::SliderFloat3("Position", &position.asDefaultUnits().x, -500.0f, 500.0f);
				ImGui::End();
				});
		}

		LightRenderQueueEntry getRenderQueueEntry() const override {
			return { LightType::Spot, color, intensity, position, direction, constant, linear, quadratic, cutOff, outerCutOff, ambientStrength };
		}
	private:
		Unitless constant;
		Unitless linear;
		Unitless quadratic;
		Angle cutOff;
		Angle outerCutOff;
		Vec3<Length> position;
		Vec3<> direction;
		Unitless ambientStrength;
	};
}