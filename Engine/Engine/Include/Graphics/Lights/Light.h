#pragma once

#include <glm/glm.hpp>

namespace Engine {
    enum class LightType {
        Directional,
        Point,
        Spot
    };


    struct LightRenderQueueEntry {
		std::string shaderKey = "Lighting";
		LightType type;
        glm::vec3 color;
        float intensity;

        glm::vec3 position;        // Only applicable for Point and Spotlights
        glm::vec3 direction;       // Only applicable for Directional and Spotlights

        // Attenuation parameters for Point and Spotlights
        float constant = 1.0f;
        float linear = 0.0f;
        float quadratic = 0.0f;

        // Spot light-specific parameters
        float cutOff = 0.0f;
        float outerCutOff = 0.0f;

        // Constructor for Directional Light
        LightRenderQueueEntry(const LightType type, const glm::vec3& color, const float intensity, const glm::vec3& direction)
            : type(type), color(color), intensity(intensity), direction(direction) {}

        // Constructor for Point Light
        LightRenderQueueEntry(const LightType type, const glm::vec3& color, const float intensity, const glm::vec3& position,
                              const float constant, const float linear, const float quadratic)
            : type(type), color(color), intensity(intensity), position(position), constant(constant), linear(linear), quadratic(quadratic) {}

        // Constructor for Spotlight
        LightRenderQueueEntry(const LightType type, const glm::vec3& color, const float intensity, const glm::vec3& position, const glm::vec3& direction,
                              const float constant, const float linear, const float quadratic, const float cutOff, const float outerCutOff)
            : type(type), color(color), intensity(intensity), position(position), direction(direction),
            constant(constant), linear(linear), quadratic(quadratic), cutOff(cutOff), outerCutOff(outerCutOff) {}
    };

	class Light {
	public:
		Light() = default;
		virtual ~Light() = default;
		virtual LightRenderQueueEntry getRenderQueueEntry() const = 0;

		LightType getType() const { return type; }
	protected:
		Light(const glm::vec3& color, const float intensity, const LightType type) : color(color), intensity(intensity), type(type) {}
	
		glm::vec3 color;
		float intensity = 1.0f;
		LightType type;
	};
}