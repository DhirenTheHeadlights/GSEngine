export module gse.physics:vbd_gpu_solver;

import std;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.log;

import :vbd_constraints;
import :vbd_solver;

export namespace gse::vbd {
	struct [[= shaders::shader_struct]] gpu_body {
		vec4<position> position;
		vec4<predicted_position> predicted_position;
		vec4<target_position> inertia_target;
		vec4<gse::position> old_position;
		vec4<gse::velocity> velocity;
		vec4<gse::velocity> predicted_velocity;
		vec4f orientation;
		vec4f predicted_orientation;
		vec4f angular_inertia_target;
		vec4f old_orientation;
		vec4<angular_velocity> angular_velocity;
		vec4<gse::angular_velocity> predicted_angular_velocity;
		vec4<gse::velocity> motor_target;
		mass mass;
		std::uint32_t flags;
		std::uint32_t sleep_counter;
		float accel_weight;
		mat3<inertia> inertia;
		vec4<displacement> half_extents;
		vec4<gse::position> aabb_min;
		vec4<gse::position> aabb_max;
	};

	struct [[= shaders::shader_struct]] gpu_contact {
		std::uint32_t body_a;
		std::uint32_t body_b;
		std::uint64_t feature_key;
		std::uint32_t sticking;
		vec4f normal;
		vec4f tangent_u;
		vec4f tangent_v;
		vec4<lever_arm> local_anchor_a;
		vec4<lever_arm> local_anchor_b;
		vec4<gap> c0;
		float friction_coeff;
		vec4<force> lambda;
		vec4<stiffness> penalty;
	};

	struct [[= shaders::shader_struct]] gpu_motor {
		std::uint32_t body_index;
		float compliance;
		force max_force;
		std::uint32_t horizontal_only;
		vec4<velocity> target_velocity;
	};

	struct [[= shaders::shader_struct]] gpu_warm_start {
		std::uint32_t body_a;
		std::uint32_t body_b;
		std::uint64_t feature_key;
		std::uint32_t sticking;
		vec4f normal;
		vec4f tangent_u;
		vec4f tangent_v;
		vec4<lever_arm> local_anchor_a;
		vec4<lever_arm> local_anchor_b;
		vec4<force> lambda;
		vec4<stiffness> penalty;
		std::uint32_t pad_end;
	};

	struct [[= shaders::shader_struct]] gpu_joint {
		std::uint32_t body_a;
		std::uint32_t body_b;
		std::uint32_t type;
		std::uint32_t limits_enabled;
		vec4<lever_arm> local_anchor_a;
		vec4<lever_arm> local_anchor_b;
		vec4f local_axis_a;
		vec4f local_axis_b;
		displacement target_distance;
		linear_compliance compliance;
		float damping;
		float pad0;
		angle limit_lower;
		angle limit_upper;
		float pad1;
		float pad2;
		vec4f rest_orientation;
		vec4<force> pos_lambda;
		vec4<stiffness> pos_penalty;
		vec4<torque> ang_lambda;
		vec4<angular_stiffness> ang_penalty;
		torque limit_lambda;
		angular_stiffness limit_penalty;
		float pad3;
		float pad4;
		vec4<gap> pos_c0;
		vec4<angle> ang_c0;
		angle limit_c0;
		float pad5;
		float pad6;
		float pad7;
	};

	struct [[= shaders::shader_struct]] frozen_jacobian {
		vec4<lever_arm> world_r_a;
		vec4<lever_arm> world_r_b;
		mat3f j_ang_a;
		mat3f j_ang_b;
	};

	struct [[= shaders::shader_struct]] dispatch_args {
		std::uint32_t x;
		std::uint32_t y;
		std::uint32_t z;
	};

	using shader_types = type_pack<
		gpu_body,
		gpu_contact,
		gpu_motor,
		gpu_warm_start,
		gpu_joint,
		frozen_jacobian,
		dispatch_args
	>;
}

export namespace gse::vbd {
	constexpr std::uint32_t max_bodies = 2048;
	constexpr std::uint32_t max_contacts = 16384;
	constexpr std::uint32_t max_motors = 16;
	constexpr std::uint32_t max_joints = 128;
	constexpr std::uint32_t max_colors = 16;
	constexpr std::uint32_t max_collision_pairs = 16384;
	constexpr std::uint32_t workgroup_size = 64;
	constexpr std::uint32_t collision_state_header_uints = 8;

	constexpr std::uint32_t solve_state_float4s_per_body = 11;
	constexpr std::uint32_t collision_state_uints = collision_state_header_uints;

	namespace timing_slot {
		constexpr std::uint32_t begin = 0;
		constexpr std::uint32_t after_collision = 2;
		constexpr std::uint32_t after_predict = 3;
		constexpr std::uint32_t after_solve = 4;
		constexpr std::uint32_t after_velocity = 5;
		constexpr std::uint32_t after_finalize = 6;
		constexpr std::uint32_t end = 1;
	}
	constexpr std::uint32_t grid_table_size = 4096;

	constexpr std::uint32_t flag_locked = 1u;
	constexpr std::uint32_t flag_update_orientation = 2u;
	constexpr std::uint32_t flag_affected_by_gravity = 4u;

	struct collision_body_data {
		vec3<displacement> half_extents;
		vec3<position> aabb_min;
		vec3<position> aabb_max;
	};

	class gpu_solver {
	public:
		~gpu_solver();

		auto create_buffers(
			const gpu::context::state& ctx
		) -> void;

		auto initialize_compute(
			run_context& ctx,
			const gpu::context::state& gpu_s
		) -> async::task<>;

