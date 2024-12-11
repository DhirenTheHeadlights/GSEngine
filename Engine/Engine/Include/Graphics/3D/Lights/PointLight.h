
#pragma once

#include <imgui.h>

#include "Light.h"

#include "Graphics/3D/CubeMap.h"

namespace gse {
	class point_light final : public light {
	public:
		point_light(const vec3<>& color, const unitless& intensity, const vec3<length>& position, const unitless& constant, const unitless& linear, const unitless& quadratic, const unitless& ambientStrength)
			: light(color, intensity, light_type::point), m_position(position), m_constant(constant), m_linear(linear), m_quadratic(quadratic), m_ambient_strength(ambientStrength), m_shadow_map() {}

		void show_debug_menu(const std::shared_ptr<id>& light_id) override {
			debug::add_imgui_callback([this, light_id] {
				ImGui::Begin(std::string("Point Light " + light_id->tag).c_str());
				ImGui::ColorEdit3("Color", &m_color.as_default_units().x);
				debug::unit_slider("Intensity", m_intensity, unitless(0.0f), unitless(100.0f));
				debug::unit_slider("Constant", m_constant, unitless(0.0f), unitless(1.0f));
				debug::unit_slider("Linear", m_linear, unitless(0.0f), unitless(0.1f));
				debug::unit_slider("Quadratic", m_quadratic, unitless(0.0f), unitless(0.1f));
				debug::unit_slider("Ambient Strength", m_ambient_strength, unitless(0.0f), unitless(10.0f));
				ImGui::SliderFloat3("Position", &m_position.as_default_units().x, -100.0f, 100.0f);
				ImGui::End();
				});
		}

		light_render_queue_entry get_render_queue_entry() const override {
			return { m_shadow_map.get_texture_id(), m_shadow_map.get_frame_buffer_id(), light_type::point, m_color, m_intensity, m_position, vec3(), m_constant, m_linear, m_quadratic, angle(), angle(), m_ambient_strength, m_near_plane, m_far_plane };
		}

		cube_map& get_shadow_map() { return m_shadow_map; }
	private:
		vec3<length> m_position;
		unitless m_constant;
		unitless m_linear;
		unitless m_quadratic;
		unitless m_ambient_strength;

		cube_map m_shadow_map;
	};
}