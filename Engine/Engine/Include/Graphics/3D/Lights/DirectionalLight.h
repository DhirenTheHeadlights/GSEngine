#pragma once

#include <imgui_internal.h>

#include "Light.h"

namespace gse {
	class directional_light final : public light {
	public:
		directional_light(const vec3<>& color, const unitless& intensity, const vec3<>& direction, const unitless& ambient_strength)
			: light(color, intensity, light_type::directional), m_direction(direction), m_ambient_strength(ambient_strength) {}

		void show_debug_menu(const std::shared_ptr<id>& light_id) override {
			debug::add_imgui_callback([this, light_id] {
				ImGui::Begin(std::string("Directional Light " + light_id->tag).c_str());
				debug::unit_slider("Intensity", m_intensity, unitless(0.0f), unitless(100.0f));
				debug::unit_slider("Ambient Strength", m_ambient_strength, unitless(0.0f), unitless(10.0f));
				ImGui::SliderFloat3("Direction", &m_direction.as_default_units().x, -1.0f, 1.0f);
				ImGui::End();
				});
		}	
		
		light_render_queue_entry get_render_queue_entry() const override {
			return { m_depth_map, m_depth_map_fbo, light_type::directional, m_color, m_intensity, vec3<length>(), m_direction, unitless(), unitless(), unitless(), angle(), angle(), m_ambient_strength, m_near_plane, m_far_plane };
		}

		void set_depth_map(const GLuint depth_map, const GLuint depth_map_fbo) override {
			m_depth_map = depth_map;
			m_depth_map_fbo = depth_map_fbo;
		}
	private:
		vec3<> m_direction;
		unitless m_ambient_strength;

		GLuint m_depth_map = 0;
		GLuint m_depth_map_fbo = 0;
	};
}