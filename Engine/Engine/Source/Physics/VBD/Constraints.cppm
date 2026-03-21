export module gse.physics:vbd_constraints;

import std;

import gse.math;
import :contact_manifold;
import :motion_component;

export namespace gse::vbd {
	using stiffness_mat3 = mat<stiffness, 3, 3>;
	using ang_stiffness_mat3 = mat<angular_stiffness, 3, 3>;
	using xtheta_mat3 = mat<linear_angular_stiffness, 3, 3>;

	struct body_solve_state {
		vec3<force> gradient = {};
		stiffness_mat3 hessian = {};
		vec3<torque> angular_gradient = {};
		ang_stiffness_mat3 angular_hessian = {};
		xtheta_mat3 hessian_xtheta = {};
	};

	struct contact_constraint {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;

		unitless::vec3 normal;
		unitless::vec3 tangent_u;
		unitless::vec3 tangent_v;

		vec3<length> r_a;
		vec3<length> r_b;

		vec3<length> c0;

		vec3<force> lambda;
		vec3<stiffness> penalty;

		stiffness penalty_floor = newtons_per_meter(1.f);
		float friction_coeff = 0.6f;
		float restitution = 0.f;
		bool sticking = false;

		velocity approach_speed = {};

		feature_id feature;
	};

	struct velocity_motor_constraint {
		std::uint32_t body_index = 0;

		vec3<velocity> target_velocity;

		float compliance = 0.01f;
		force max_force = newtons(1000.f);
		bool horizontal_only = false;
	};

	enum class joint_type : std::uint8_t { distance, fixed, hinge, slider };

	struct joint_constraint {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;
		joint_type type = joint_type::distance;

		vec3<length> local_anchor_a;
		vec3<length> local_anchor_b;
		unitless::vec3 local_axis_a = { 0.f, 1.f, 0.f };
		unitless::vec3 local_axis_b = { 0.f, 1.f, 0.f };

		length target_distance = {};
		inverse_mass compliance = {};
		float damping = 0.f;
		angle limit_lower = radians(-std::numbers::pi_v<float>);
		angle limit_upper = radians(std::numbers::pi_v<float>);
		bool limits_enabled = false;
		quat rest_orientation;

		vec3<force> pos_lambda;
		vec3<stiffness> pos_penalty;

		vec3<torque> ang_lambda;
		vec3<angular_stiffness> ang_penalty;

		torque limit_lambda = {};
		angular_stiffness limit_penalty = {};

		vec3<length> pos_c0;
		vec3<angle> ang_c0;
		angle limit_c0 = {};
	};

	struct body_state {
		vec3<length> position;
		vec3<length> predicted_position;
		vec3<length> inertia_target;
		vec3<length> initial_position;

		vec3<velocity> body_velocity;

		quat orientation;
		quat predicted_orientation;
		quat angular_inertia_target;
		quat initial_orientation;

		vec3<angular_velocity> body_angular_velocity;

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
	return sleep_counter >= 60;
}
