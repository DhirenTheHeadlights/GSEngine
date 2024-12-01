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
		glm::vec3 padding1;   // Offset 4, Size 12 bytes (Padding to align to 16 bytes)
        glm::vec3 position;   // Offset 16, Size 12 bytes
        float padding2;       // Offset 28, Size 4 bytes (Padding to align to 16 bytes)
        glm::vec3 direction;  // Offset 32, Size 12 bytes
        float padding3;       // Offset 44, Size 4 bytes (Padding to align to 16 bytes)
        glm::vec3 color;      // Offset 48, Size 12 bytes
		// We don't need padding here, not sure why - it ends up working correctly without it
        float intensity;      // Offset 64, Size 4 bytes
        float constant;       // Offset 68, Size 4 bytes
        float linear;         // Offset 72, Size 4 bytes
        float quadratic;      // Offset 76, Size 4 bytes
        float cutOff;           // Offset 80, Size 4 bytes
        float outerCutOff;      // Offset 84, Size 4 bytes
        float ambientStrength;  // Offset 88, Size 4 bytes
        float padding5;         // Offset 92, Size 4 bytes (Padding to align total size to multiple of 16 bytes)
    };

    struct LightRenderQueueEntry {
		std::string shaderKey = "Emissive";
		LightShaderEntry shaderEntry;

        // Constructor for Directional Light
		LightRenderQueueEntry(const LightType type, const Vec3<>& color, const Unitless& intensity, const Vec3<>& direction, const Unitless& ambientStrength)
            : shaderEntry({ static_cast<int>(type), { 0, 0, 0 }, { 0, 0, 0 }, 0, direction.asDefaultUnits(), 0, color.asDefaultUnits(), intensity, 0, 0, 0, 0, 0, ambientStrength, 0 }) {}

        // Constructor for Point Light
		LightRenderQueueEntry(const LightType type, const Vec3<>& color, const Unitless& intensity, const Vec3<Length>& position, const Unitless& constant, const Unitless& linear, const Unitless& quadratic, const Unitless& ambientStrength)
			: shaderEntry({ static_cast<int>(type), { 0, 0, 0 }, position.asDefaultUnits(), 0, {0, 0, 0}, 0, color.asDefaultUnits(), intensity, constant, linear, quadratic, 0, 0, ambientStrength, 0 }) {}

        // Constructor for Spotlight
		LightRenderQueueEntry(const LightType type, const Vec3<>& color, const Unitless& intensity, const Vec3<Length>& position, const Vec3<>& direction, const Unitless& constant, const Unitless& linear, const Unitless& quadratic, const Angle& cutOff, const Angle& outerCutOff, const Unitless& ambientStrength)
            : shaderEntry({ static_cast<int>(type), { 0, 0, 0 }, position.asDefaultUnits(), 0, direction.asDefaultUnits(), 0, color.asDefaultUnits(), intensity, constant, linear, quadratic, std::cos(cutOff.as<Radians>()), std::cos(outerCutOff.as<Radians>()), ambientStrength, 0 }) {}
    };

	class Light {
	public:
		Light() = default;
		virtual ~Light() = default;
		virtual LightRenderQueueEntry getRenderQueueEntry() const = 0;
        virtual void showDebugMenu() = 0;

		LightType getType() const { return type; }
	protected:
		Light(const Vec3<>& color, const Unitless& intensity, const LightType type)
			: color(color), intensity(intensity), type(type) {}
	
		Vec3<> color;
		Unitless intensity = 1.0f;
		LightType type;
	};
}