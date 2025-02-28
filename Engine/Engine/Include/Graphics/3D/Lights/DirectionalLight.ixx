module;

#include <imgui.h>
#include <string>

export module gse.graphics.directional_light;

import gse.graphics.light;
import gse.physics.math;
import gse.graphics.debug;
import gse.graphics.cube_map;

export namespace gse {
	class directional_light final : public light {
	public:
		directional_light(const unitless::vec3& color, const float intensity, const unitless::vec3& direction, const float ambient_strength)
			: light(color, intensity, light_type::directional), m_direction(direction), m_ambient_strength(ambient_strength) {
		}

		auto show_debug_menu(const std::string_view& name, std::uint32_t parent_id) -> void override {
			debug::add_imgui_callback([this, name, parent_id] {
				ImGui::Begin(std::string("Directional Light " + std::string(name) + ": " + std::to_string(parent_id)).c_str());
				debug::unit_slider("Intensity", m_intensity, 0.0f, 100.0f);
				debug::unit_slider("Ambient Strength", m_ambient_strength, 0.0f, 10.0f);
				ImGui::SliderFloat3("Direction", &m_direction.x, -1.0f, 1.0f);
				ImGui::End();
				});
		}

		auto get_render_queue_entry() const -> light_render_queue_entry override {
			return { light_type::directional, m_color, m_intensity, vec3<length>(), m_direction, 0, 0, 0, angle(), angle(), m_ambient_strength, m_near_plane, m_far_plane, m_ignore_list_id };
		}
	private:
		unitless::vec3 m_direction;
		float m_ambient_strength;
	};
}