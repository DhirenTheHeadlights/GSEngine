module;

#include <imgui.h>

export module gse.graphics.point_light;

import gse.graphics.light;
import gse.physics.math.vector;
import gse.physics.math.units;
import gse.graphics.debug;
import gse.graphics.cube_map;

export namespace gse {
	class point_light final : public light {
	public:
		point_light(const vec3<>& color, const unitless& intensity, const vec3<length>& position, const unitless& constant, const unitless& linear, const unitless& quadratic, const unitless& ambient_strength)
			: light(color, intensity, light_type::point), m_position(position), m_constant(constant), m_linear(linear), m_quadratic(quadratic), m_ambient_strength(ambient_strength), m_shadow_map() {}

		auto show_debug_menu(const std::string_view& name, std::uint32_t parent_id) -> void override {
			debug::add_imgui_callback([this, name, parent_id] {
				ImGui::Begin(std::string("Point Light " + std::string(name) + ": " + std::to_string(parent_id)).c_str());
				ImGui::ColorEdit3("Color", &m_color.as_default_units().x);
				debug::unit_slider("Intensity", m_intensity, unitless(0.0f), unitless(100.0f));
				debug::unit_slider("Constant", m_constant, unitless(0.0f), unitless(1.0f));
				debug::unit_slider("Linear", m_linear, unitless(0.0f), unitless(0.1f));
				debug::unit_slider("Quadratic", m_quadratic, unitless(0.0f), unitless(0.1f));
				debug::unit_slider("Ambient Strength", m_ambient_strength, unitless(0.0f), unitless(10.0f));
				ImGui::SliderFloat3("Position", &m_position.as_default_units().x, -1000.0f, 1000.0f);
				ImGui::End();
				});
		}

		auto get_render_queue_entry() const -> light_render_queue_entry override {
			return { m_shadow_map.get_texture_id(), m_shadow_map.get_frame_buffer_id(), light_type::point, m_color, m_intensity, m_position, vec3(), m_constant, m_linear, m_quadratic, angle(), angle(), m_ambient_strength, m_near_plane, m_far_plane, m_ignore_list_id };
		}

		auto get_shadow_map() -> cube_map& { return m_shadow_map; }

		auto set_position(const vec3<length>& position) -> void override {
			m_position = position;
		}
	private:
		vec3<length> m_position;
		unitless m_constant;
		unitless m_linear;
		unitless m_quadratic;
		unitless m_ambient_strength;

		cube_map m_shadow_map;
	};
}