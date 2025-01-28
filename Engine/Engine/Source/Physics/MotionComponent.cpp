module;

#include "glm/gtc/quaternion.hpp"

module gse.physics.motion_component;

import glm;

import gse.physics.math.vector;
import gse.physics.math.vector_math;

auto gse::physics::motion_component::get_speed() const -> velocity {
	return magnitude(current_velocity);
}

auto gse::physics::motion_component::get_transformation_matrix() const -> glm::mat4 {
	const glm::mat4 translation = translate(glm::mat4(1.0f), current_position.as_default_units());
	const auto rotation = glm::mat4(mat3_cast(orientation));
	const glm::mat4 transformation = translation * rotation; // * scale
	return transformation;
}