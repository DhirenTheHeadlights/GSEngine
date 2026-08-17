export module gse.physics:motion_component;

import std;

import :transform_component;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;

export namespace gse::physics {
	struct dynamic_body {
		[[= networked]] mass mass = kilograms(1.f);
		[[= networked]] float restitution = 0.3f;
		[[= networked]] bool affected_by_gravity = true;
		[[= networked]] bool update_orientation = true;
	};

	struct kinematic_body {};

	struct static_body {};

	struct motion_component {
		[[= networked]] vec3<velocity> current_velocity;
		[[= networked]] vec3<angular_velocity> angular_velocity;
		[[= networked]] std::variant<dynamic_body, kinematic_body, static_body> body = dynamic_body{};
		std::uint8_t reset_pending = 0;
	};

	struct impulse_request {
		id target;
		vec3<impulse> impulse;
	};

	auto is_dynamic(
		const motion_component& mc
	) -> bool;

	auto is_kinematic(
		const motion_component& mc
	) -> bool;

	auto is_static(
		const motion_component& mc
	) -> bool;

	auto mass_of(
		const motion_component& mc
	) -> mass;

	auto inv_inertial_tensor(
		const mat3<inverse_inertia>& inv_inertia_body,
		const quat& orientation
	) -> mat3<inverse_inertia>;

	auto is_rotatable(
		const mat3<inverse_inertia>& inv_inertia
	) -> bool;

	auto com_from_origin(
		const vec3<position>& origin,
		const quat& orientation,
		const vec3<displacement>& com_local
	) -> vec3<position>;

	auto origin_from_com(
		const vec3<position>& com,
		const quat& orientation,
		const vec3<displacement>& com_local
	) -> vec3<position>;

	auto interpolated_transform(
		const transform_component& tc,
		const motion_component* mc,
		const vec3<displacement>& com_local,
		time_t<float, seconds> lag
	) -> transform_component;
}

auto gse::physics::is_dynamic(const motion_component& mc) -> bool {
	return std::holds_alternative<dynamic_body>(mc.body);
}

auto gse::physics::is_kinematic(const motion_component& mc) -> bool {
	return std::holds_alternative<kinematic_body>(mc.body);
}

auto gse::physics::is_static(const motion_component& mc) -> bool {
	return std::holds_alternative<static_body>(mc.body);
}

auto gse::physics::mass_of(const motion_component& mc) -> mass {
	if (const auto* d = std::get_if<dynamic_body>(&mc.body)) {
		return d->mass;
	}
	return kilograms(0.f);
}

auto gse::physics::inv_inertial_tensor(const mat3<inverse_inertia>& inv_inertia_body, const quat& orientation) -> mat3<inverse_inertia> {
	const auto rotation = mat3_cast(orientation);
	return rotation * inv_inertia_body * rotation.transpose();
}

auto gse::physics::com_from_origin(const vec3<position>& origin, const quat& orientation, const vec3<displacement>& com_local) -> vec3<position> {
	return origin + rotate_vector(orientation, com_local);
}

auto gse::physics::origin_from_com(const vec3<position>& com, const quat& orientation, const vec3<displacement>& com_local) -> vec3<position> {
	return com - rotate_vector(orientation, com_local);
}

auto gse::physics::interpolated_transform(const transform_component& tc, const motion_component* mc, const vec3<displacement>& com_local, const time_t<float, seconds> lag) -> transform_component {
	if (!mc || lag <= time_t<float, seconds>{}) {
		return tc;
	}

	const vec3<position> com = com_from_origin(tc.position, tc.orientation, com_local) -
		mc->current_velocity * lag;

	quat orientation = tc.orientation;
	if (const auto step = mc->angular_velocity * lag; magnitude(step) > radians(1e-6f)) {
		orientation = normalize(from_axis_angle_vector(-step) * tc.orientation);
	}

	return {
		.position = origin_from_com(com, orientation, com_local),
		.orientation = orientation
	};
}

auto gse::physics::is_rotatable(const mat3<inverse_inertia>& inv_inertia) -> bool {
	for (std::size_t axis = 0; axis < 3; ++axis) {
		if (inv_inertia[axis][axis] <= inverse_inertia{}) {
			return false;
		}
	}
	return true;
}
