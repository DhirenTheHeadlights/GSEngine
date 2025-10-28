module;

#include "GLFW/glfw3.h"

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

			m_w = actions::add("FreeCamera_Move_Forward", key::w);
			m_a = actions::add("FreeCamera_Move_Left", key::a);
			m_s = actions::add("FreeCamera_Move_Backward", key::s);
			m_d = actions::add("FreeCamera_Move_Right", key::d);
			m_space = actions::add("FreeCamera_Move_Up", key::space);
			m_ctrl = actions::add("FreeCamera_Move_Down", key::left_control);
		}

		auto update() -> void override {
			auto& camera = renderer::camera();
			// Move Forward (-Z)
			if (actions::held(m_w)) {
				camera.move(camera.direction_relative_to_origin({ 0.f, 0.f, -100.f }));
			}
			// Move Backward (+Z)
			if (actions::held(m_s)) {
				camera.move(camera.direction_relative_to_origin({ 0.f, 0.f, 100.f }));
			}
			// Strafe Left (-X)
			if (actions::held(m_a)) {
				camera.move(camera.direction_relative_to_origin({ -100.f, 0.f, 0.f }));
			}
			// Strafe Right (+X)
			if (actions::held(m_d)) {
				camera.move(camera.direction_relative_to_origin({ 100.f, 0.f, 0.f }));
			}
			// Move Up (+Y)
			if (actions::held(m_space)) {
				camera.move(camera.direction_relative_to_origin({ 0.f, 100.f, 0.f }));
			}
			// Move Down (-Y)
			if (actions::held(m_ctrl)) {
				camera.move(camera.direction_relative_to_origin({ 0.f, -100.f, 0.f }));
			}
		}
	private:
		vec3<length> m_initial_position;

		actions::index m_w, m_a, m_s, m_d, m_space, m_ctrl;
	};
}