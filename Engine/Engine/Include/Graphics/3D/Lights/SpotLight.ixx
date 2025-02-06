module;

#include <imgui.h>

export module gse.graphics.spot_light;

import std;

import gse.graphics.light;
import gse.physics.math;
import gse.graphics.debug;
import gse.graphics.cube_map;

export namespace gse {
	class spot_light final : public light {
	public:
		spot_light(const unitless::vec3& color, const float intensity, const vec3<length>& position, const unitless::vec3& direction, const float constant, const float linear, const float quadratic, const angle cut_off, const angle outer_cut_off, const float ambient_strength)
			: light(color, intensity, light_type::spot), m_constant(constant), m_linear(linear), m_quadratic(quadratic),
			m_cut_off(cut_off), m_outer_cut_off(outer_cut_off), m_position(position), m_direction(direction), m_ambient_strength(ambient_strength) {
		}

		auto show_debug_menu(const std::string_view& name, std::uint32_t parent_id) -> void override {
			debug::add_imgui_callback([this, name, parent_id] {
				ImGui::Begin(std::string("Spot Light " + std::string(name) + ": " + std::to_string(parent_id)).c_str());
				ImGui::ColorEdit3("Color", &m_color.x);
				debug::unit_slider("Intensity", m_intensity, 0.0f, 10000.0f);
				debug::unit_slider("Constant", m_constant, 0.0f, 1.0f);
				debug::unit_slider("Linear", m_linear, 0.0f, 1.0f);
				debug::unit_slider("Quadratic", m_quadratic, 0.0f, 1.0f);
				debug::unit_slider<units::degrees>("Cut Off", m_cut_off, degrees(0.0f), m_outer_cut_off);
				debug::unit_slider<units::degrees>("Outer Cut Off", m_outer_cut_off, degrees(0.0f), degrees(90.0f));
				debug::unit_slider("Ambient Strength", m_ambient_strength, 0.01f, 0.1f);
				ImGui::SliderFloat3("Direction", &m_direction.x, -1.0f, 1.0f);
				debug::unit_slider<units::meters>("Near Plane", m_near_plane, meters(0.1f), meters(100.0f));
				debug::unit_slider<units::meters>("Far Plane", m_far_plane, meters(100.0f), meters(1000.0f));
				ImGui::End();
				});
		}

		auto get_render_queue_entry() const -> light_render_queue_entry override {
			return { m_depth_map, m_depth_map_fbo, light_type::spot, m_color, m_intensity, m_position, m_direction, m_constant, m_linear, m_quadratic, m_cut_off > m_outer_cut_off ? m_outer_cut_off : m_cut_off, m_outer_cut_off, m_ambient_strength, m_near_plane, m_far_plane, m_ignore_list_id };
		}

		auto set_depth_map(const std::uint32_t depth_map, const std::uint32_t depth_map_fbo) -> void override {
			m_depth_map = depth_map;
			m_depth_map_fbo = depth_map_fbo;
		}

		auto set_position(const vec3<length>& position) -> void override {
			m_position = position;
		}
	private:
		float m_constant;
		float m_linear;
		float m_quadratic;
		angle m_cut_off;
		angle m_outer_cut_off;
		vec3<length> m_position;
		unitless::vec3 m_direction;
		float m_ambient_strength;

		std::uint32_t m_depth_map = 0;
		std::uint32_t m_depth_map_fbo = 0;
	};
}