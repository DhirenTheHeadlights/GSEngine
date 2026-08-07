export module gse.physics:system;

import std;

import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.time;
import gse.diag;
import gse.save;
import gse.log;
import gse.gpu;

import gse.math;
import gse.meta;
import gse.gpu;

import :narrow_phase_collision;
import :joint_drive_component;
import :joint_spec;
import :kinematic_target_component;
import :motion_component;
import :motor_component;
import :muscle_component;
import :collision_component;
import :transform_component;
import :contact_manifold;
import :vbd_constraints;
import :vbd_contact_cache;
import :vbd_solver;
import :vbd_gpu_solver;

export namespace gse::physics {
	struct joint_definition {
		id entity_a;
		id entity_b;
		vbd::joint_type type = vbd::joint_type::distance;
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
		bool rest_orientation_initialized = false;

		vec3<force> pos_lambda;
		vec3<stiffness> pos_penalty;
		vec3<torque> ang_lambda;
		vec3<angular_stiffness> ang_penalty;
		torque limit_lambda = {};
		angular_stiffness limit_penalty = {};

		vec3<angular_stiffness> soft_ang_stiffness = {};

		float activation = 0.f;
		force max_force = newtons(0.f);

		vec3<angle> drive_target = {};
		vec3<angular_stiffness> drive_stiffness = {};
		float drive_damping = 0.f;
		torque drive_max_torque = {};
	};

	using joint_handle = std::uint32_t;

	struct gpu_upload_payload {
		std::vector<vbd::body_state> bodies;
		std::vector<vbd::velocity_motor_constraint> motors;
		std::vector<vbd::joint_constraint> joints;
		std::vector<vbd::impulse_constraint> impulses;
		vbd::solver_config solver_cfg;
		time_t<float, seconds> dt{};
		int steps = 1;
		bool refresh_joints = false;
		bool force_reseed = false;
	};

	struct gpu_body_index_map {
		std::vector<std::pair<id, std::uint32_t>> entries;
	};

	struct gpu_solver_stats {
		bool active = false;
		std::uint32_t motor_count = 0;
	};

	struct reset_physics_request {};

	struct transform_snapshot {
		vec3<position> position;
		quat orientation;
	};

	struct gpu_solver_frame_info {
		const gpu::buffer* snapshot = nullptr;
		std::uint32_t body_count = 0;
		std::uint32_t body_stride = 0;
		std::uint32_t position_offset = 0;
	};

	struct interpolation_state {
		bool advancing = true;
	};

	struct [[= gse::system_state<"Physics">{}, = gse::settings::category<"Physics">{}, = gse::deferred_system{}]] data {
		[[= gse::settings::describe<"Step the physics world each frame.">{}]] bool update_phys = true;

		[[
			= gse::settings::describe<"Run the constraint solver on the GPU instead of the CPU. The GPU pipelines and "
									  "buffers are built once during startup, so this requires a restart.">{},
			= gse::settings::restart_required{},
			= gse::shared
		]]
		bool use_gpu_solver = false;

		[[
			= gse::settings::describe<"Number of constraint solver iterations per substep. Higher values reduce "
									  "jitter at the cost of frame time.">{},
			= gse::settings::range<1, 40>{}
		]]
		int solver_iterations = 15;

		[[
			= gse::settings::describe<"Use Jacobi iteration instead of Gauss-Seidel. More parallel-friendly but converges slower.">{}
		]]
		bool use_jacobi = false;

		[[
			= gse::settings::describe<"Relaxation factor for the Jacobi solver. Lower values are more stable; "
									  "higher values converge faster.">{},
			= gse::settings::range<0.1f, 1.0f>{}
		]]
		float jacobi_omega = 0.67f;

		[[
			= gse::settings::describe<"Number of substeps per simulation tick. More substeps improve stability for "
									  "fast-moving bodies.">{},
			= gse::settings::range<1, 8>{}
		]]
		int physics_substeps = 2;

		[[
			= gse::settings::describe<"Bodies per parallel chunk in the constraint solver colour sweep. Lower values "
									  "spread the sweep across more threads at the cost of scheduling overhead.">{},
			= gse::settings::range<1, 256>{}
		]]
		int color_chunk_grain = 8;

		[[
			= gse::settings::describe<"Parallel chunks per worker in the broad phase. The pair test does more work for "
									  "early objects than late ones, so higher values balance the load at the cost of "
									  "scheduling overhead.">{},
			= gse::settings::range<1, 32>{}
		]]
		int broad_phase_chunks_per_worker = 8;

