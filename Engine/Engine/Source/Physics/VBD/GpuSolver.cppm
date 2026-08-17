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
import gse.gpu_record;

import :vbd_constraints;
import :vbd_solver;

export namespace gse::vbd {
	struct [[= shaders::shader_struct]] dispatch_args {
		std::uint32_t x;
		std::uint32_t y;
		std::uint32_t z;
	};

	using shader_types = type_pack<vbd_limits, joint_type, solver_config, body_state, contact_constraint, velocity_motor_constraint, joint_constraint, impulse_constraint, frozen_jacobian, dispatch_args>;

	struct vbd_solve_chain {};

	struct vbd_apply_body_inputs_stage {};
	struct vbd_render_mirror_stage {};
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
	struct vbd_update_sticking_stage {};
	struct vbd_prepare_color_indirect_stage {};
	struct vbd_post_stabilize_stage {};
	struct vbd_finalize_stage {};
	struct vbd_state_copy_stage {};
	struct vbd_hash_state_stage {};

	struct solver_diagnostics {
		std::uint32_t attempted_contacts = 0;
		std::uint32_t max_used_color = 0;
		std::uint32_t coloring_fallbacks = 0;
		std::uint32_t coloring_conflicts = 0;
		std::uint32_t contact_duplicates = 0;
		std::uint32_t warm_start_hits = 0;
		std::uint32_t stale_reads = 0;
		std::uint32_t stale_checks = 0;
		std::uint32_t sweep_bails = 0;
		std::array<std::uint32_t, limits.max_colors> color_populations = {};
		std::uint32_t broad_nodes_walked = 0;
		std::uint32_t broad_aabb_tests = 0;
		std::uint32_t broad_pairs = 0;
		force joint_lambda_max{};
		stiffness joint_penalty_max{};
		gap joint_c_max{};
		gap joint_c0_max{};

		auto operator==(
			const solver_diagnostics&
		) const -> bool = default;
	};

	struct solver_upload {
		std::vector<body_state> bodies;
		std::vector<velocity_motor_constraint> motors;
		std::vector<joint_constraint> joints;
		std::vector<impulse_constraint> impulses;
		solver_config solver_cfg;
		time_step dt{};
		int steps = 1;
		bool refresh_joints = false;
		bool force_reseed = false;
	};

	class gpu_solver {
	public:
		auto create_buffers(
			shared_view<gpu::context::data> ctx
		) -> void;

		auto initialize_compute(
			context& ctx,
			shared_view<gpu::context::data> gpu_s
		) -> async::task<>;

		auto dispatch_compute(
			context& ctx,
			channel_write<gpu::render_pass_request> pass_out
		) -> async::task<>;

		auto compute_initialized() const -> bool;

		auto buffers_created() const -> bool;

		auto upload(
			const solver_upload& payload
		) -> void;

		auto commit_upload() -> void;

		auto set_preserve_warm_starts(
			bool preserve
		) -> void;

		auto set_color_launch_hint(
			std::uint32_t max_used_color
		) -> void;

		auto read_grounded() const -> std::span<const std::uint32_t>;

		auto read_narrow_phase_debug() const -> std::span<const std::uint32_t>;

		auto read_contact_dump() const -> std::span<const contact_constraint>;

		auto read_adjacency_meta() const -> std::span<const std::uint32_t>;

		auto read_contact_adjacency_dump() const -> std::span<const std::uint32_t>;

		auto read_body_states() const -> std::span<const body_state>;

		auto diagnostics() const -> solver_diagnostics;

		auto query_body_snapshot(
			std::uint32_t body_index
		) const -> std::optional<body_state>;

		auto pending_dispatch() const -> bool;

		auto body_count() const -> std::uint32_t;

		auto reseeding() const -> bool;

		auto motor_count() const -> std::uint32_t;

		auto joint_count() const -> std::uint32_t;

		auto solver_cfg() const -> const solver_config&;

		auto dt() const -> time_step;

		auto render_body_buffer() const -> const gpu::buffer&;

		auto dispatch_generation() const -> std::uint64_t;

		auto retired_generation() const -> std::uint64_t;

		auto latest_dispatch_complete() const -> bool;

