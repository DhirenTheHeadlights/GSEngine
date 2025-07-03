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

struct free_camera_hook final : gse::hook<gse::entity> {
	free_camera_hook(const gse::vec3<gse::length>& initial_position)
		: m_initial_position(initial_position) {}

	auto initialize() -> void override {
		gse::get_camera().set_position(m_initial_position);
	}

	auto update() -> void override {
		auto& camera = gse::get_camera();
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

	gse::vec3<gse::length> m_initial_position;
};

auto gse::create_free_camera(const vec3<length>& initial_position) -> uuid {
	const auto camera_id = registry::create_entity();
	registry::add_entity_hook(camera_id, std::make_unique<free_camera_hook>(initial_position));
	return camera_id;
}