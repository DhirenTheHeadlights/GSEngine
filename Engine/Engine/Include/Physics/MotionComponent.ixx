export module gse.physics.motion_component;

import std;

import gse.core.component;
import gse.physics.math;

export namespace gse::physics {
	struct motion_component final : component {
		explicit motion_component(const std::uint32_t id) : component(id) {}

		vec3<length> current_position;
		vec3<velocity> current_velocity;
		vec3<acceleration> current_acceleration;
		vec3<torque> current_torque;

		velocity max_speed = meters_per_second(1.f);
		mass mass = kilograms(1.f);
		length most_recent_y_collision = meters(std::numeric_limits<float>::max());

		quat orientation = quat(1.f, 0.f, 0.f, 0.f);
		vec3<angular_velocity> angular_velocity;
		vec3<angular_acceleration> angular_acceleration;
		inertia moment_of_inertia = 1.f;

		bool affected_by_gravity = true;
		bool moving = false;
		bool airborne = true;
		bool self_controlled = false;

		auto get_speed() const -> velocity;

		auto get_transformation_matrix() const -> mat4;

		auto get_inverse_inertia_tensor_world() const -> mat3;
	};
}

auto gse::physics::motion_component::get_speed() const -> velocity {
	return magnitude(current_velocity);
}

auto gse::physics::motion_component::get_transformation_matrix() const -> mat4 {
	const mat4 translation = translate(mat4(1.0f), current_position);
	const auto rotation = mat4(mat3_cast(orientation));
	const mat4 transformation = translation * rotation; // * scale
	return transformation;
}

auto gse::physics::motion_component::get_inverse_inertia_tensor_world() const -> mat3 {
	const float i_body = moment_of_inertia.as_default_unit();
	const mat3 inv_i_body = gse::identity<float, 3, 3>() * (1.f / i_body);
	const auto rotation = mat3_cast(orientation);
	return rotation * inv_i_body * rotation.transpose();
}
