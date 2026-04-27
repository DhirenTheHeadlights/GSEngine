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

	struct buffer_layout {
		std::uint32_t stride = 0;
		flat_map<std::string, std::uint32_t> offsets;

		auto operator[](const std::string& name) const -> std::uint32_t {
			return offsets.at(name);
		}
	};

	constexpr std::uint32_t flag_locked = 1u;
	constexpr std::uint32_t flag_update_orientation = 2u;
	constexpr std::uint32_t flag_affected_by_gravity = 4u;

	struct collision_body_data {
		vec3<displacement> half_extents;
		vec3<position> aabb_min;
		vec3<position> aabb_max;
	};

	struct warm_start_entry {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;
		std::uint64_t feature_key = 0;
		bool sticking = false;
		vec3f normal;
		vec3f tangent_u;
		vec3f tangent_v;
		vec3<lever_arm> local_anchor_a;
		vec3<lever_arm> local_anchor_b;
		vec3<force> lambda;
		vec3<stiffness> penalty;
	};

	struct contact_readback_entry {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;
		std::uint64_t feature_key = 0;
		bool sticking = false;
		vec3f normal;
		vec3f tangent_u;
		vec3f tangent_v;
		vec3<lever_arm> local_anchor_a;
		vec3<lever_arm> local_anchor_b;
		vec3<gap> c0;
		vec3<force> lambda;
		vec3<stiffness> penalty;
		float friction_coeff = 0.f;
	};

	class gpu_solver {
	public:
		~gpu_solver();

		auto create_buffers(
			gpu::context& ctx
		) -> void;

		auto initialize_compute(
			gpu::context& ctx,
			asset_registry<gpu::context>& assets
		) -> void;

		auto dispatch_compute(
		) -> void;

		auto compute_initialized(
		) const -> bool;

		auto buffers_created(
		) const -> bool;

		auto body_stride(
		) const -> std::uint32_t;

		auto contact_stride(
		) const -> std::uint32_t;

		auto contact_lambda_offset(
		) const -> std::uint32_t;

		auto upload(
			std::span<const body_state> bodies,
			std::span<const collision_body_data> collision_data,
			std::span<const float> accel_weights,
			std::span<const velocity_motor_constraint> motors,
			std::span<const joint_constraint> joints,
			std::span<const warm_start_entry> prev_contacts,
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
			std::vector<contact_readback_entry>& contacts_out,
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

		auto body_layout(
		) const -> const buffer_layout&;

	private:
		struct compute_shaders {
			resource::handle<shader> predict;
			resource::handle<shader> solve_color;
			resource::handle<shader> update_lambda;
			resource::handle<shader> derive_velocities;
			resource::handle<shader> finalize;
			resource::handle<shader> collision_reset;
			resource::handle<shader> collision_broad_phase;
			resource::handle<shader> collision_narrow_phase;
			resource::handle<shader> collision_build_adjacency;
			resource::handle<shader> collision_grid_build;
			resource::handle<shader> update_joint_lambda;
			resource::handle<shader> prepare_indirect;
			resource::handle<shader> prepare_contact_indirect;
			resource::handle<shader> prepare_color_indirect;
			resource::handle<shader> freeze_jacobians;
			resource::handle<shader> apply_jacobi;

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

		buffer_layout m_body_layout;
		buffer_layout m_contact_layout;
		buffer_layout m_motor_layout;
		buffer_layout m_warm_start_layout;
		buffer_layout m_joint_layout;
		buffer_layout m_frozen_jacobian_layout;

		std::uint32_t m_warm_start_count = 0;

		std::vector<std::byte> m_upload_body_data;
		std::vector<std::byte> m_upload_motor_data;
		std::vector<std::byte> m_upload_warm_start_data;
		std::vector<std::byte> m_upload_joint_data;
		std::vector<std::uint32_t> m_upload_motor_map;
		std::vector<std::uint32_t> m_upload_collision_state;
		std::vector<std::uint8_t> m_upload_authoritative_bodies;

		std::vector<std::byte> m_staged_contact_data;
		std::vector<std::byte> m_staged_joint_data;
		std::uint32_t m_staged_body_count = 0;
		std::uint32_t m_staged_contact_count = 0;
		std::uint32_t m_staged_joint_count = 0;
		std::uint32_t m_staged_color_count = max_colors;
		bool m_staged_valid = false;

		std::vector<std::byte> m_readback_staging;
		std::vector<std::byte> m_latest_gpu_body_data;
		std::uint32_t m_latest_gpu_body_count = 0;
	};
}
