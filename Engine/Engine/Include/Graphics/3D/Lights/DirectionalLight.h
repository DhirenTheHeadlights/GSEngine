
#pragma once

#include <imgui_internal.h>

#include "Light.h"

namespace Engine {
	class DirectionalLight : public Light {
	public:
		DirectionalLight(const Vec3<>& color, const Unitless& intensity, const Vec3<>& direction, const Unitless& ambientStrength)
			: Light(color, intensity, LightType::Directional), direction(direction), ambientStrength(ambientStrength) {}

		void showDebugMenu(const std::shared_ptr<ID>& lightID) override {
			Debug::addImguiCallback([this, lightID] {
				ImGui::Begin(std::string("Directional Light " + lightID->tag).c_str());
				Debug::unitSlider("Intensity", intensity, unitless(0.0f), unitless(100.0f));
				Debug::unitSlider("Ambient Strength", ambientStrength, unitless(0.0f), unitless(10.0f));
				ImGui::SliderFloat3("Direction", &direction.asDefaultUnits().x, -1.0f, 1.0f);
				ImGui::End();
				});
		}	
		
		LightRenderQueueEntry getRenderQueueEntry() const override {
			return { depthMap, depthMapFBO, LightType::Directional, color, intensity, Vec3<Length>(), direction, 0.0f, 0.0f, 0.0f, degrees(0.0f), degrees(0.0f), ambientStrength };
		}

		void setDepthMap(const GLuint depthMap, const GLuint depthMapFBO) override {
			this->depthMap = depthMap;
			this->depthMapFBO = depthMapFBO;
		}
	private:
		Vec3<> direction;
		Unitless ambientStrength;

		GLuint depthMap = 0;
		GLuint depthMapFBO = 0;
	};
}