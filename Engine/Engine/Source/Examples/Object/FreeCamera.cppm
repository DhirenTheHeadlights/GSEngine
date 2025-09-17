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
		}

		auto update() -> void override {
			auto& camera = renderer::camera();
			// Move Forward (-Z)
			if (keyboard::held(key::w)) {
				camera.move(camera.direction_relative_to_origin({ 0.f, 0.f, -100.f }));
			}
			// Move Backward (+Z)
			if (keyboard::held(key::s)) {
				camera.move(camera.direction_relative_to_origin({ 0.f, 0.f, 100.f }));
			}
			// Strafe Left (-X)
			if (keyboard::held(key::a)) {
				camera.move(camera.direction_relative_to_origin({ -100.f, 0.f, 0.f }));
			}
			// Strafe Right (+X)
			if (keyboard::held(key::d)) {
				camera.move(camera.direction_relative_to_origin({ 100.f, 0.f, 0.f }));
			}
			// Move Up (+Y)
			if (keyboard::held(key::space)) {
				camera.move(camera.direction_relative_to_origin({ 0.f, 100.f, 0.f }));
			}
			// Move Down (-Y)
			if (keyboard::held(key::left_control)) {
				camera.move(camera.direction_relative_to_origin({ 0.f, -100.f, 0.f }));
			}
		}
	private:
		vec3<length> m_initial_position;
	};
}