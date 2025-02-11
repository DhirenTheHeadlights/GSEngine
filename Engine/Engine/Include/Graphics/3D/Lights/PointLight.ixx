module;

#include <imgui.h>
#include <string>

export module gse.graphics.point_light;

import gse.graphics.light;
import gse.physics.math;
import gse.graphics.debug;
import gse.graphics.cube_map;

export namespace gse {
	class point_light final : public light {
	public:
		point_light(const unitless::vec3& color, const float intensity, const vec3<length>& position, const float constant, const float linear, const float quadratic, const float ambient_strength)
			: light(color, intensity, light_type::point), m_position(position), m_constant(constant), m_linear(linear), m_quadratic(quadratic), m_ambient_strength(ambient_strength), m_shadow_map() {
		}

		auto show_debug_menu(const std::string_view& name, std::uint32_t parent_id) -> void override {
			debug::add_imgui_callback([this, name, parent_id] {
				ImGui::Begin(std::string("Point Light " + std::string(name) + ": " + std::to_string(parent_id)).c_str());
				ImGui::ColorEdit3("Color", &m_color.x);
				debug::unit_slider("Intensity", m_intensity, 0.f, 100.0f);
				debug::unit_slider("Constant", m_constant, 0.f, 1.0f);
				debug::unit_slider("Linear", m_linear, 0.f, 0.1f);
				debug::unit_slider("Quadratic", m_quadratic, 0.f, 0.1f);
				debug::unit_slider("Ambient Strength", m_ambient_strength, 0.f, 10.0f);
				ImGui::End();
				});
		}

		auto get_render_queue_entry() const -> light_render_queue_entry override {
			return { m_shadow_map.get_texture_id(), m_shadow_map.get_frame_buffer_id(), light_type::point, m_color, m_intensity, m_position, unitless::vec3(), m_constant, m_linear, m_quadratic, angle(), angle(), m_ambient_strength, m_near_plane, m_far_plane, m_ignore_list_id };
		}

		auto get_shadow_map() -> cube_map& { return m_shadow_map; }

		auto set_position(const vec3<length>& position) -> void override {
			m_position = position;
		}
	private:
		vec3<length> m_position;
		float m_constant;
		float m_linear;
		float m_quadratic;
		float m_ambient_strength;

		cube_map m_shadow_map;
	};
}