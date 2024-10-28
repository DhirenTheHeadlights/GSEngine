#pragma once

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
	};
}