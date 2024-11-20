#pragma once

#include <glm/glm.hpp>

#include "Graphics/Debug.h"

namespace Engine {
    enum class LightType {
        Directional,
        Point,
        Spot
    };

    struct alignas(16) LightShaderEntry {
        int lightType;        // 4 bytes
        int padding1;         // 4 bytes padding to align to 8 bytes
        // Next available offset: 8 bytes
        glm::vec3 position;   // 12 bytes
        float padding2;       // 4 bytes padding to align to 16 bytes
        // Next available offset: 24 bytes
        glm::vec3 direction;  // 12 bytes
        float padding3;       // 4 bytes padding to align to 16 bytes
        // Next available offset: 40 bytes
        glm::vec3 color;      // 12 bytes
        float intensity;      // 4 bytes
        // Next available offset: 56 bytes
        float constant;       // 4 bytes
        float linear;         // 4 bytes
        float quadratic;      // 4 bytes
        float padding4;       // 4 bytes padding to align to 16 bytes
        // Next available offset: 72 bytes
        float cutOff;         // 4 bytes
        float outerCutOff;    // 4 bytes
        float ambientStrength; // 4 bytes
        float padding5;    // 4 bytes padding to make the struct size a multiple of 16 bytes
        // Total size: 88 bytes
    };

    struct LightRenderQueueEntry {
		std::string shaderKey = "Emissive";
		LightShaderEntry shaderEntry;

        // Constructor for Directional Light
        LightRenderQueueEntry(const LightType type, const glm::vec3& color, const float intensity, const glm::vec3& direction, float ambientStrength)
			: shaderEntry({ static_cast<int>(type), 0, {0, 0, 0}, 0, direction, 0, color, intensity, 0, 0, 0, 0, 0, 0, ambientStrength, 0 }) {}

        // Constructor for Point Light
        LightRenderQueueEntry(const LightType type, const glm::vec3& color, const float intensity, const glm::vec3& position,
                              const float constant, const float linear, const float quadratic, float ambientStrength)
			: shaderEntry({ static_cast<int>(type), 0, position, 0, {0, 0, 0}, 0, color, intensity, constant, linear, quadratic, 0, 0, 0, ambientStrength, 0 }) {}

        // Constructor for Spotlight
        LightRenderQueueEntry(const LightType type, const glm::vec3& color, const float intensity, const glm::vec3& position, const glm::vec3& direction,
                              const float constant, const float linear, const float quadratic, const float cutOff, const float outerCutOff, float ambientStrength)
			: shaderEntry({ static_cast<int>(type), 0, position, 0, direction, 0, color, intensity, constant, linear, quadratic, 0, cutOff, outerCutOff, ambientStrength, 0 }) {}
    };

	class Light {
	public:
		Light() = default;
		virtual ~Light() = default;
		virtual LightRenderQueueEntry getRenderQueueEntry() const = 0;
        virtual void showDebugMenu() = 0;

		LightType getType() const { return type; }
	protected:
		Light(const glm::vec3& color, const float intensity, const LightType type) : color(color), intensity(intensity), type(type) {}
	
		glm::vec3 color;
		float intensity = 1.0f;
		LightType type;
	};
}