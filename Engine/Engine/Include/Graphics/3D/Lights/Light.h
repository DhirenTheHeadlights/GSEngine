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

        GLuint depthMap = 0;
        GLuint depthMapFBO = 0;

        Length nearPlane = meters(0.1f);
        Length farPlane = meters(1000.0f);

        LightRenderQueueEntry(
			GLuint depthMap,
			GLuint depthMapFBO,
            LightType type,
            const Vec3<>& color,
            const Unitless& intensity,
            const Vec3<Length>& position = Vec3<Length>(),  // Default: No position for non-point lights
            const Vec3<>& direction = Vec3(),               // Default: No direction for non-directional lights
            const Unitless& constant = 1.0f,                // Default: No attenuation
            const Unitless& linear = 0.0f,
            const Unitless& quadratic = 0.0f,
            const Angle& cutOff = degrees(0.0f),            // Default: No spotlight cutoff
			const Angle& outerCutOff = degrees(0.0f),	    // Default: No spotlight outer cutoff
            const Unitless& ambientStrength = 0.0f,         // Default: No ambient strength
			const Length& nearPlane = meters(0.1f),         // Default: Near plane for shadow mapping
			const Length& farPlane = meters(1000.0f)        // Default: Far plane for shadow mapping
		)
    	: shaderEntry({
				static_cast<int>(type),
				{0, 0, 0},
				position.asDefaultUnits(),
				0,
				direction.asDefaultUnits(),
				0,
				color.asDefaultUnits(),
				intensity,
				constant,
				linear,
				quadratic,
				std::cos(cutOff.as<Radians>()),
				std::cos(outerCutOff.as<Radians>()),
				ambientStrength,
				0
			}),
    		depthMap(depthMap),
    		depthMapFBO(depthMapFBO),
			nearPlane(nearPlane),
			farPlane(farPlane)
		{}
    };

	class Light {
	public:
		Light() = default;
		virtual ~Light() = default;
		virtual LightRenderQueueEntry getRenderQueueEntry() const = 0;
        virtual void showDebugMenu(const std::shared_ptr<ID>& lightID) = 0;

		LightType getType() const { return type; }
		virtual void setDepthMap(const GLuint depthMap, const GLuint depthMapFBO) {}
	protected:
		Light(const Vec3<>& color, const Unitless& intensity, const LightType type)
			: color(color), intensity(intensity), type(type) {}
	
		Vec3<> color;
		Unitless intensity = 1.0f;
		LightType type;

		Length nearPlane = meters(0.1f);
		Length farPlane = meters(1000.0f);
	};
}