export module gse.shader:vbd_physics;

import std;

import gse.containers;
import gse.gpu;
import gse.math;

export namespace gse::shaders::vbd_physics {
	struct [[= shader_struct]] gpu_body {
		vec4f position;
		vec4f predicted_position;
		vec4f inertia_target;
		vec4f old_position;
		vec4f velocity;
		vec4f predicted_velocity;
		vec4f orientation;
		vec4f predicted_orientation;
		vec4f angular_inertia_target;
		vec4f old_orientation;
		vec4f angular_velocity;
		vec4f predicted_angular_velocity;
		vec4f motor_target;
		float mass;
		std::uint32_t flags;
		std::uint32_t sleep_counter;
		float accel_weight;
		mat3f inertia;
		vec4f half_extents;
		vec4f aabb_min;
		vec4f aabb_max;
	};

	struct [[= shader_struct]] gpu_contact {
		std::uint32_t body_a;
		std::uint32_t body_b;
		std::uint32_t feature_key_hi;
		std::uint32_t feature_key_lo;
		std::uint32_t sticking;
		vec4f normal;
		vec4f tangent_u;
		vec4f tangent_v;
		vec4f r_a;
		vec4f r_b;
		vec4f c0_friction;
		vec4f lambda;
		vec4f penalty;
	};

	struct [[= shader_struct]] gpu_motor {
		std::uint32_t body_index;
		float compliance;
		float max_force;
		std::uint32_t horizontal_only;
		vec4f target_velocity;
	};

	struct [[= shader_struct]] gpu_warm_start {
		std::uint32_t body_a;
		std::uint32_t body_b;
		std::uint32_t feature_key_hi;
		std::uint32_t feature_key_lo;
		std::uint32_t sticking;
		vec4f normal;
		vec4f tangent_u;
		vec4f tangent_v;
		vec4f local_anchor_a;
		vec4f local_anchor_b;
		vec4f lambda;
		vec4f penalty;
	};

	struct [[= shader_struct]] gpu_joint {
		std::uint32_t body_a;
		std::uint32_t body_b;
		std::uint32_t type;
		std::uint32_t limits_enabled;
		vec4f local_anchor_a;
		vec4f local_anchor_b;
		vec4f local_axis_a;
		vec4f local_axis_b;
		float target_distance;
		float compliance;
		float damping;
		float pad0;
		float limit_lower;
		float limit_upper;
		float pad1;
		float pad2;
		vec4f rest_orientation;
		vec4f pos_lambda;
		vec4f pos_penalty;
		vec4f ang_lambda;
		vec4f ang_penalty;
		float limit_lambda;
		float limit_penalty;
		float pad3;
		float pad4;
		vec4f pos_c0;
		vec4f ang_c0;
		float limit_c0;
		float pad5;
		float pad6;
		float pad7;
	};

	struct [[= shader_struct]] frozen_jacobian {
		vec4f world_r_a;
		vec4f world_r_b;
		mat3f j_ang_a;
		mat3f j_ang_b;
	};

	struct [[= shader_struct]] dispatch_args {
		std::uint32_t x;
		std::uint32_t y;
		std::uint32_t z;
	};

	struct [[= shader_struct]] vbd_push_constants {
		std::uint32_t body_count;
		std::uint32_t contact_count;
		std::uint32_t motor_count;
		std::uint32_t color_offset;
		std::uint32_t color_count;
		std::uint32_t warm_start_count;
		std::uint32_t post_stabilize;
		std::uint32_t joint_count;
		float h_squared;
		float dt;
		float beta;
		float ang_beta;
		float linear_damping;
		float velocity_sleep_threshold;
		float angular_sleep_threshold;
		float current_alpha;
		float collision_margin;
		float friction_coefficient;
		float penalty_min;
		float penalty_max;
		float gamma;
		float solver_alpha;
		float speculative_margin;
		float stick_threshold;
		std::uint32_t substep;
		std::uint32_t iteration;
		float convergence_threshold;
		std::uint32_t min_iterations;
		float grid_cell_size;
		std::uint32_t use_jacobi;
		float jacobi_omega;
	};

	struct [[= binding<0, 0>{}, = ssbo_readwrite]] body_data {
		using element = gpu_body;
	};

	struct [[= binding<0, 1>{}, = ssbo_readwrite]] contact_data {
		using element = gpu_contact;
	};

	struct [[= binding<0, 2>{}, = ssbo_readonly]] motor_data {
		using element = gpu_motor;
	};

