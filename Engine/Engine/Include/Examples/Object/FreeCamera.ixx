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

		free_camera(const id& owner_id, registry* reg, const params& p) : hook(owner_id, reg), m_initial_position(p.initial_position) {}

		auto initialize() -> void override {
			renderer::camera().set_position(m_initial_position);
		}

		auto update() -> void override {
			auto& camera = renderer::camera();
			// Move Forward (-Z)
			if (input::get_keyboard().keys[GLFW_KEY_W].held) {
				camera.move(camera.direction_relative_to_origin({ 0.f, 0.f, -1.f }));
			}
			// Move Backward (+Z)
			if (input::get_keyboard().keys[GLFW_KEY_S].held) {
				camera.move(camera.direction_relative_to_origin({ 0.f, 0.f, 1.f }));
			}
			// Strafe Left (-X)
			if (input::get_keyboard().keys[GLFW_KEY_A].held) {
				camera.move(camera.direction_relative_to_origin({ -1.f, 0.f, 0.f }));
			}
			// Strafe Right (+X)
			if (input::get_keyboard().keys[GLFW_KEY_D].held) {
				camera.move(camera.direction_relative_to_origin({ 1.f, 0.f, 0.f }));
			}
			// Move Up (+Y)
			if (input::get_keyboard().keys[GLFW_KEY_SPACE].held) {
				camera.move(camera.direction_relative_to_origin({ 0.f, 1.f, 0.f }));
			}
			// Move Down (-Y)
			if (input::get_keyboard().keys[GLFW_KEY_LEFT_CONTROL].held) {
				camera.move(camera.direction_relative_to_origin({ 0.f, -1.f, 0.f }));
			}
		}
	private:
		vec3<length> m_initial_position;
	};
}