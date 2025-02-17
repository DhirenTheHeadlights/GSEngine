export module gse.physics.motion_component;

import std;

import gse.graphics.render_component;
import gse.core.component;
import gse.physics.math;

export namespace gse::physics {
	struct motion_component final : component {
		motion_component(std::uint32_t id, const vec3<length>& size = {});

		vec3<length> current_position;
		vec3<velocity> current_velocity;
		vec3<acceleration> current_acceleration;
		vec3<torque> current_torque;

		velocity max_speed;
		mass mass = kilograms(1.0f);
		length most_recent_y_collision = meters(std::numeric_limits<float>::max());

		quat orientation;
		mat3 inertial_tensor;
		vec3<angular_velocity> angular_velocity;
		vec3<angular_acceleration> angular_acceleration;

		bool affected_by_gravity = true;
		bool moving = false;
		bool airborne = true;
		bool self_controlled = false;

		auto get_speed() const -> velocity;

		auto get_transformation_matrix() const -> mat4;

		auto required_components() -> std::vector<std::type_index> override;
	};

	auto create_inertial_tensor(const vec3<length>& size, const mass& mass) -> mat3;
}

gse::physics::motion_component::motion_component(const std::uint32_t id, const vec3<length>& size) : component(id), inertial_tensor(create_inertial_tensor(size, mass)) {}

auto gse::physics::motion_component::get_speed() const -> velocity {
	return magnitude(current_velocity);
}

auto gse::physics::motion_component::get_transformation_matrix() const -> mat4 {
	const mat4 translation = translate(mat4(1.0f), current_position);
	const auto rotation = mat4(mat3_cast(orientation));
	const mat4 transformation = translation * rotation; // * scale
	return transformation;
}

auto gse::physics::motion_component::required_components() -> std::vector<std::type_index> {
	return { typeid(render_component) };
}

auto gse::physics::create_inertial_tensor(const vec3<length>& size, const mass& mass) -> mat3 {
	const auto size_m = size.as<units::meters>();
	const auto mass_kg = mass.as<units::kilograms>() * 1 / 12; // 1/12 is the moment of inertia for a cube

	const auto x_sq = size_m.x * size_m.x;
	const auto y_sq = size_m.y * size_m.y;
	const auto z_sq = size_m.z * size_m.z;

	return {
		{ mass_kg * (y_sq + z_sq), 0, 0 },
		{ 0, mass_kg * (x_sq + z_sq), 0 },
		{ 0, 0, mass_kg * (x_sq + y_sq) }
	};
}
