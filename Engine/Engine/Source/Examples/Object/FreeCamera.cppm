export module gse.examples:free_camera;

import std;

import gse.runtime;
import gse.utility;
import gse.graphics;
import gse.physics;
import gse.platform;

export namespace gse {
	class free_camera final : public hook<entity> {
	public:
		struct params {
			vec3<length> initial_position = vec3<length>(0.f, 0.f, 5.f);
		};

		free_camera(const params& p) : m_initial_position(p.initial_position) {}

		auto initialize() -> void override {
			renderer::camera().set_position(m_initial_position);

			m_w = actions::add<"FreeCamera_Move_Forward">(key::w);
			m_a = actions::add<"FreeCamera_Move_Left">(key::a);
			m_s = actions::add<"FreeCamera_Move_Backward">(key::s);
			m_d = actions::add<"FreeCamera_Move_Right">(key::d);
			m_space = actions::add<"FreeCamera_Move_Up">(key::space);
			m_ctrl = actions::add<"FreeCamera_Move_Down">(key::left_control);

			m_move_axis = find_or_generate_id("FreeCamera_Move");
			actions::add_axis2_actions({
				.left = m_a,
				.right = m_d,
				.back = m_s,
				.fwd = m_w,
				.scale = 1.f,
				.id = m_move_axis
			});
		}

		auto update() -> void override {
			auto& camera = renderer::camera();

			const auto v = actions::axis2_v(m_move_axis);
			const float lift = (actions::held(m_space) ? 1.f : 0.f) - (actions::held(m_ctrl) ? 1.f : 0.f);

			camera.move(camera.direction_relative_to_origin({v.x(), lift, v.y()}) * 100.f);
		}
	private:
		vec3<length> m_initial_position;

		actions::index m_w, m_a, m_s, m_d, m_space, m_ctrl;
		id m_move_axis;
	};
}