	struct [[= binding<0, 3>{}, = ssbo_readwrite]] color_data {
		using element = std::uint32_t;
	};

	struct [[= binding<0, 4>{}, = ssbo_readwrite]] contact_offsets {
		using element = std::uint32_t;
	};

	struct [[= binding<0, 5>{}, = ssbo_readwrite]] solve_state {
		using element = vec4f;
	};

	struct [[= binding<0, 6>{}, = ssbo_readwrite]] collision_pairs {
		using element = std::uint32_t;
	};

	struct [[= binding<0, 7>{}, = ssbo_readwrite]] collision_state {
		using element = std::uint32_t;
	};

	struct [[= binding<0, 8>{}, = ssbo_readonly]] warm_starts {
		using element = gpu_warm_start;
	};

	struct [[= binding<0, 9>{}, = ssbo_readwrite]] joint_data {
		using element = gpu_joint;
	};

	struct [[= binding<0, 10>{}, = ssbo_readwrite]] contact_counts {
		using element = std::uint32_t;
	};

	struct [[= binding<0, 11>{}, = ssbo_readwrite]] contact_adjacency {
		using element = std::uint32_t;
	};

	struct [[= binding<0, 12>{}, = ssbo_readwrite]] motor_map {
		using element = std::uint32_t;
	};

	struct [[= binding<0, 13>{}, = ssbo_readwrite]] joint_offsets {
		using element = std::uint32_t;
	};

	struct [[= binding<0, 14>{}, = ssbo_readwrite]] joint_counts {
		using element = std::uint32_t;
	};

	struct [[= binding<0, 15>{}, = ssbo_readwrite]] joint_adjacency {
		using element = std::uint32_t;
	};

	struct [[= binding<0, 16>{}, = ssbo_readwrite]] grid_data {
		using element = std::uint32_t;
	};

	struct [[= binding<0, 17>{}, = ssbo_readwrite]] indirect_args {
		using element = dispatch_args;
	};

	struct [[= binding<0, 18>{}, = ssbo_readwrite]] frozen_jacobians {
		using element = frozen_jacobian;
	};

	struct [[= binding<0, 19>{}, = ssbo_readwrite]] solve_deltas {
		using element = vec4f;
	};

	using shader_types = type_pack<
		gpu_body,
		gpu_contact,
		gpu_motor,
		gpu_warm_start,
		gpu_joint,
		frozen_jacobian,
		dispatch_args,
		vbd_push_constants
	>;

	using shader_binding_types = type_pack<
		body_data,
		contact_data,
		motor_data,
		color_data,
		contact_offsets,
		solve_state,
		collision_pairs,
		collision_state,
		warm_starts,
		joint_data,
		contact_counts,
		contact_adjacency,
		motor_map,
		joint_offsets,
		joint_counts,
		joint_adjacency,
		grid_data,
		indirect_args,
		frozen_jacobians,
		solve_deltas
	>;
}

static_assert(sizeof(gse::shaders::vbd_physics::gpu_body) == gse::shaders::slang_scalar_size<gse::shaders::vbd_physics::gpu_body>());
static_assert(sizeof(gse::shaders::vbd_physics::gpu_contact) == gse::shaders::slang_scalar_size<gse::shaders::vbd_physics::gpu_contact>());
static_assert(sizeof(gse::shaders::vbd_physics::gpu_motor) == gse::shaders::slang_scalar_size<gse::shaders::vbd_physics::gpu_motor>());
static_assert(sizeof(gse::shaders::vbd_physics::gpu_warm_start) == gse::shaders::slang_scalar_size<gse::shaders::vbd_physics::gpu_warm_start>());
static_assert(sizeof(gse::shaders::vbd_physics::gpu_joint) == gse::shaders::slang_scalar_size<gse::shaders::vbd_physics::gpu_joint>());
static_assert(sizeof(gse::shaders::vbd_physics::frozen_jacobian) == gse::shaders::slang_scalar_size<gse::shaders::vbd_physics::frozen_jacobian>());
static_assert(sizeof(gse::shaders::vbd_physics::dispatch_args) == gse::shaders::slang_scalar_size<gse::shaders::vbd_physics::dispatch_args>());
static_assert(sizeof(gse::shaders::vbd_physics::vbd_push_constants) == gse::shaders::slang_scalar_size<gse::shaders::vbd_physics::vbd_push_constants>());
