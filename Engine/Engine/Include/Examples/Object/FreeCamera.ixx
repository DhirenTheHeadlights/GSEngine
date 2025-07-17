module;

#include "GLFW/glfw3.h"

export module gse.examples:free_camera;

import std;

import gse.runtime;
import gse.utility;
import gse.physics;
import gse.platform;

export namespace gse {
	auto create_free_camera(const vec3<length>& initial_position) -> uuid;
}

class free_camera final : gse::hook<gse::entity> {
public:
	free_camera(const gse::id& owner_id, const gse::vec3<gse::length>& initial_position) : hook(owner_id), m_initial_position(initial_position) {}

	auto initialize() -> void override {
		gse::renderer::camera().set_position(m_initial_position);
	}

	auto update() -> void override {
		auto& camera = gse::renderer::camera();
		// Move Forward (-Z)
		if (gse::input::get_keyboard().keys[GLFW_KEY_W].held) {
			camera.move(camera.direction_relative_to_origin({ 0.f, 0.f, -1.f }));
		}
		// Move Backward (+Z)
		if (gse::input::get_keyboard().keys[GLFW_KEY_S].held) {
			camera.move(camera.direction_relative_to_origin({ 0.f, 0.f, 1.f }));
		}
		// Strafe Left (-X)
		if (gse::input::get_keyboard().keys[GLFW_KEY_A].held) {
			camera.move(camera.direction_relative_to_origin({ -1.f, 0.f, 0.f }));
		}
		// Strafe Right (+X)
		if (gse::input::get_keyboard().keys[GLFW_KEY_D].held) {
			camera.move(camera.direction_relative_to_origin({ 1.f, 0.f, 0.f }));
		}
		// Move Up (+Y)
		if (gse::input::get_keyboard().keys[GLFW_KEY_SPACE].held) {
			camera.move(camera.direction_relative_to_origin({ 0.f, 1.f, 0.f }));
		}
		// Move Down (-Y)
		if (gse::input::get_keyboard().keys[GLFW_KEY_LEFT_CONTROL].held) {
			camera.move(camera.direction_relative_to_origin({ 0.f, -1.f, 0.f }));
		}
	}
private:
	gse::vec3<gse::length> m_initial_position;
};