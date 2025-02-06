module;

#include "glm/gtc/quaternion.hpp"

export module gse.physics.motion_component;

import std;
import glm;

import gse.core.component;
import gse.physics.math;

export namespace gse::physics {
	struct motion_component final : component {
		motion_component(const std::uint32_t id) : component(id) {}

		vec3<length> current_position;
		vec3<velocity> current_velocity;
		vec3<acceleration> current_acceleration;
		vec3<torque> current_torque;

		velocity max_speed = meters_per_second(1.f);
		mass mass = kilograms(1.f);
		length most_recent_y_collision = meters(std::numeric_limits<float>::max());

		glm::quat orientation = glm::quat(1.f, 0.f, 0.f, 0.f);
		vec3<angular_velocity> angular_velocity;
		vec3<angular_acceleration> angular_acceleration;
		float moment_of_inertia = 1.f;

		bool affected_by_gravity = true;
		bool moving = false;
		bool airborne = true;
		bool self_controlled = false;

		auto get_speed() const -> velocity;

		auto get_transformation_matrix() const -> glm::mat4;
	};
}

auto gse::physics::motion_component::get_speed() const -> velocity {
	return glm::length(to_glm_vec(current_velocity));
}

auto gse::physics::motion_component::get_transformation_matrix() const -> glm::mat4 {
	const glm::mat4 translation = glm::translate(glm::mat4(1.0f), to_glm_vec(current_position));
	const auto rotation = glm::mat4(mat3_cast(orientation));
	const glm::mat4 transformation = translation * rotation; // * scale
	return transformation;
}
