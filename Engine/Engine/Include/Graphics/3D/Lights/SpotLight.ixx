module;

#include <imgui.h>

export module gse.graphics.spot_light;

import std;

import gse.graphics.light;
import gse.physics.math.vector;
import gse.physics.math.units;
import gse.graphics.debug;
import gse.graphics.cube_map;

export namespace gse {
	class spot_light final : public light {
	public:
		spot_light(const vec3<>& color, const unitless& intensity, const vec3<length>& position, const vec3<>& direction, const unitless& constant, const unitless& linear, const unitless& quadratic, const angle& cut_off, const angle& outer_cut_off, const unitless& ambient_strength)
			: light(color, intensity, light_type::spot), m_constant(constant), m_linear(linear), m_quadratic(quadratic),
			  m_cut_off(cut_off), m_outer_cut_off(outer_cut_off), m_position(position), m_direction(direction), m_ambient_strength(ambient_strength) {}

		auto show_debug_menu(const std::string_view& name, std::uint32_t parent_id) -> void override {
			debug::add_imgui_callback([this, name, parent_id] {
				ImGui::Begin(std::string("Spot Light " + std::string(name) + ": " + std::to_string(parent_id)).c_str());
				ImGui::ColorEdit3("Color", &m_color.as_default_units().x);
				debug::unit_slider("Intensity", m_intensity, unitless(0.0f), unitless(10000.0f));
				debug::unit_slider("Constant", m_constant, unitless(0.0f), unitless(1.0f));
				debug::unit_slider("Linear", m_linear, unitless(0.0f), unitless(1.0f));
				debug::unit_slider("Quadratic", m_quadratic, unitless(0.0f), unitless(1.0f));
				debug::unit_slider<units::degrees>("Cut Off", m_cut_off, degrees(0.0f), m_outer_cut_off);
				debug::unit_slider<units::degrees>("Outer Cut Off", m_outer_cut_off, degrees(0.0f), degrees(90.0f));
				debug::unit_slider("Ambient Strength", m_ambient_strength, unitless(0.01f), unitless(0.1f));
				ImGui::SliderFloat3("Direction", &m_direction.as_default_units().x, -1.0f, 1.0f);
				ImGui::SliderFloat3("Position", &m_position.as_default_units().x, -500.0f, 500.0f);
				debug::unit_slider<units::meters>("Near Plane", m_near_plane, meters(0.1f), meters(100.0f));
				debug::unit_slider<units::meters>("Far Plane", m_far_plane, meters(100.0f), meters(1000.0f));
				ImGui::End();
				});
		}

		auto get_render_queue_entry() const -> light_render_queue_entry override {
			return { m_depth_map, m_depth_map_fbo, light_type::spot, m_color, m_intensity, m_position, m_direction, m_constant, m_linear, m_quadratic, m_cut_off > m_outer_cut_off ? m_outer_cut_off : m_cut_off, m_outer_cut_off, m_ambient_strength, m_near_plane, m_far_plane, m_ignore_list_id };
		}

		auto set_depth_map(const GLuint depth_map, const GLuint depth_map_fbo) -> void override {
			m_depth_map = depth_map;
			m_depth_map_fbo = depth_map_fbo;
		}

		auto set_position(const vec3<length>& position) -> void override {
			m_position = position;
		}
	private:
		unitless m_constant;
		unitless m_linear;
		unitless m_quadratic;
		angle m_cut_off;
		angle m_outer_cut_off;
		vec3<length> m_position;
		vec3<> m_direction;
		unitless m_ambient_strength;

		GLuint m_depth_map = 0;
		GLuint m_depth_map_fbo = 0;
	};
}