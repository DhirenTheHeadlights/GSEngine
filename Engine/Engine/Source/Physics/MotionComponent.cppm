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
		[[= networked]] inertia moment_of_inertia = kilograms_meters_squared(163.f);
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
		const motion_component& mc,
		const quat& orientation
	) -> mat3<inverse_inertia>;

	auto interpolated_transform(
		const transform_component& tc,
		const motion_component* mc,
		time_t<float, seconds> lag
	) -> transform_component {
		if (!mc || lag <= time_t<float, seconds>{}) {
			return tc;
		}

		transform_component result{
			.position = tc.position - mc->current_velocity * lag,
			.orientation = tc.orientation
		};

		if (const auto step = mc->angular_velocity * lag; magnitude(step) > radians(1e-6f)) {
			result.orientation = normalize(from_axis_angle_vector(-step) * tc.orientation);
		}

		return result;
	}
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

auto gse::physics::inv_inertial_tensor(const motion_component& mc, const quat& orientation) -> mat3<inverse_inertia> {
	const auto* d = std::get_if<dynamic_body>(&mc.body);
	if (!d) {
		return gse::identity<float, 3, 3>() * inverse_inertia{};
	}
	const inverse_inertia inv_i = 1.f / d->moment_of_inertia;
	const auto inv_i_body = gse::identity<float, 3, 3>() * inv_i;
	const auto rotation = mat3_cast(orientation);
	return rotation * inv_i_body * rotation.transpose();
}
