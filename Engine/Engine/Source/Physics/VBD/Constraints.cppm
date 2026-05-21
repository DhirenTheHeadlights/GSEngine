export module gse.physics:vbd_constraints;

import std;

import gse.math;
import gse.gpu;
import :contact_manifold;
import :motion_component;

export namespace gse::vbd {
	struct [[= shaders::shader_constant_block]] vbd_limits {
		std::uint32_t max_bodies = 2048;
		std::uint32_t max_contacts = 16384;
		std::uint32_t max_collision_pairs = 16384;
		std::uint32_t max_colors = 16;
		std::uint32_t max_joints = 128;
		std::uint32_t max_impulses = 64;
		std::uint32_t max_motors = 16;
		std::uint32_t max_contact_adjacency = 16384 * 2;
		std::uint32_t max_joint_adjacency = 128 * 2;
		std::uint32_t max_grounded_uints = (2048 + 31) / 32;
		std::uint32_t grid_table_size = 4096;
		std::uint32_t grid_max_entries = 2048 * 8;
		std::uint32_t workgroup_size = 64;
		std::uint32_t adjacency_workgroup_size = 1024;
		std::uint32_t coloring_rounds = 32;
		std::uint32_t sleep_threshold = 60;
		std::uint32_t collision_state_header_uints = 8;
		std::uint32_t solve_state_float4s_per_body = 11;
		std::uint32_t state_contact_count_index = 0;
		std::uint32_t state_max_used_color_index = 1;
		std::uint32_t state_debug_count_index = 2;
		std::uint32_t state_convergence_max_delta_index = 4;
		std::uint32_t state_converged_flag_index = 5;
		std::uint32_t state_convergence_max_angular_delta_index = 6;
		std::uint32_t narrow_phase_debug_record_uints = 8;
		std::uint32_t max_narrow_phase_debug_records = 32;
		std::uint32_t feature_vertex = 0;
		std::uint32_t feature_edge = 1;
		std::uint32_t feature_face = 2;
		std::uint32_t feature_side_none = 0xFFu;
		std::uint32_t sat_axis_face = 0;
		std::uint32_t sat_axis_cross = 1;
	};

	constexpr vbd_limits limits{};

	struct body_solve_state {
		vec3<force> gradient = {};
		mat3<stiffness> hessian = {};
		vec3<torque> angular_gradient = {};
		mat3<angular_stiffness> angular_hessian = {};
		mat3<linear_angular_stiffness> hessian_xtheta = {};
	};

	enum class [[= shaders::shader_enum]] joint_type : std::uint32_t {
		distance,
		fixed,
		hinge,
		slider,
		muscle,
		ball,
		universal
	};

	struct [[= shaders::shader_struct]] frozen_jacobian {
		vec3<lever_arm> world_r_a;
		vec3<lever_arm> world_r_b;
		mat3<length> j_ang_a;
		mat3<length> j_ang_b;
	};

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

		vec3<angular_stiffness> soft_ang_stiffness = {};

		float activation = 0.f;
		force max_force = newtons(0.f);
	};

	struct [[= shaders::shader_struct]] body_state {
		vec3<position> position;
		vec3<predicted_position> predicted_position;
		vec3<target_position> inertia_target;
		vec3<gse::position> old_position;

		vec3<velocity> velocity;

		quat orientation;
		quat predicted_orientation;
		quat angular_inertia_target;
		quat old_orientation;

		vec3<angular_velocity> angular_velocity;

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

auto gse::vbd::body_state::inverse_mass() const -> gse::inverse_mass {
	if (locked) {
		return gse::inverse_mass{ 0.f };
	}
	return 1.f / mass;
}

auto gse::vbd::body_state::sleeping() const -> bool {
	return sleep_counter >= limits.sleep_threshold;
}
