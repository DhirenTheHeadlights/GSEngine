#pragma once

#include "Light.h"

namespace Engine {
	class PointLight : public Light {
	public:
		PointLight(const glm::vec3& position, const glm::vec3& color, const float intensity, const float constant, const float linear, const float quadratic)
			: Light(color, intensity, LightType::Point), position(position), constant(constant), linear(linear), quadratic(quadratic) {}

		float constant;
		float linear;
		float quadratic;
		glm::vec3 position;

		LightRenderQueueEntry getRenderQueueEntry() const override {
			return { LightType::Point, color, intensity, position, constant, linear, quadratic };
		}
	};
}