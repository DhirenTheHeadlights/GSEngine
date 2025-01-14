#pragma once

#include <glm/glm.hpp>

#include "Core/EngineComponent.h"
#include "glm/detail/type_quat.hpp"
#include "glm/gtc/quaternion.hpp"
#include "Physics/Units/Units.h"
#include "Physics/Vector/Vec3.h"
#include "Vector/Math.h"

namespace gse::physics {
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
		vec3<units::radians_per_second> angular_velocity;
		vec3<units::radians_per_second_squared> angular_acceleration;
		unitless moment_of_inertia = 1.f;

		bool affected_by_gravity = true;
		bool moving = false;
		bool airborne = true;
		bool self_controlled = false;

		auto get_speed() const -> velocity {
			return magnitude(current_velocity);
		}

		auto get_transformation_matrix() const -> glm::mat4 {
			const glm::mat4 translation = translate(glm::mat4(1.0f), current_position.as_default_units());
			const auto rotation = glm::mat4(mat3_cast(orientation));
			const glm::mat4 transformation = translation * rotation; // * scale
			return transformation;
		}
	};
}
