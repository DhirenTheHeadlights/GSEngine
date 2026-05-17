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
	struct [[= shaders::shader_struct]] dispatch_args {
		std::uint32_t x;
		std::uint32_t y;
		std::uint32_t z;
	};

	using shader_types = type_pack<
		vbd_limits,
		joint_type,
		solver_config,
		body_state,
		contact_constraint,
		velocity_motor_constraint,
		joint_constraint,
		impulse_constraint,
		frozen_jacobian,
		dispatch_args
	>;

	struct vbd_solve_chain {};

	struct vbd_clear_state_buffers_stage {};
	struct vbd_collision_reset_stage {};
	struct vbd_grid_build_stage {};
	struct vbd_broad_phase_stage {};
	struct vbd_prepare_indirect_stage {};
	struct vbd_narrow_phase_stage {};
	struct vbd_prepare_contact_indirect_stage {};
	struct vbd_build_adjacency_stage {};
	struct vbd_build_coloring_stage {};
	struct vbd_apply_impulses_stage {};
	struct vbd_predict_stage {};
	struct vbd_freeze_jacobians_stage {};
	struct vbd_solve_iterations_stage {};
	struct vbd_derive_velocities_stage {};
	struct vbd_apply_restitution_stage {};
	struct vbd_post_stabilize_stage {};
	struct vbd_finalize_stage {};
	struct vbd_state_copy_stage {};

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
			std::span<const impulse_constraint> impulses,
			const solver_config& solver_cfg,
			time_step dt,
			int steps,
			bool refresh_joints
		) -> void;

		auto total_substeps(
		) const -> std::uint32_t;

		auto commit_upload(
		) -> void;

		auto read_grounded(
		) const -> std::span<const std::uint32_t>;

		auto query_body_snapshot(
			std::uint32_t body_index
		) const -> std::optional<body_state>;

		auto pending_dispatch(
		) const -> bool;

		auto body_count(
		) const -> std::uint32_t;

		auto motor_count(
		) const -> std::uint32_t;

		auto joint_count(
		) const -> std::uint32_t;

		auto solver_cfg(
		) const -> const solver_config&;

		auto dt(
		) const -> time_step;

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
			gpu::pipeline collision_build_coloring_pipeline;
			gpu::pipeline update_joint_lambda_pipeline;
			gpu::pipeline prepare_indirect_pipeline;
			gpu::pipeline prepare_contact_indirect_pipeline;
			gpu::pipeline prepare_color_indirect_pipeline;
			gpu::pipeline freeze_jacobians_pipeline;
			gpu::pipeline apply_jacobi_pipeline;
			gpu::pipeline apply_restitution_pipeline;
			gpu::pipeline apply_impulses_pipeline;

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
			gpu::buffer collision_pair_buffer;
			gpu::buffer collision_state_buffer;
			gpu::buffer warm_start_buffer;
			gpu::buffer joint_buffer;
			gpu::buffer grid_buffer;
			gpu::buffer physics_snapshot_buffer;
			gpu::buffer indirect_dispatch_buffer;
			gpu::buffer frozen_jacobian_buffer;
			gpu::buffer solve_deltas_buffer;
			gpu::buffer grounded_buffer;
			gpu::buffer grounded_readback_buffer;
			gpu::buffer impulse_buffer;

			bool grounded_valid = false;
		};

		per_frame_resource<per_frame_data> m_frames{ per_frame_data{}, per_frame_data{} };
		std::uint32_t m_dispatch_slot = 0;

		bool m_buffers_created = false;
		bool m_pending_dispatch = false;
		bool m_body_buffers_seeded = false;
		std::uint32_t m_seeded_body_count = 0;
		bool m_joint_buffers_seeded = false;

		std::uint32_t m_body_count = 0;
		std::uint32_t m_motor_count = 0;
		std::uint32_t m_joint_count = 0;
		std::uint32_t m_impulse_count = 0;

		std::uint32_t m_steps = 1;
		gap m_grid_cell_size = meters(2.0f);
		solver_config m_solver_cfg;
		time_step m_dt{};

		std::uint32_t m_warm_start_count = 0;

		std::vector<velocity_motor_constraint> m_upload_motors;
		std::vector<joint_constraint> m_upload_joints;
		std::vector<impulse_constraint> m_upload_impulses;
		std::vector<std::uint32_t> m_upload_motor_map;
		std::vector<std::uint32_t> m_upload_collision_state;
		bool m_upload_joints_dirty = false;
	};
}
