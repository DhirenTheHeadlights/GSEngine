export module gse.physics:vbd_constraints;

import std;

import gse.math;
import :contact_manifold;
import :motion_component;

export namespace gse::vbd {
	struct body_solve_state {
		vec3<force> gradient = {};
		mat3<stiffness> hessian = {};
		vec3<torque> angular_gradient = {};
		mat3<angular_stiffness> angular_hessian = {};
		mat3<linear_angular_stiffness> hessian_xtheta = {};
	};

	struct contact_constraint {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;

		vec3f normal;
		vec3f tangent_u;
		vec3f tangent_v;

		vec3<lever_arm> r_a;
		vec3<lever_arm> r_b;

		vec3<gap> c0;

		vec3<force> lambda;
		vec3<stiffness> penalty;

		stiffness penalty_floor = newtons_per_meter(1.f);
		float friction_coeff = 0.6f;
		float restitution = 0.f;
		bool sticking = false;

		normal_speed approach_speed = {};

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
		joint_constraint(
		) = default;

		~joint_constraint(
		) = default;

		joint_constraint(
			joint_constraint&&
		) noexcept = default;

		auto operator=(
			joint_constraint&&
		) noexcept -> joint_constraint& = default;

		joint_constraint(
			const joint_constraint&
		) = default;

		auto operator=(
			const joint_constraint&
		) -> joint_constraint& = default;

		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;
		joint_type type = joint_type::distance;

		vec3<lever_arm> local_anchor_a;
		vec3<lever_arm> local_anchor_b;
		vec3f local_axis_a = { 0.f, 1.f, 0.f };
		vec3f local_axis_b = { 0.f, 1.f, 0.f };

		displacement target_distance = {};
		linear_compliance compliance = {};
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

		vec3<gap> pos_c0;
		vec3<angle> ang_c0;
		angle limit_c0 = {};
	};

	struct body_state {
		vec3<position> position;
		vec3<predicted_position> predicted_position;
		vec3<target_position> inertia_target;
		vec3<gse::position> initial_position;

		vec3<velocity> body_velocity;

		quat orientation;
		quat predicted_orientation;
		quat angular_inertia_target;
		quat initial_orientation;

		vec3<angular_velocity> body_angular_velocity;

		vec3<target_position> motor_target;

		mass mass_value = kilograms(1.f);
		mat3<inverse_inertia> inv_inertia;

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
