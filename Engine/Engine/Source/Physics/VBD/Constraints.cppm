export module gse.physics:vbd_constraints;

import std;

import gse.math;
import gse.gpu;
import :contact_manifold;
import :motion_component;

export namespace gse::vbd {
	constexpr std::uint32_t max_contacts = 16384;

	struct body_solve_state {
		vec3<force> gradient = {};
		mat3<stiffness> hessian = {};
		vec3<torque> angular_gradient = {};
		mat3<angular_stiffness> angular_hessian = {};
		mat3<linear_angular_stiffness> hessian_xtheta = {};
	};

	enum class joint_type : std::uint32_t { distance, fixed, hinge, slider };

	struct [[= shaders::shader_struct]] contact_constraint {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;
		std::uint64_t feature_key = 0;
		std::uint32_t sticking = 0;

		vec3f normal;
		vec3f tangent_u;
		vec3f tangent_v;

		vec3<lever_arm> local_anchor_a;
		vec3<lever_arm> local_anchor_b;

		vec3<gap> c0;

		float friction_coeff = 0.6f;
		float restitution = 0.f;

		stiffness penalty_floor = newtons_per_meter(1.f);
		normal_speed approach_speed = {};

		vec3<force> lambda;
		vec3<stiffness> penalty;

		std::uint32_t pad_end = 0;
	};

	struct [[= shaders::shader_struct]] velocity_motor_constraint {
		std::uint32_t body_index = 0;
		std::uint32_t horizontal_only = 0;

		vec3<velocity> target_velocity;

		float compliance = 0.01f;
		force max_force = newtons(1000.f);
	};

	struct [[= shaders::shader_struct]] impulse_constraint {
		std::uint32_t body_index = 0;

		vec3<velocity> delta_velocity;
	};

	struct [[= shaders::shader_struct]] joint_constraint {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;
		joint_type type = joint_type::distance;
		std::uint32_t limits_enabled = 0;

		vec3<lever_arm> local_anchor_a = {};
		vec3<lever_arm> local_anchor_b = {};
		vec3f local_axis_a = { 0.f, 1.f, 0.f };
		vec3f local_axis_b = { 0.f, 1.f, 0.f };

		displacement target_distance = {};
		linear_compliance compliance = {};
		float damping = 0.f;
		angle limit_lower = radians(-std::numbers::pi_v<float>);
		angle limit_upper = radians(std::numbers::pi_v<float>);
		quat rest_orientation = {};

		vec3<force> pos_lambda = {};
		vec3<stiffness> pos_penalty = {};

		vec3<torque> ang_lambda = {};
		vec3<angular_stiffness> ang_penalty;

		torque limit_lambda = {};
		angular_stiffness limit_penalty = {};

		vec3<gap> pos_c0 = {};
		vec3<angle> ang_c0 = {};
		angle limit_c0 = {};
	};

	struct [[= shaders::shader_struct]] body_state {
		vec3<position> position;
		vec3<predicted_position> predicted_position;
		vec3<target_position> inertia_target;
		vec3<gse::position> old_position;

		vec3<velocity> velocity;
		vec3<gse::velocity> predicted_velocity;

		quat orientation;
		quat predicted_orientation;
		quat angular_inertia_target;
		quat old_orientation;

		vec3<angular_velocity> angular_velocity;
		vec3<gse::angular_velocity> predicted_angular_velocity;

		vec3<target_position> motor_target;

		mass mass = kilograms(1.f);
		std::uint32_t locked = 0;
		std::uint32_t update_orientation = 1;
		std::uint32_t affected_by_gravity = 1;
		std::uint32_t sleep_counter = 0;

		float accel_weight = 0.f;
		float restitution = 0.f;
		mat3<inverse_inertia> inv_inertia;

		vec3<displacement> half_extents;
		vec3<gse::position> aabb_min;
		vec3<gse::position> aabb_max;

		auto inverse_mass() const -> inverse_mass;
		auto sleeping() const -> bool;
	};
}

export template <>
struct gse::shaders::slang_type<gse::vbd::joint_type> {
	static constexpr std::string_view name = "uint";
};

auto gse::vbd::body_state::inverse_mass() const -> gse::inverse_mass {
	if (locked) {
		return gse::inverse_mass{ 0.f };
	}
	return 1.f / mass;
}

auto gse::vbd::body_state::sleeping() const -> bool {
	return sleep_counter >= 60;
}
