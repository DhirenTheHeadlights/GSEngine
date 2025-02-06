module;

#include <glad/glad.h>

export module gse.graphics.light;

import std;
import glm;

import gse.core.id;
import gse.physics.math;
import gse.graphics.debug;

export namespace gse {
    enum class light_type : std::uint8_t {
        directional,
        point,
        spot
    };

    struct alignas(16) light_shader_entry {
        int light_type;             // Offset 0, Size 4 bytes
		glm::vec3 padding1;         // Offset 4, Size 12 bytes (Padding to align to 16 bytes)
        glm::vec3 position;         // Offset 16, Size 12 bytes
        float padding2;             // Offset 28, Size 4 bytes (Padding to align to 16 bytes)
        glm::vec3 direction;        // Offset 32, Size 12 bytes
        float padding3;             // Offset 44, Size 4 bytes (Padding to align to 16 bytes)
        glm::vec3 color;            // Offset 48, Size 12 bytes
		// We don't need padding here, not sure why - it ends up working correctly without it
        float intensity;            // Offset 64, Size 4 bytes
        float constant;             // Offset 68, Size 4 bytes
        float linear;               // Offset 72, Size 4 bytes
        float quadratic;            // Offset 76, Size 4 bytes
        float cut_off;              // Offset 80, Size 4 bytes
        float outer_cut_off;        // Offset 84, Size 4 bytes
        float ambient_strength;     // Offset 88, Size 4 bytes
        float padding5;             // Offset 92, Size 4 bytes (Padding to align total size to multiple of 16 bytes)
    };

    struct light_render_queue_entry {
        std::string shader_key = "Emissive";
        light_shader_entry shader_entry;
        const id* ignore_list_id = nullptr;

        std::uint32_t depth_map = 0;
        std::uint32_t depth_map_fbo = 0;

        length near_plane = meters(0.1f);
        length far_plane = meters(1000.0f);

        light_render_queue_entry(
            const std::uint32_t depth_map,
            const std::uint32_t depth_map_fbo,
            light_type type,
            const unitless::vec3& color,
            const float intensity,
            const vec3<length>& position = vec3<length>(),           // Default: No position for non-point lights
            const unitless::vec3& direction = unitless::vec3(),         // Default: No direction for non-directional lights
            const float constant = 1.0f,                     // Default: No attenuation
            const float linear = 0.0f,
            const float quadratic = 0.0f,
            const angle cut_off = degrees(0.0f),            // Default: No spotlight cutoff
            const angle outer_cut_off = degrees(0.0f),	        // Default: No spotlight outer cutoff
            const float ambient_strength = 0.0f,                     // Default: No ambient strength
            const length near_plane = meters(0.1f),             // Default: Near plane for shadow mapping
            const length far_plane = meters(1000.0f),          // Default: Far plane for shadow mapping
            const id* ignore_list_id = nullptr 			        // Default: No ignore list
        )
            : shader_entry({
                    .light_type = static_cast<int>(type),
                    .padding1 = { 0.f, 0.f, 0.f },
                    .position = to_glm_vec(position),
                    .padding2 = 0,
                    .direction = to_glm_vec(direction),
                    .padding3 = 0,
                    .color = to_glm_vec(color),
                    .intensity = intensity,
                    .constant = constant,
                    .linear = linear,
                    .quadratic = quadratic,
                    .cut_off = std::cos(cut_off.as<units::radians>()),
                    .outer_cut_off = std::cos(outer_cut_off.as<units::radians>()),
                    .ambient_strength = ambient_strength,
                    .padding5 = 0
                }),
            ignore_list_id(ignore_list_id),
            depth_map(depth_map),
            depth_map_fbo(depth_map_fbo),
            near_plane(near_plane),
            far_plane(far_plane) {
        }
    };

    class light {
    public:
        light() = default;
        virtual ~light() = default;

        virtual auto get_render_queue_entry() const->light_render_queue_entry = 0;
        virtual auto show_debug_menu(const std::string_view& name, std::uint32_t parent_id) -> void = 0;

        auto get_type() const -> light_type { return m_type; }

        virtual auto set_depth_map(std::uint32_t depth_map, std::uint32_t depth_map_fbo) -> void {}
        virtual auto set_position(const vec3<length>& position) -> void {}

        auto set_ignore_list_id(id* ignore_list_id) -> void { m_ignore_list_id = ignore_list_id; }
        auto get_ignore_list_id() const -> id* { return m_ignore_list_id; }
    protected:
        light(const unitless::vec3& color, const float& intensity, const light_type type)
            : m_color(color), m_intensity(intensity), m_type(type) {
        }

        unitless::vec3 m_color;
        float m_intensity = 1.0f;
        light_type m_type;

        length m_near_plane = meters(10.f);
        length m_far_plane = meters(1000.0f);

        id* m_ignore_list_id = nullptr;
    };
}