	private:
		struct compute_shaders {
			gpu::shader_program predict_pipeline;
			gpu::shader_program solve_color_pipeline;
			gpu::shader_program solve_sweep_pipeline;
			gpu::shader_program update_lambda_pipeline;
			gpu::shader_program derive_velocities_pipeline;
			gpu::shader_program finalize_pipeline;
			gpu::shader_program collision_reset_pipeline;
			gpu::shader_program collision_grid_build_pipeline;
			gpu::shader_program collision_broad_phase_pipeline;
			gpu::shader_program collision_narrow_phase_pipeline;
			gpu::shader_program collision_build_adjacency_pipeline;
			gpu::shader_program collision_sort_adjacency_pipeline;
			gpu::shader_program collision_build_coloring_pipeline;
			gpu::shader_program collision_color_round_pipeline;
			gpu::shader_program collision_color_commit_pipeline;
			gpu::shader_program update_joint_lambda_pipeline;
			gpu::shader_program prepare_indirect_pipeline;
			gpu::shader_program prepare_contact_indirect_pipeline;
			gpu::shader_program prepare_color_indirect_pipeline;
			gpu::shader_program convergence_check_pipeline;
			gpu::shader_program freeze_jacobians_pipeline;
			gpu::shader_program apply_jacobi_pipeline;
			gpu::shader_program apply_restitution_pipeline;
			gpu::shader_program update_sticking_pipeline;
			gpu::shader_program apply_impulses_pipeline;
			gpu::shader_program apply_body_inputs_pipeline;
			gpu::shader_program hash_state_pipeline;
			gpu::shader_program hash_warm_inputs_pipeline;
			gpu::shader_program hash_adjacency_pipeline;
			gpu::shader_program hash_colors_pipeline;
			gpu::shader_program hash_bodies_pipeline;

			bool initialized = false;
			bool device_local_seeded = false;
		} m_compute;

		struct per_frame_data {
			gpu::buffer body_buffer;
			gpu::bindless_handle body_alt_view;
			gpu::buffer contact_buffer;
			gpu::buffer color_buffer;
			gpu::buffer jointless_color_buffer;
			gpu::buffer contact_offsets_buffer;
			gpu::buffer contact_counts_buffer;
			gpu::buffer contact_adjacency_buffer;
			gpu::buffer joint_offsets_buffer;
			gpu::buffer joint_counts_buffer;
			gpu::buffer joint_adjacency_buffer;
			gpu::buffer solve_state_buffer;
			gpu::buffer collision_pair_buffer;
			gpu::buffer collision_state_buffer;
			gpu::buffer warm_start_buffer;
			gpu::buffer joint_buffer;
			gpu::buffer grid_buffer;
			gpu::buffer indirect_dispatch_buffer;
			gpu::buffer jointless_indirect_dispatch_buffer;
			gpu::buffer frozen_jacobian_buffer;
			gpu::buffer solve_deltas_buffer;
			gpu::buffer grounded_buffer;
			gpu::buffer coloring_scratch_buffer;
			gpu::buffer render_body_buffer;
		};

		per_frame_resource<per_frame_data> m_frames{ per_frame_data{}, per_frame_data{} };
		const gpu::frame* m_frame = nullptr;
		std::uint32_t m_dispatch_slot = 0;
		std::uint64_t m_dispatch_generation = 0;
		std::uint32_t m_recorded_ring = 0;
		std::uint64_t m_recorded_frame = 0;

		gpu::upload_channel m_body_input_channel;
		gpu::upload_channel m_motor_channel;
		gpu::upload_channel m_motor_map_channel;
		gpu::upload_channel m_solver_config_channel;
		gpu::upload_channel m_impulse_channel;
		gpu::upload_channel m_jointed_pairs_channel;
		gpu::upload_channel m_island_channel;
		gpu::upload_channel m_body_env_channel;
		gpu::upload_channel m_static_bodies_channel;
		gpu::upload_channel m_joint_upload_channel;

		gpu::readback_channel m_snapshot_channel;
		gpu::readback_channel m_grounded_channel;
		gpu::readback_channel m_collision_state_channel;
		gpu::readback_channel m_contact_dump_channel;
		gpu::readback_channel m_adjacency_meta_channel;
		gpu::readback_channel m_contact_adjacency_channel;

		bool m_buffers_created = false;
		bool m_pending_dispatch = false;
		bool m_body_buffers_seeded = false;
		std::uint32_t m_seeded_body_count = 0;
		bool m_joint_buffers_seeded = false;
		bool m_apply_all_body_inputs = false;
		bool m_preserve_warm_starts = false;

		std::uint32_t m_body_count = 0;
		std::uint32_t m_motor_count = 0;
		std::uint32_t m_joint_count = 0;
		std::uint32_t m_color_launch_hint = 0;
		std::uint32_t m_island_count = 0;
		std::uint32_t m_jointless_body_count = 0;
		std::uint32_t m_impulse_count = 0;

		std::uint32_t m_steps = 1;
		gap m_grid_cell_size = meters(2.0f);
		solver_config m_solver_cfg;
		time_step m_dt{};

		std::vector<velocity_motor_constraint> m_upload_motors;
		std::vector<joint_constraint> m_upload_joints;
		std::vector<impulse_constraint> m_upload_impulses;
		std::vector<std::uint32_t> m_upload_motor_map;
		std::vector<std::uint32_t> m_upload_jointed_pairs;
		std::vector<std::uint32_t> m_upload_islands;
		std::vector<std::uint32_t> m_upload_body_env;
		std::vector<std::uint32_t> m_upload_static_bodies;
		std::vector<std::uint8_t> m_jointed_body_mask;
		bool m_upload_joints_dirty = false;
	};
}
