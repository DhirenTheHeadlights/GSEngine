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
        int lightType;        // Offset 0, Size 4 bytes
        float padding1[3];    // Offset 4, Size 12 bytes (Padding to align to 16 bytes)
        // Offset now at 16 bytes

        glm::vec3 position;   // Offset 16, Size 12 bytes
        float padding2;       // Offset 28, Size 4 bytes (Padding to align to 16 bytes)
        // Offset now at 32 bytes

        glm::vec3 direction;  // Offset 32, Size 12 bytes
        float padding3;       // Offset 44, Size 4 bytes (Padding to align to 16 bytes)
        // Offset now at 48 bytes

        glm::vec3 color;      // Offset 48, Size 12 bytes
        float padding4;       // Offset 60, Size 4 bytes (Padding to align to 16 bytes)
        // Offset now at 64 bytes

        float intensity;      // Offset 64, Size 4 bytes
        float constant;       // Offset 68, Size 4 bytes
        float linear;         // Offset 72, Size 4 bytes
        float quadratic;      // Offset 76, Size 4 bytes
        // Offset now at 80 bytes

        float cutOff;           // Offset 80, Size 4 bytes
        float outerCutOff;      // Offset 84, Size 4 bytes
        float ambientStrength;  // Offset 88, Size 4 bytes
        float padding5;         // Offset 92, Size 4 bytes (Padding to align total size to multiple of 16 bytes)
        // Total size now 96 bytes
    };

    struct LightRenderQueueEntry {
		std::string shaderKey = "Emissive";
		LightShaderEntry shaderEntry;

        // Constructor for Directional Light
        LightRenderQueueEntry(const LightType type, const glm::vec3& color, const float intensity, const glm::vec3& direction, float ambientStrength)
            : shaderEntry({ static_cast<int>(type), { 0, 0, 0 }, { 0, 0, 0 }, 0, direction, 0, color, intensity, 0, 0, 0, 0, 0, 0, ambientStrength, 0 }) {}

        // Constructor for Point Light
        LightRenderQueueEntry(const LightType type, const glm::vec3& color, const float intensity, const glm::vec3& position,
                              const float constant, const float linear, const float quadratic, const float ambientStrength)
			: shaderEntry({ static_cast<int>(type), { 0, 0, 0 }, position, 0, {0, 0, 0}, 0, color, intensity, constant, linear, quadratic, 0, 0, 0, ambientStrength, 0 }) {}

        // Constructor for Spotlight
        LightRenderQueueEntry(const LightType type, const glm::vec3& color, const float intensity, const glm::vec3& position, const glm::vec3& direction,
                              const float constant, const float linear, const float quadratic, const float cutOff, const float outerCutOff, float ambientStrength)
			: shaderEntry({ static_cast<int>(type), { 0, 0, 0 }, position, 0, direction, 0, color, intensity, constant, linear, quadratic, 0, cutOff, outerCutOff, ambientStrength, 0 }) {}
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