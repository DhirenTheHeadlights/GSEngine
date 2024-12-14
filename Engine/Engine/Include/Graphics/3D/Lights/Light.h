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

        GLuint depth_map = 0;
        GLuint depth_map_fbo = 0;

        length near_plane = meters(0.1f);
        length far_plane = meters(1000.0f);

        light_render_queue_entry(
            GLuint depth_map,
            GLuint depth_map_fbo,
            light_type type,
            const vec3<>& color,
            const unitless& intensity,
            const vec3<length>& position = vec3<length>(),   // Default: No position for non-point lights
            const vec3<>& direction = vec3(),                // Default: No direction for non-directional lights
            const unitless& constant = 1.0f,                 // Default: No attenuation
            const unitless& linear = 0.0f,
            const unitless& quadratic = 0.0f,
            const angle& cut_off = degrees(0.0f),            // Default: No spotlight cutoff
            const angle& outer_cut_off = degrees(0.0f),	     // Default: No spotlight outer cutoff
            const unitless& ambient_strength = 0.0f,         // Default: No ambient strength
            const length& near_plane = meters(0.1f),         // Default: Near plane for shadow mapping
            const length& far_plane = meters(1000.0f)        // Default: Far plane for shadow mapping
        )
            : shader_entry({
                    static_cast<int>(type),
                    {0, 0, 0},
                    position.as_default_units(),
                    0,
                    direction.as_default_units(),
                    0,
                    color.as_default_units(),
                    intensity,
                    constant,
                    linear,
                    quadratic,
                    std::cos(cut_off.as<units::radians>()),
                    std::cos(outer_cut_off.as<units::radians>()),
                    ambient_strength,
                    0
                }),
            depth_map(depth_map),
            depth_map_fbo(depth_map_fbo),
            near_plane(near_plane),
            far_plane(far_plane)
        {}
    };

	class light {
	public:
		light() = default;
		virtual ~light() = default;
		virtual light_render_queue_entry get_render_queue_entry() const = 0;
        virtual void show_debug_menu(const std::shared_ptr<id>& light_id) = 0;

		light_type get_type() const { return m_type; }
		virtual void set_depth_map(GLuint depth_map, GLuint depth_map_fbo) {}
		virtual void set_position(const vec3<length>& position) {}
	protected:
		light(const vec3<>& color, const unitless& intensity, const light_type type)
			: m_color(color), m_intensity(intensity), m_type(type) {}
	
		vec3<> m_color;
		unitless m_intensity = 1.0f;
		light_type m_type;

		length m_near_plane = meters(10.f);
		length m_far_plane = meters(1000.0f);
	};
}