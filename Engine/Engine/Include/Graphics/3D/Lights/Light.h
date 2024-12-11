#pragma once

#include <glm/glm.hpp>

#include "Graphics/Debug.h"

namespace gse {
    enum class light_type : std::uint8_t {
        directional,
        point,
        spot
    };

    struct alignas(16) light_shader_entry {
        int light_type;        // Offset 0, Size 4 bytes
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
        float cut_off;            // Offset 80, Size 4 bytes
        float outer_cut_off;      // Offset 84, Size 4 bytes
        float ambient_strength;   // Offset 88, Size 4 bytes
        float padding5;           // Offset 92, Size 4 bytes (Padding to align total size to multiple of 16 bytes)
    };

    struct light_render_queue_entry {
		std::string shader_key = "Emissive";
		light_shader_entry shader_entry;

        // Constructor for Directional Light
		light_render_queue_entry(const light_type type, const vec3<>& color, const unitless& intensity, const vec3<>& direction, const unitless& ambient_strength)
            : shader_entry({ static_cast<int>(type), { 0, 0, 0 }, { 0, 0, 0 }, 0, direction.as_default_units(), 0, color.as_default_units(), intensity, 0, 0, 0, 0, 0, ambient_strength, 0 }) {}

        // Constructor for Point Light
		light_render_queue_entry(const light_type type, const vec3<>& color, const unitless& intensity, const vec3<length>& position, const unitless& constant, const unitless& linear, const unitless& quadratic, const unitless& ambient_strength)
			: shader_entry({ static_cast<int>(type), { 0, 0, 0 }, position.as_default_units(), 0, {0, 0, 0}, 0, color.as_default_units(), intensity, constant, linear, quadratic, 0, 0, ambient_strength, 0 }) {}

        // Constructor for Spotlight
		light_render_queue_entry(const light_type type, const vec3<>& color, const unitless& intensity, const vec3<length>& position, const vec3<>& direction, const unitless& constant, const unitless& linear, const unitless& quadratic, const angle& cut_off, const angle& outer_cut_off, const unitless& ambient_strength)
            : shader_entry({ static_cast<int>(type), { 0, 0, 0 }, position.as_default_units(), 0, direction.as_default_units(), 0, color.as_default_units(), intensity, constant, linear, quadratic, std::cos(cut_off.as<units::radians>()), std::cos(outer_cut_off.as<units::radians>()), ambient_strength, 0 }) {}
    };

	class light {
	public:
		light() = default;
		virtual ~light() = default;
		virtual light_render_queue_entry get_render_queue_entry() const = 0;
        virtual void show_debug_menu(const std::shared_ptr<id>& light_id) = 0;

		light_type get_type() const { return m_type; }
	protected:
		light(const vec3<>& color, const unitless& intensity, const light_type type)
			: m_color(color), m_intensity(intensity), m_type(type) {}
	
		vec3<> m_color;
		unitless m_intensity = 1.0f;
		light_type m_type;
	};
}