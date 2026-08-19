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
import gse.gpu_record;

import :narrow_phase_collision;
import :joint_drive_component;
import :joint_spec;
import :kinematic_target_component;
import :motion_component;
import :motor_component;
import :muscle_component;
import :collision_component;
import :convex_hull;
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
		int steps = 0;
		int readback_age_steps = 0;
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
			= gse::settings::describe<"Dispatch at most one GPU solver tick per frame and skip while the previous "
									  "batch is still executing; excess fixed-step demand is dropped, so overload "
									  "dilates the sim instead of multiplying substeps into ever-longer batches. "
									  "Interactive only: the skip decision follows real GPU timing, so leave this "
									  "off for benches and any run that must be hash-comparable.">{}
		]]
		bool gpu_async_dispatch = false;

		[[
			= gse::settings::describe<"Fold the GPU solver's per-colour Gauss-Seidel dispatches into one sweep dispatch "
									  "per iteration, sequencing colours inside the kernel with a device-scope barrier. "
									  "Bit-identical to the unfolded loop and gate-validated on both backends. The barrier "
									  "spins, so heavy graphics contention can preempt sweep workgroups and starve it; a "
									  "starved sweep bails fast, skips the rest of the batch, and drops the solver back "
									  "to the unfolded loop. The fold retries automatically after a cooldown and also "
									  "re-arms on reseed, world reset, or re-enable of this setting.">{}
		]]
		bool gpu_solve_fold = false;

		[[
			= gse::settings::describe<"Cap the GPU solver's colour palette. Bodies that cannot take a conflict-free "
									  "colour under the cap share one deterministically instead of opening a new "
									  "colour, so the solve runs fewer, fatter colour dispatches with fewer barriers "
									  "per iteration. Conflicting pairs degrade toward undamped Jacobi for that "
									  "iteration and are surfaced by the coloring conflict counter; small caps can "
									  "destabilize stiff piles. 0 keeps the natural colouring; -1 sizes the cap "
									  "automatically from live colour populations, folding tail colours below a "
									  "population floor and backing off when conflicts exceed a budget.">{},
			= gse::settings::range<-1, 16>{}
		]]
		int gpu_color_cap = 0;

		[[
			= gse::settings::describe<"Workgroups per folded sweep dispatch. More workgroups widen the sweep's stride "
									  "and raise occupancy on large scenes, but every workgroup must be co-resident "
									  "for the colour barrier; oversubscribing residency trips the sweep's bail "
									  "fallback. 0 keeps the built-in width of 32.">{},
			= gse::settings::range<0, 256>{}
		]]
		int gpu_sweep_workgroups = 0;

		[[
			= gse::settings::describe<"Number of constraint solver iterations per substep. Higher values reduce "
									  "jitter at the cost of frame time.">{},
			= gse::settings::range<1, 40>{},
			= gse::shared
		]]
		int solver_iterations = 15;

		[[
			= gse::settings::describe<"Upper bound on adaptive solver iterations per substep. Past solver_iterations "
									  "the solver keeps iterating only while the worst violation is above the "
									  "convergence thresholds and still shrinking.">{},
			= gse::settings::range<1, 64>{},
			= gse::shared
		]]
		int max_solver_iterations = 40;

		[[
			= gse::settings::describe<"Worst contact violation below which the adaptive iteration loop stops at the "
									  "solver_iterations floor.">{}
		]]
		length convergence_threshold_linear = meters(1e-4f);

		[[
			= gse::settings::describe<"Worst joint angular violation below which the adaptive iteration loop stops at "
									  "the solver_iterations floor.">{}
		]]
		angle convergence_threshold_angular = radians(1e-3f);

		[[
			= gse::settings::describe<"Scale the linear convergence threshold with scene motion: the effective "
									  "threshold becomes the larger of convergence_threshold_linear and this fraction "
									  "of the fastest dynamic body's travel per substep. Churning scenes stop "
									  "iterating once residual error is invisible against their own motion while "
									  "settling stacks keep the tight threshold and the full adaptive budget. Applies "
									  "to both solvers identically; 0 disables.">{},
			= gse::settings::range<0.f, 0.25f>{},
			= gse::shared
		]]
		float convergence_speed_scale = 0.f;

		[[
			= gse::settings::describe<"Fraction of each contact's pre-existing penetration carried forward per substep; "
									  "the remainder is corrected as position error. Lower values depenetrate faster "
									  "but inject more energy.">{},
			= gse::settings::range<0.f, 1.f>{}
		]]
		float solver_alpha = 0.99f;

		[[
			= gse::settings::describe<"Penalty ramp rate. Each dual update grows a violated contact row's penalty by "
									  "this stiffness per meter of violation.">{}
		]]
		stiffness_per_length solver_beta = newtons_per_meter_squared(100000.f);

		[[
			= gse::settings::describe<"Per-substep decay factor for contact penalties and warm-started duals.">{},
			= gse::settings::range<0.5f, 1.f>{}
		]]
		float solver_gamma = 0.99f;

		[[
			= gse::settings::describe<"Base penalty stiffness for inactive contact rows. Active rows floor at the "
									  "mass-scaled value instead.">{}
		]]
		stiffness penalty_min = newtons_per_meter(1.f);

		[[
			= gse::settings::describe<"Upper bound on contact penalty stiffness.">{}
		]]
		stiffness penalty_max = newtons_per_meter(1e9f);

		[[
			= gse::settings::describe<"Contact offset added to every separation before the solver sees it. Bodies come "
									  "to rest with this gap.">{}
		]]
		gap collision_margin = meters(0.0005f);

		[[
			= gse::settings::describe<"Grip envelope. Contacts within this distance keep their friction anchors and "
									  "warm-start stiffness memory.">{}
		]]
		gap stick_threshold = meters(0.01f);

		[[
			= gse::settings::describe<"Linear speed below which a body may begin falling asleep. Zero disables sleeping "
									  "entirely, which is what a capture run wants — a sleeping island is not woken by "
									  "having its support removed.">{}
		]]
		velocity velocity_sleep_threshold = meters_per_second(0.05f);

		[[
			= gse::settings::describe<"Angular speed below which a body may begin falling asleep. Paired with "
									  "velocity_sleep_threshold; both must be satisfied.">{}
		]]
		angular_velocity angular_sleep_threshold = radians_per_second(0.05f);

		[[
			= gse::settings::describe<"Maximum distance at which approaching pairs get speculative contacts. Each "
									  "pair's actual window scales with its relative speed, down to "
									  "speculative_margin_floor for calm pairs.">{}
		]]
		gap speculative_margin = meters(0.02f);

		[[
			= gse::settings::describe<"Minimum speculative contact window. Calm pairs use this window; it must stay "
									  "wider than settling pairs' gap oscillation or the contact set churns at the "
									  "boundary.">{}
		]]
		gap speculative_margin_floor = meters(0.01f);

		[[
			= gse::settings::describe<"Use Jacobi iteration instead of Gauss-Seidel. More parallel-friendly but converges slower.">{},
			= gse::shared
		]]
		bool use_jacobi = false;

		[[
			= gse::settings::describe<"Record the gpu solver's per-stage diagnostic hash dispatches every tick so ContactTrace "
									  "can print the pass-hash line. Off keeps the dispatch stream lean; the solve itself is "
									  "identical either way.">{},
			= gse::shared
		]]
		bool trace_hashes = false;

		[[
			= gse::settings::describe<"Relaxation factor for the Jacobi solver. Lower values are more stable; "
									  "higher values converge faster.">{},
			= gse::settings::range<0.1f, 1.0f>{},
			= gse::shared
		]]
		float jacobi_omega = 0.67f;

		[[
			= gse::settings::describe<"Solve bodies serially in height order, alternating sweep direction each "
									  "iteration, instead of the parallel colour sweep. Trades parallelism for "
									  "convergence on tall stacks.">{},
			= gse::shared
		]]
		bool use_ordered_sweep = false;

		[[
			= gse::settings::describe<"Number of substeps per simulation tick. More substeps improve stability for "
									  "fast-moving bodies.">{},
			= gse::settings::range<1, 8>{},
			= gse::shared
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

		bool gpu_unavailable_reported = false;
		id_mapped_collection<joint_definition> joints;
		[[= gse::shared]] std::vector<convex_hull> hulls;

		[[= gse::shared]] vbd::solver vbd_solver;
		vbd::contact_cache contact_cache;
		[[= gse::shared]] std::unordered_map<id, std::uint32_t> sleep_counters;
		bool gpu_joints_dirty = true;
		std::uint32_t gpu_uploaded_body_count = 0;
		std::uint32_t gpu_uploaded_joint_count = 0;
		[[= gse::shared]] std::flat_map<id, std::uint32_t> id_to_body_index;
		std::flat_map<id, transform_component> kinematic_step_start;
		std::vector<impulse_request> gpu_pending_impulses;
		[[= gse::shared]] int sim_steps_this_frame = 0;
		[[= gse::shared]] int gpu_readback_age_steps = 0;
		bool gpu_sweep_fold_bailed = false;
		bool gpu_solve_fold_prev = false;
		int gpu_sweep_retry_cooldown = 0;
		int gpu_sweep_retry_backoff = 0;
		int gpu_sweep_calm_frames = 0;
		int gpu_color_cap_resolved = 0;
		int gpu_color_cap_dwell = 0;
		int gpu_color_cap_min = 0;

		[[= gse::shared]] std::vector<std::uint8_t> body_airborne;
		[[= gse::shared]] std::vector<std::uint8_t> body_sleeping;

		[[= gse::shared]] vbd::gpu_solver gpu_solver;
	};

	struct collision_pair {
		id owner;
		aabb box;
		const transform_component* tc;
		const collision_component* cc;
		const convex_hull* hull;
		std::uint32_t body_index;
		float restitution;
	};

	struct candidate_pair {
		std::uint32_t a;
		std::uint32_t b;
	};

	struct body_build_view {
		std::span<const id> motion_owners;
		std::span<const motion_component> motions;
		std::span<const id> transform_owners;
		std::span<const transform_component> transforms;
		std::span<const id> collision_owners;
		std::span<const collision_component> collisions;
		std::span<const convex_hull> hulls;
		std::span<const mass_properties> mass_props;
	};

	auto resolve_hull(
		const collision_shape& shape,
		std::span<const convex_hull> hulls
	) -> const convex_hull*;

	auto solver_config_from_settings(
		const data& d
	) -> vbd::solver_config;

	auto gpu_solver_active(
		const data& d
	) -> bool;

	auto gpu_solver_active(
		shared_view<data> d
	) -> bool;

	auto gpu_solver_frame_info_of(
		const data& d
	) -> gpu_solver_frame_info;

	auto build_mass_properties(
		const body_build_view& view,
		std::vector<mass_properties>& out
	) -> void;

	auto build_body_states(
		const body_build_view& view,
		const std::unordered_map<id, std::uint32_t>& sleep_counters,
		std::vector<vbd::body_state>& bodies,
		std::flat_map<id, std::uint32_t>& id_to_body_index,
		std::vector<std::uint8_t>& has_transform
	) -> void;

	auto build_body_bounds(
		const body_build_view& view,
		const std::flat_map<id, std::uint32_t>& id_to_body_index,
		std::span<const std::uint8_t> has_transform,
		std::span<vbd::body_state> bodies
	) -> void;

	auto build_joint_constraints(
		std::span<joint_definition> definitions,
		const std::flat_map<id, std::uint32_t>& id_to_body_index,
		std::span<const vbd::body_state> bodies,
		std::vector<vbd::joint_constraint>& out
	) -> void;

	auto build_motor_constraints(
		read<motor_component>& motor,
		write<motion_component>& motion,
		const std::flat_map<id, std::uint32_t>& id_to_body_index,
		std::span<const std::uint8_t> body_airborne,
		std::span<vbd::body_state> bodies,
		std::vector<vbd::velocity_motor_constraint>& out
	) -> void;

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
		channel_write<interpolation_state> interp_out,
		write<joint_spec> specs,
		read<muscle_component> muscles,
		read<joint_drive_component> drives,
		read<kinematic_target_component> targets,
		write<transform_component> transform,
		write<motion_component> motion
	) -> async::task<>;

	[[= gse::system_run<1>{}]]
	auto ensure_results(
		write<collision_component> collision,
		structural<collision_result_component> results
	) -> async::task<>;

	[[= gse::system_run<2>{}]]
	auto integrate(
		context& ctx,
		data& d,
		channel_read<impulse_request, reset_physics_request> requests_in,
		channel_write<gpu_solver_frame_info, vbd::solver_upload> solver_out,
		write<transform_component> transform,
		write<motion_component> motion,
		read<motor_component> motor,
		write<collision_component> collision,
		write<collision_result_component> results,
		write<hull_definition> hull_definitions
	) -> async::task<>;

	[[= gse::system_frame{}]]
	auto frame(
		context& ctx,
		std::optional<shared_view<gpu::context::data>> gpu_s,
		data& d,
		channel_read<vbd::solver_upload> uploads_in,
		channel_write<gpu_solver_frame_info> frame_out,
		channel_write<gpu::render_pass_request> pass_out
	) -> async::task<>;

	auto create_joint(
		data& d,
		id owner,
		const joint_definition& def
	) -> void;

	auto remove_joint(
		data& d,
		id owner
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
		write<collision_component>& collision,
		write<motion_component>& motion,
		const std::flat_map<id, std::uint32_t>& id_to_body_index,
		std::span<const convex_hull> hulls
	) -> std::vector<collision_pair>;

	auto build_pair_set(
		std::vector<collision_pair>& objects,
		const std::flat_set<std::pair<std::uint64_t, std::uint64_t>>& jointed_pairs,
		gap sweep_margin,
		std::size_t chunks_per_worker
	) -> std::vector<candidate_pair>;

	auto pair_set_valid(
		std::span<const collision_pair> objects,
		length max_travel
	) -> bool;

	auto add_scene_contacts_to_solver(
		vbd::solver& solver,
		vbd::contact_cache& contact_cache,
		std::span<const collision_pair> objects,
		std::span<const candidate_pair> candidates,
		vbd::time_step sub_dt,
		write<collision_result_component>& results,
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
		channel_write<gpu_solver_frame_info, vbd::solver_upload> channels,
		bool reset
	) -> void;
}