		bool gpu_buffers_created = false;
		gpu_solver_stats gpu_stats;
		std::vector<joint_definition> joints;

		vbd::solver vbd_solver;
		vbd::contact_cache contact_cache;
		std::unordered_map<id, std::uint32_t> sleep_counters;
		bool gpu_joints_dirty = true;
		std::uint32_t gpu_uploaded_body_count = 0;
		std::uint32_t gpu_uploaded_joint_count = 0;
		[[= gse::shared]] std::flat_map<id, std::uint32_t> id_to_body_index;
		std::flat_map<id, joint_handle> joint_handles_by_entity;
		std::flat_map<id, transform_component> kinematic_step_start;
		std::vector<impulse_request> gpu_pending_impulses;

		[[= gse::shared]] std::vector<std::uint8_t> body_airborne;
		[[= gse::shared]] std::vector<std::uint8_t> body_sleeping;

		[[= gse::shared]] vbd::gpu_solver gpu_solver;
	};

	struct collision_pair {
		id owner;
		aabb box;
	};

	[[= gse::system_init{}]]
	auto init(
		context& ctx,
		std::optional<shared_view<gpu::context::data>> gpu_s,
		data& d
	) -> async::task<>;

	[[= gse::system_run<>{}, = gse::runs_after_optional<^^gpu::context::data>{}]]
	auto prepare(
		context& ctx,
		data& d,
		write<joint_spec> specs,
		read<muscle_component> muscles,
		read<joint_drive_component> drives,
		read<kinematic_target_component> targets,
		write<transform_component> transform,
		write<motion_component> motion
	) -> async::task<>;

	[[= gse::system_run<1>{}]]
	auto ensure_results(
		context& ctx,
		data& d,
		structural<collision_result_component> results
	) -> async::task<>;

	[[= gse::system_run<2>{}]]
	auto integrate(
		context& ctx,
		data& d,
		write<transform_component> transform,
		write<motion_component> motion,
		read<motor_component> motor,
		write<collision_component> collision,
		write<collision_result_component> results
	) -> async::task<>;

	[[= gse::system_frame{}]]
	auto frame(
		context& ctx,
		std::optional<shared_view<gpu::context::data>> gpu_s,
		data& d
	) -> async::task<>;

	auto create_joint(
		data& d,
		const joint_definition& def
	) -> joint_handle;

	auto remove_joint(
		data& d,
		joint_handle handle
	) -> void;

	auto query_transform(
		shared_view<data> d,
		id entity_id
	) -> std::optional<transform_snapshot>;

	auto is_airborne(
		shared_view<data> d,
		id entity_id
	) -> bool;

	auto is_sleeping(
		shared_view<data> d,
		id entity_id
	) -> bool;

	auto apply_kinematic_targets(
		read<kinematic_target_component>& targets,
		write<transform_component>& transform,
		write<motion_component>& motion,
		std::flat_map<id, transform_component>& step_start,
		time_t<float, seconds> dt
	) -> void;

	auto collect_collision_objects(
		write<transform_component>& transform,
		write<collision_component>& collision
	) -> std::vector<collision_pair>;

	auto add_scene_contacts_to_solver(
		vbd::solver& solver,
		vbd::contact_cache& contact_cache,
		std::vector<collision_pair>& objects,
		const std::flat_map<id, std::uint32_t>& id_to_body_index,
		const std::flat_set<std::pair<std::uint64_t, std::uint64_t>>& jointed_pairs,
		bool update_scene_state,
		write<transform_component>& transform,
		write<motion_component>& motion,
		write<collision_component>& collision,
		write<collision_result_component>* results,
		std::span<std::uint8_t> body_airborne,
		std::size_t chunks_per_worker
	) -> void;

	auto update_vbd(
		int steps,
		data& d,
		write<transform_component>& transform,
		write<motion_component>& motion,
		read<motor_component>& motor,
		write<collision_component>& collision,
		write<collision_result_component>& results,
		std::span<const impulse_request> impulses
	) -> void;

	auto update_vbd_gpu(
		int steps,
		data& d,
		write<transform_component>& transform,
		write<motion_component>& motion,
		read<motor_component>& motor,
		write<collision_component>& collision,
		write<collision_result_component>& results,
		std::span<const impulse_request> impulses,
		time_t<float, seconds> dt,
		channel_writer& channels,
		bool reset
	) -> void;
}
