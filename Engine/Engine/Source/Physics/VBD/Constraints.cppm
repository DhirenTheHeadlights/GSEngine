export module gse.physics:vbd_constraints;

import std;

import gse.math;
import :contact_manifold;
import :motion_component;

export namespace gse::vbd {
	struct body_solve_state {
		unitless::vec3 gradient = {};
		mat3 hessian = {};
		unitless::vec3 angular_gradient = {};
		mat3 angular_hessian = {};
		mat3 hessian_xtheta = {};
	};

	struct contact_constraint {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;

		unitless::vec3 normal;
		unitless::vec3 tangent_u;
		unitless::vec3 tangent_v;

		vec3<length> r_a;
		vec3<length> r_b;

		length initial_separation;

		float compliance = 0.f;
		float damping = 0.f;

		float lambda = 0.f;
		float lambda_tangent_u = 0.f;
		float lambda_tangent_v = 0.f;

		float friction_coeff = 0.6f;

		feature_id feature;
	};

	struct velocity_motor_constraint {
		std::uint32_t body_index = 0;

		vec3<velocity> target_velocity;

		float compliance = 0.01f;
		force max_force = newtons(1000.f);
		bool horizontal_only = false;

		std::array<float, 3> lambda = { 0.f, 0.f, 0.f };
	};

	struct body_state {
		vec3<length> position;
		vec3<length> predicted_position;
		vec3<length> inertia_target;
		vec3<length> old_position;

		vec3<velocity> body_velocity;
		vec3<velocity> predicted_velocity;

		quat orientation;
		quat predicted_orientation;
		quat angular_inertia_target;
		quat old_orientation;

		vec3<angular_velocity> body_angular_velocity;
		vec3<angular_velocity> predicted_angular_velocity;

		vec3<length> motor_target;

		mass mass_value = kilograms(1.f);
		physics::inv_inertia_mat inv_inertia;

		bool locked = false;
		bool update_orientation = true;
		bool affected_by_gravity = true;

		std::uint32_t sleep_counter = 0;

		auto inverse_mass() const -> inverse_mass;
		auto sleeping() const -> bool;
	};
}

auto gse::vbd::body_state::inverse_mass() const -> gse::inverse_mass {
	if (locked) return gse::inverse_mass{ 0.f };
	return 1.f / mass_value;
}

auto gse::vbd::body_state::sleeping() const -> bool {
	return sleep_counter >= 300;
}