		auto dispatch_compute(
		) -> void;

		auto compute_initialized(
		) const -> bool;

		auto buffers_created(
		) const -> bool;

		auto upload(
			std::span<const body_state> bodies,
			std::span<const collision_body_data> collision_data,
			std::span<const float> accel_weights,
			std::span<const velocity_motor_constraint> motors,
			std::span<const joint_constraint> joints,
			std::span<const gpu_warm_start> prev_contacts,
			std::span<const std::uint32_t> authoritative_body_indices,
			const solver_config& solver_cfg,
			time_step dt,
			int steps
		) -> void;

		auto total_substeps(
		) const -> std::uint32_t;

		auto commit_upload(
		) -> void;

		auto stage_readback(
		) -> void;

		auto readback(
			std::span<body_state> bodies,
			std::vector<gpu_contact>& contacts_out,
			std::span<joint_constraint> joints_out
		) -> void;

		auto has_readback_data(
		) const -> bool;

		auto pending_dispatch(
		) const -> bool;

		auto ready_to_dispatch(
		) const -> bool;

		auto mark_dispatched(
		) -> void;

		auto body_count(
		) const -> std::uint32_t;

		auto contact_count(
		) const -> std::uint32_t;

		auto motor_count(
		) const -> std::uint32_t;

		auto joint_count(
		) const -> std::uint32_t;

		auto solver_cfg(
		) const -> const solver_config&;

		auto dt(
		) const -> time_step;

		auto frame_count(
		) const -> std::uint32_t;

		auto solve_time(
		) const -> time_step;

		auto snapshot_buffer(
			std::uint32_t slot
		) const -> const gpu::buffer&;

		auto latest_snapshot_slot(
		) const -> std::uint32_t;

		auto compute_semaphore(
		) const -> gpu::compute_semaphore_state;

	private:
		struct compute_shaders {
			gpu::pipeline predict_pipeline;
			gpu::pipeline solve_color_pipeline;
			gpu::pipeline update_lambda_pipeline;
			gpu::pipeline derive_velocities_pipeline;
			gpu::pipeline finalize_pipeline;
			gpu::pipeline collision_reset_pipeline;
			gpu::pipeline collision_grid_build_pipeline;
			gpu::pipeline collision_broad_phase_pipeline;
			gpu::pipeline collision_narrow_phase_pipeline;
			gpu::pipeline collision_build_adjacency_pipeline;
			gpu::pipeline update_joint_lambda_pipeline;
			gpu::pipeline prepare_indirect_pipeline;
			gpu::pipeline prepare_contact_indirect_pipeline;
			gpu::pipeline prepare_color_indirect_pipeline;
			gpu::pipeline freeze_jacobians_pipeline;
			gpu::pipeline apply_jacobi_pipeline;

			float solve_ms = 0.f;
			bool initialized = false;
		} m_compute;

		struct per_frame_data {
			gpu::compute_queue queue;
			gpu::descriptor_region descriptors;

			gpu::buffer body_buffer;
			gpu::buffer contact_buffer;
			gpu::buffer motor_buffer;
			gpu::buffer color_buffer;
			gpu::buffer contact_offsets_buffer;
			gpu::buffer contact_counts_buffer;
			gpu::buffer contact_adjacency_buffer;
			gpu::buffer motor_map_buffer;
			gpu::buffer joint_offsets_buffer;
			gpu::buffer joint_counts_buffer;
			gpu::buffer joint_adjacency_buffer;
			gpu::buffer solve_state_buffer;
			gpu::buffer readback_buffer;
			gpu::buffer collision_pair_buffer;
			gpu::buffer collision_state_buffer;
			gpu::buffer warm_start_buffer;
			gpu::buffer joint_buffer;
			gpu::buffer grid_buffer;
			gpu::buffer physics_snapshot_buffer;
			gpu::buffer indirect_dispatch_buffer;
			gpu::buffer frozen_jacobian_buffer;
			gpu::buffer solve_deltas_buffer;

			struct readback_frame_info {
				std::uint32_t body_count = 0;
				std::uint32_t contact_count = 0;
				std::uint32_t joint_count = 0;
			} readback_info;

			bool readback_pending = false;
			bool first_submit = true;
		};

		per_frame_resource<per_frame_data> m_frames{ per_frame_data{}, per_frame_data{} };
		std::uint32_t m_dispatch_slot = 0;

		bool m_buffers_created = false;
		bool m_pending_dispatch = false;
		std::uint32_t m_frame_count = 0;

		std::uint32_t m_body_count = 0;
		std::uint32_t m_contact_count = 0;
		std::uint32_t m_motor_count = 0;
		std::uint32_t m_joint_count = 0;

		std::uint32_t m_steps = 1;
		gap m_grid_cell_size = meters(2.0f);
		solver_config m_solver_cfg;
		time_step m_dt{};

		std::uint32_t m_warm_start_count = 0;

		std::vector<gpu_motor> m_upload_motors;
		std::vector<gpu_warm_start> m_upload_warm_starts;
		std::vector<gpu_joint> m_upload_joints;
		std::vector<std::uint32_t> m_upload_motor_map;
		std::vector<std::uint32_t> m_upload_collision_state;
		std::vector<std::uint8_t> m_upload_authoritative_bodies;

		std::vector<gpu_contact> m_staged_contacts;
		std::vector<gpu_joint> m_staged_joints;
		std::vector<gpu_body> m_staged_bodies;
		std::uint32_t m_staged_body_count = 0;
		std::uint32_t m_staged_contact_count = 0;
		std::uint32_t m_staged_joint_count = 0;
		bool m_staged_valid = false;
	};
}
