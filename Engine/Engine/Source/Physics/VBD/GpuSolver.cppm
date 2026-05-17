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

import :vbd_constraints;
import :vbd_solver;

export namespace gse::vbd {
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
		body_state,
		contact_constraint,
		velocity_motor_constraint,
		joint_constraint,
		frozen_jacobian,
		dispatch_args
	>;
}

export namespace gse::vbd {
	constexpr std::uint32_t max_bodies = 2048;
	constexpr std::uint32_t max_motors = 16;
	constexpr std::uint32_t max_joints = 128;
	constexpr std::uint32_t max_colors = 16;
	constexpr std::uint32_t max_collision_pairs = 16384;
	constexpr std::uint32_t workgroup_size = 64;
	constexpr std::uint32_t collision_state_header_uints = 8;

	constexpr std::uint32_t solve_state_float4s_per_body = 11;
	constexpr std::uint32_t collision_state_uints = collision_state_header_uints;
	constexpr std::uint32_t grid_table_size = 4096;

	struct vbd_solve_chain {};

	struct vbd_clear_state_buffers_stage {};
	struct vbd_collision_reset_stage {};
	struct vbd_grid_build_stage {};
	struct vbd_broad_phase_stage {};
	struct vbd_prepare_indirect_stage {};
	struct vbd_narrow_phase_stage {};
	struct vbd_prepare_contact_indirect_stage {};
	struct vbd_build_adjacency_stage {};
	struct vbd_predict_stage {};
	struct vbd_freeze_jacobians_stage {};
	struct vbd_solve_iterations_stage {};
	struct vbd_derive_velocities_stage {};
	struct vbd_apply_restitution_stage {};
	struct vbd_post_stabilize_stage {};
	struct vbd_finalize_stage {};
	struct vbd_readback_copy_stage {};

	class gpu_solver {
	public:
		auto create_buffers(
			const gpu::context::data& ctx
		) -> void;

		auto initialize_compute(
			run_context& ctx,
			const gpu::context::data& gpu_s
		) -> async::task<>;

		auto dispatch_compute(
			frame_context& ctx
		) -> async::task<>;

		auto compute_initialized(
		) const -> bool;

		auto buffers_created(
		) const -> bool;

		auto upload(
			std::span<const body_state> bodies,
			std::span<const velocity_motor_constraint> motors,
			std::span<const joint_constraint> joints,
			std::span<const contact_constraint> prev_contacts,
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
			std::vector<contact_constraint>& contacts_out,
			std::span<joint_constraint> joints_out
		) -> void;

		auto has_readback_data(
		) const -> bool;

		auto pending_dispatch(
		) const -> bool;

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

		auto snapshot_buffer(
			std::uint32_t slot
		) const -> const gpu::buffer&;

		auto latest_snapshot_slot(
		) const -> std::uint32_t;

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
			gpu::pipeline apply_restitution_pipeline;

			bool initialized = false;
		} m_compute;

		struct per_frame_data {
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
			std::uint64_t dispatched_at_frame = 0;
		};

		per_frame_resource<per_frame_data> m_frames{ per_frame_data{}, per_frame_data{} };
		std::uint32_t m_dispatch_slot = 0;

		bool m_buffers_created = false;
		bool m_pending_dispatch = false;
		std::uint32_t m_frame_count = 0;
		bool m_body_buffers_seeded = false;
		std::uint32_t m_seeded_body_count = 0;

		std::uint32_t m_body_count = 0;
		std::uint32_t m_contact_count = 0;
		std::uint32_t m_motor_count = 0;
		std::uint32_t m_joint_count = 0;

		std::uint32_t m_steps = 1;
		gap m_grid_cell_size = meters(2.0f);
		solver_config m_solver_cfg;
		time_step m_dt{};

		std::uint32_t m_warm_start_count = 0;

		std::vector<velocity_motor_constraint> m_upload_motors;
		std::vector<contact_constraint> m_upload_warm_starts;
		std::vector<joint_constraint> m_upload_joints;
		std::vector<std::uint32_t> m_upload_motor_map;
		std::vector<std::uint32_t> m_upload_collision_state;

		std::vector<contact_constraint> m_staged_contacts;
		std::vector<joint_constraint> m_staged_joints;
		std::vector<body_state> m_staged_bodies;
		std::uint32_t m_staged_body_count = 0;
		std::uint32_t m_staged_contact_count = 0;
		std::uint32_t m_staged_joint_count = 0;
		bool m_staged_valid = false;

	};
}
