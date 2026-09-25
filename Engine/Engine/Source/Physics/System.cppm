export module gse.physics:system;

import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.gpu;
import gse.gpu;
import gse.gpu_record;
import gse.log;
import gse.math;
import gse.meta;
import gse.save;
import gse.time;
import std;

import :collision_component;
import :convex_hull;
import :joint_drive_component;
import :joint_spec;
import :kinematic_target_component;
import :motion_component;
import :motor_component;
import :muscle_component;
import :transform_component;
import :vbd_constraints;
import :vbd_contact_cache;
import :vbd_gpu_solver;
import :vbd_solver;

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

	struct motor_input {
		id owner;
		motor_component motor;
	};

	struct kinematic_input {
		id owner;
		transform_component start;
		vec3<velocity> velocity;
		vec3<angular_velocity> angular_velocity;
	};

	struct step_inputs {
		std::vector<motor_input> motors;
		std::vector<kinematic_input> kinematics;
		std::vector<impulse_request> impulses;
	};

	struct motor_override {
		std::uint64_t step = 0;
		id owner;
		motor_component motor;
	};

	struct body_snapshot {
		id owner;
		transform_component transform;
		motion_component motion;
	};

	struct body_override {
		std::uint64_t step = 0;
		id owner;
		transform_component transform;
		motion_component motion;
	};

	struct rollback_request {
		std::uint32_t steps = 0;
		std::vector<body_override> body_overrides;
		std::vector<motor_override> overrides;
	};

	struct rollback_history_request {
		int steps = 0;
	};

	struct carried_body_state {
		id owner;
		vec3<velocity> previous_velocity;
		float accel_weight = 0.f;
	};

	class sleep_counter_table {
	public:
		auto clear() -> void {
			m_ids.clear();
			m_values.clear();
			m_index.clear();
		}

		auto set(const id owner, const std::uint32_t value) -> void {
			m_values[slot(owner)] = value;
		}

		[[nodiscard]] auto get(const id owner) const -> std::uint32_t {
			const auto it = m_index.find(owner);
			return it == m_index.end() ? 0u : m_values[it->second];
		}

		[[nodiscard]] auto view(const std::span<const id> owners) const -> std::optional<std::span<const std::uint32_t>> {
			if (!prefix_matches(owners)) {
				return std::nullopt;
			}
			return std::span<const std::uint32_t>(m_values).first(owners.size());
		}

		auto aligned(const std::span<const id> owners) -> std::span<std::uint32_t> {
			if (!prefix_matches(owners)) {
				rekey(owners);
			}
			return std::span<std::uint32_t>(m_values).first(owners.size());
		}

	private:
		[[nodiscard]] auto prefix_matches(const std::span<const id> owners) const -> bool {
			return m_ids.size() >= owners.size() && std::ranges::equal(std::span<const id>(m_ids).first(owners.size()), owners);
		}

		auto slot(const id owner) -> std::size_t {
			const auto it = m_index.find(owner);
			if (it != m_index.end()) {
				return it->second;
			}
			m_ids.push_back(owner);
			m_values.push_back(0u);
			m_index.emplace(owner, m_ids.size() - 1);
			return m_ids.size() - 1;
		}

		auto rekey(const std::span<const id> owners) -> void {
			std::vector<id> ids(owners.begin(), owners.end());
			std::vector<std::uint32_t> values(owners.size());
			std::unordered_map<id, std::size_t> index;
			index.reserve(owners.size() + m_ids.size());
			for (std::size_t i = 0; i < owners.size(); ++i) {
				values[i] = get(owners[i]);
				index.emplace(owners[i], i);
			}
			for (std::size_t i = 0; i < m_ids.size(); ++i) {
				if (index.contains(m_ids[i])) {
					continue;
				}
				index.emplace(m_ids[i], ids.size());
				ids.push_back(m_ids[i]);
				values.push_back(m_values[i]);
			}
			m_ids = std::move(ids);
			m_values = std::move(values);
			m_index = std::move(index);
		}

		std::vector<id> m_ids;
		std::vector<std::uint32_t> m_values;
		std::unordered_map<id, std::size_t> m_index;
	};

	struct step_snapshot {
		std::uint64_t step = 0;
		std::vector<body_snapshot> bodies;
		std::vector<carried_body_state> carried;
		sleep_counter_table sleep_counters;
		id_mapped_collection<joint_definition> joints;
		vbd::contact_cache contact_cache;
		step_inputs inputs;
	};

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

	struct gpu_tick_plan {
		bool active = false;
		bool reset = false;
		int replay_ticks = 0;
		int total_ticks = 0;
		std::uint64_t first_tick = 0;
		std::optional<std::uint64_t> restore_tick;
		std::vector<step_inputs> per_tick;
		std::uint64_t generation = 0;
	};

	struct joint_rest_orientation {
		id owner;
		quat rest_orientation;
	};

	struct recorded_tick_inputs {
		std::uint64_t tick = 0;
		step_inputs inputs;
	};

	struct gpu_upload_report {
		std::flat_map<id, std::uint32_t> body_index;
		std::vector<joint_rest_orientation> rest_orientations;
		std::vector<recorded_tick_inputs> recorded;
	};

	struct [[= same_frame_channel]] gpu_upload_payload {
		vbd::solver_upload upload;
	};

	auto sim_transform(
		const transform_component& tc,
		const motion_component* mc,
		const interpolation_state& interpolation
	) -> transform_component;

	auto render_lag(
		const interpolation_state& interpolation
	) -> time_t<float, seconds>;

	auto render_transform(
		const transform_component& tc,
		const motion_component* mc,
		const interpolation_state& interpolation
	) -> transform_component;

	struct [[= system_state<"Physics">{}, = settings::category<"Physics">{}, = deferred_system{}]] data {
		[[= settings::describe<"Step the physics world each frame.">{}]] bool update_phys = true;

		[[
			= settings::describe<"Run the constraint solver on the GPU instead of the CPU. The GPU pipelines and "
									  "buffers are built once during startup, so this requires a restart.">{},
			= settings::restart_required{},
			= shared
		]]
		bool use_gpu_solver = false;

		[[
			= settings::describe<"Most rigid bodies the GPU solver can hold. Sizes the body, contact-adjacency, "
									  "colouring and grounded buffers once at startup and is baked into the solver's "
									  "shaders, so this requires a restart.">{},
			= settings::restart_required{},
			= settings::range<64, 1048576>{}
		]]
		int gpu_max_bodies = 20480;

		[[
			= settings::describe<"Most contacts the GPU solver can carry per tick; contacts past this are dropped "
									  "and counted by the diagnostics. Sizes the contact, warm-start and frozen-Jacobian "
									  "buffers once at startup and is baked into the solver's shaders, so this "
									  "requires a restart.">{},
			= settings::restart_required{},
			= settings::range<1024, 4194304>{}
		]]
		int gpu_max_contacts = 262144;

		[[
			= settings::describe<"Most broad-phase pairs the GPU solver's grid can emit per tick. Sized once at "
									  "startup and baked into the solver's shaders, so this requires a restart.">{},
			= settings::restart_required{},
			= settings::range<1024, 4194304>{}
		]]
		int gpu_max_collision_pairs = 262144;

		[[
			= settings::describe<"Most joints the GPU solver can hold. Sized once at startup and baked into the "
									  "solver's shaders, so this requires a restart.">{},
			= settings::restart_required{},
			= settings::range<16, 1048576>{}
		]]
		int gpu_max_joints = 8192;

		[[
			= settings::describe<"Most jointed islands the GPU solver can sweep serially. Sized once at startup "
									  "and baked into the solver's shaders, so this requires a restart.">{},
			= settings::restart_required{},
			= settings::range<1, 65536>{}
		]]
		int gpu_max_islands = 512;

		[[
			= settings::describe<"Most impulses the GPU solver accepts per upload. Sized once at startup, so this "
									  "requires a restart.">{},
			= settings::restart_required{},
			= settings::range<16, 262144>{}
		]]
		int gpu_max_impulses = 4096;

		[[
			= settings::describe<"Most velocity motors the GPU solver accepts per upload, summed over every tick "
									  "in a replay batch. Sized once at startup, so this requires a restart.">{},
			= settings::restart_required{},
			= settings::range<16, 262144>{}
		]]
		int gpu_max_motors = 4096;

		[[
			= settings::describe<"Cells in the GPU solver's broad-phase hash grid. Sized once at startup and "
									  "baked into the solver's shaders, so this requires a restart.">{},
			= settings::restart_required{},
			= settings::range<1024, 1048576>{}
		]]
		int gpu_grid_table_size = 32768;

		[[
			= settings::describe<"Bodies each rollback ring slot can hold; a scene with more bodies runs with the "
									  "ring off. Sized once at startup, so this requires a restart.">{},
			= settings::restart_required{},
			= settings::range<64, 1048576>{}
		]]
		int gpu_ring_max_bodies = 4096;

		[[
			= settings::describe<"Contacts each rollback ring slot can hold. Sized once at startup, so this "
									  "requires a restart.">{},
			= settings::restart_required{},
			= settings::range<1024, 4194304>{}
		]]
		int gpu_ring_max_contacts = 16384;

		[[
			= settings::describe<"Dispatch at most one GPU solver tick per frame and skip while the previous "
									  "batch is still executing; excess fixed-step demand is dropped, so overload "
									  "dilates the sim instead of multiplying substeps into ever-longer batches. "
									  "Interactive only: the skip decision follows real GPU timing, so leave this "
									  "off for benches and any run that must be hash-comparable.">{}
		]]
		bool gpu_async_dispatch = false;

		[[
			= settings::describe<"Wait for the compute queue before reading the GPU solver's results each tick, "
									  "so simulation consumers observe the previous tick's state instead of a "
									  "snapshot that is max_frames_in_flight frames old. Costs the GPU/CPU overlap; "
									  "for training and benches, not interactive play.">{},
			= settings::restart_required{},
			= shared
		]]
		bool gpu_sync_readback = false;

		[[
			= settings::describe<"Deliver the solver upload on the same-frame channel so the dispatch happens in the "
									  "frame that produced it, instead of a next-frame channel that costs the control loop "
									  "a tick. Implied by gpu_sync_readback.">{},
			= settings::restart_required{},
			= shared
		]]
		bool gpu_same_frame_upload = false;

		[[
			= settings::describe<"Read the GPU solver's full per-body state back to the host every tick, not only the "
									  "slim pose and velocity snapshot the simulation consumes. Debug and parity tools that "
									  "inspect solver internals need it; it multiplies the per-tick readback about five times.">{},
			= settings::restart_required{},
			= shared
		]]
		bool gpu_full_snapshot = false;

		[[
			= settings::describe<"Solve each gpu island with a coloured Gauss-Seidel sweep: bodies that share no "
									  "constraint are solved together, so a sweep takes one round per colour instead of "
									  "one per dependency level. Changes the solve order, so results differ from the "
									  "ordered sweep.">{},
			= shared
		]]
		bool gpu_island_colored_sweep = true;

		[[
			= settings::describe<"Pack several small gpu islands into one solve workgroup and give each lane the next "
									  "body of the current colour step, so a step keeps most lanes busy. Each island keeps "
									  "its colours and solve order, so results are unchanged.">{},
			= shared
		]]
		bool gpu_island_pack = true;

		[[
			= settings::describe<"Fewest gpu islands at which island packing turns on. Packing divides the solve "
									  "workgroup count, so below this count the packed dispatch leaves the device "
									  "underfilled and runs slower than one island per workgroup.">{},
			= settings::range<0, 1048576>{},
			= shared
		]]
		int gpu_island_pack_min_islands = 2048;

		[[
			= settings::describe<"Fold the GPU solver's per-colour Gauss-Seidel dispatches into one sweep dispatch "
									  "per iteration, sequencing colours inside the kernel with a device-scope barrier. "
									  "Bit-identical to the unfolded loop and gate-validated on both backends. The barrier "
									  "spins, so heavy graphics contention can preempt sweep workgroups and starve it; a "
									  "starved sweep bails fast, skips the rest of the batch, and drops the solver back "
									  "to the unfolded loop. The fold retries automatically after a cooldown and also "
									  "re-arms on reseed, world reset, or re-enable of this setting.">{}
		]]
		bool gpu_solve_fold = false;

		[[
			= settings::describe<"Cap the GPU solver's colour palette. Bodies that cannot take a conflict-free "
									  "colour under the cap share one deterministically instead of opening a new "
									  "colour, so the solve runs fewer, fatter colour dispatches with fewer barriers "
									  "per iteration. Conflicting pairs degrade toward undamped Jacobi for that "
									  "iteration and are surfaced by the coloring conflict counter; small caps can "
									  "destabilize stiff piles. 0 keeps the natural colouring; -1 sizes the cap "
									  "automatically from live colour populations, folding tail colours below a "
									  "population floor and backing off when conflicts exceed a budget.">{},
			= settings::range<-1, 16>{}
		]]
		int gpu_color_cap = 0;

		[[
			= settings::describe<"Workgroups per folded sweep dispatch. More workgroups widen the sweep's stride "
									  "and raise occupancy on large scenes, but every workgroup must be co-resident "
									  "for the colour barrier; oversubscribing residency trips the sweep's bail "
									  "fallback. 0 keeps the built-in width of 32.">{},
			= settings::range<0, 256>{}
		]]
		int gpu_sweep_workgroups = 0;

		[[
			= settings::describe<"Constraint solver iterations per substep. With adaptive_solver_iterations off "
									  "both solvers run exactly this many every substep; with it on this is the cap "
									  "and either solver may stop as early as the first iteration. Higher values "
									  "reduce jitter at the cost of frame time.">{},
			= settings::range<1, 64>{},
			= shared
		]]
		int solver_iterations = 15;

		[[
			= settings::describe<"Stop iterating as soon as the worst violation falls below the convergence "
									  "thresholds or stops shrinking, checked from the first iteration with no "
									  "lower bound. Off runs a fixed solver_iterations every substep.">{},
			= shared
		]]
		bool adaptive_solver_iterations = false;

		[[
			= settings::describe<"Worst contact violation below which adaptive_solver_iterations stops the "
									  "loop.">{}
		]]
		length convergence_threshold_linear = meters(1e-4f);

		[[
			= settings::describe<"Worst joint angular violation below which adaptive_solver_iterations stops the "
									  "loop.">{}
		]]
		angle convergence_threshold_angular = radians(1e-3f);

		[[
			= settings::describe<"Scale the linear convergence threshold with scene motion: the effective "
									  "threshold becomes the larger of convergence_threshold_linear and this fraction "
									  "of the fastest dynamic body's travel per substep. Churning scenes stop "
									  "iterating once residual error is invisible against their own motion while "
									  "settling stacks keep the tight threshold and the full adaptive budget. Applies "
									  "to both solvers identically; 0 disables.">{},
			= settings::range<0.f, 0.25f>{},
			= shared
		]]
		float convergence_speed_scale = 0.f;

		[[
			= settings::describe<"Fraction of each contact's pre-existing penetration carried forward per substep; "
									  "the remainder is corrected as position error. Lower values depenetrate faster "
									  "but inject more energy.">{},
			= settings::range<0.f, 1.f>{}
		]]
		float solver_alpha = 0.99f;

		[[
			= settings::describe<"Penalty ramp rate. Each dual update grows a violated contact row's penalty by "
									  "this stiffness per meter of violation.">{}
		]]
		stiffness_per_length solver_beta = newtons_per_meter_squared(100000.f);

		[[
			= settings::describe<"Per-substep decay factor for contact penalties and warm-started duals.">{},
			= settings::range<0.5f, 1.f>{}
		]]
		float solver_gamma = 0.99f;

		[[
			= settings::describe<"Base penalty stiffness for inactive contact rows. Active rows floor at the "
									  "mass-scaled value instead.">{}
		]]
		stiffness penalty_min = newtons_per_meter(1.f);

		[[
			= settings::describe<"Upper bound on contact penalty stiffness.">{}
		]]
		stiffness penalty_max = newtons_per_meter(1e9f);

		[[
			= settings::describe<"Contact offset added to every separation before the solver sees it. Bodies come "
									  "to rest with this gap.">{}
		]]
		gap collision_margin = meters(0.0005f);

		[[
			= settings::describe<"Grip envelope. Contacts within this distance keep their friction anchors and "
									  "warm-start stiffness memory.">{}
		]]
		gap stick_threshold = meters(0.01f);

		[[
			= settings::describe<"Linear speed below which a body may begin falling asleep. Zero disables sleeping "
									  "entirely, which is what a capture run wants — a sleeping island is not woken by "
									  "having its support removed.">{}
		]]
		velocity velocity_sleep_threshold = meters_per_second(0.05f);

		[[
			= settings::describe<"Angular speed below which a body may begin falling asleep. Paired with "
									  "velocity_sleep_threshold; both must be satisfied.">{}
		]]
		angular_velocity angular_sleep_threshold = radians_per_second(0.05f);

		[[
			= settings::describe<"Maximum distance at which approaching pairs get speculative contacts. Each "
									  "pair's actual window scales with its relative speed, down to "
									  "speculative_margin_floor for calm pairs.">{}
		]]
		gap speculative_margin = meters(0.02f);

		[[
			= settings::describe<"Minimum speculative contact window. Calm pairs use this window; it must stay "
									  "wider than settling pairs' gap oscillation or the contact set churns at the "
									  "boundary.">{}
		]]
		gap speculative_margin_floor = meters(0.01f);

		[[
			= settings::describe<"Use Jacobi iteration instead of Gauss-Seidel. More parallel-friendly but converges slower.">{},
			= shared
		]]
		bool use_jacobi = false;

		[[
			= settings::describe<"Record the gpu solver's per-stage diagnostic hash dispatches every tick so ContactTrace "
									  "can print the pass-hash line. Off keeps the dispatch stream lean; the solve itself is "
									  "identical either way.">{},
			= shared
		]]
		bool trace_hashes = false;

		[[
			= settings::describe<"Log the age in ticks of the gpu body snapshot the solver served each step. The control "
									  "loop is act -> dispatch -> readback -> observe, so this age is the loop length the "
									  "policy actually faces, and one extra tick of it costs most of a locomotion policy.">{},
			= shared
		]]
		bool trace_readback_age = false;

		[[
			= settings::describe<"Count, for every gpu island solve iteration, how many islands already meet the "
									  "adaptive convergence tolerance on their own constraints, and log the running "
									  "histogram. Measures what a per-island early-out could skip; the solve itself runs "
									  "every iteration either way.">{},
			= shared
		]]
		bool trace_island_convergence = false;

		[[
			= settings::describe<"Ticks of gpu island convergence counts summed into each trace_island_convergence line.">{},
			= settings::range<1, 100000>{}
		]]
		int trace_island_convergence_ticks = 600;

		[[
			= settings::describe<"Relaxation factor for the Jacobi solver. Lower values are more stable; "
									  "higher values converge faster.">{},
			= settings::range<0.1f, 1.0f>{},
			= shared
		]]
		float jacobi_omega = 0.67f;

		[[
			= settings::describe<"Solve bodies serially in height order, alternating sweep direction each "
									  "iteration, instead of the parallel colour sweep. Trades parallelism for "
									  "convergence on tall stacks.">{},
			= shared
		]]
		bool use_ordered_sweep = false;

		[[
			= settings::describe<"Number of substeps per simulation tick. More substeps improve stability for "
									  "fast-moving bodies.">{},
			= settings::range<1, 8>{},
			= shared
		]]
		int physics_substeps = 2;

		[[
			= settings::describe<"Bodies per parallel chunk in the constraint solver colour sweep. Lower values "
									  "spread the sweep across more threads at the cost of scheduling overhead.">{},
			= settings::range<1, 256>{}
		]]
		int color_chunk_grain = 8;

		[[
			= settings::describe<"Parallel chunks per worker in the broad phase. The pair test does more work for "
									  "early objects than late ones, so higher values balance the load at the cost of "
									  "scheduling overhead.">{},
			= settings::range<1, 32>{}
		]]
		int broad_phase_chunks_per_worker = 8;

		[[
			= settings::describe<"Fixed steps of physics history kept for rollback. Zero keeps no history; the "
									  "netcode and the rollback parity scenarios set it.">{},
			= settings::range<0, 600>{}
		]]
		int rollback_history_steps = 0;

		[[
			= settings::describe<"Solver body index of one body whose contacts both solvers trace per iteration: "
									  "the cpu solver logs each contact's violation, dual force and penalty before "
									  "every dual update, and the gpu solver records the same for its first substep "
									  "into the collision state header, printed by ContactTrace. An index rather than "
									  "an owner id so the trace covers the very first tick, before the body map exists; "
									  "-1 traces nothing.">{},
			= settings::range<-1, 5119>{},
			= shared
		]]
		int trace_body = -1;

		bool gpu_unavailable_reported = false;
		int rollback_history_floor = 0;
		[[= shared]] std::uint64_t step_index = 0;
		[[= shared]] std::uint64_t observed_step = 0;
		[[= shared]] std::vector<step_snapshot> rollback_ring;
		[[= shared]] id_mapped_collection<joint_definition> joints;
		bool joint_rest_orientations_pending = true;
		std::vector<id> results_ensured_owners;
		std::vector<std::uint32_t> drive_joint_slots;
		[[= shared]] std::uint64_t joints_generation = 1;
		[[= shared]] std::uint64_t joint_inputs_generation = 1;
		[[= shared]] std::vector<convex_hull> hulls;

		[[= shared]] vbd::solver vbd_solver;
		vbd::contact_cache contact_cache;
		[[= shared]] sleep_counter_table sleep_counters;
		[[= shared]] std::flat_map<id, std::uint32_t> id_to_body_index;
		std::vector<std::pair<id, std::uint32_t>> id_to_body_index_entries;
		std::flat_map<id, transform_component> kinematic_step_start;
		std::vector<impulse_request> gpu_pending_impulses;
		std::unordered_map<id, std::uint64_t> gpu_reset_ticks;
		[[= shared]] gpu_tick_plan gpu_plan;
		[[= shared]] interpolation_state interpolation;
		bool gpu_sweep_fold_bailed = false;
		bool gpu_solve_fold_prev = false;
		int gpu_sweep_retry_cooldown = 0;
		int gpu_sweep_retry_backoff = 0;
		int gpu_sweep_calm_frames = 0;
		int gpu_color_cap_resolved = 0;
		int gpu_color_cap_dwell = 0;
		int gpu_color_cap_min = 0;
		std::array<std::uint64_t, vbd::limits.island_convergence_count> island_convergence_totals{};
		std::uint64_t island_convergence_generation = 0;
		int island_convergence_samples = 0;

		[[= shared]] std::vector<std::uint8_t> body_airborne;
		[[= shared]] std::vector<std::uint8_t> body_sleeping;
		std::vector<std::uint32_t> gpu_grounded_bits;

		[[= shared]] vbd::gpu_solver gpu_solver;
	};

	namespace gpu_upload {
		struct upload_scratch {
			std::vector<vbd::body_state> bodies;
			std::vector<vbd::body_scan_entry> body_scan;
			std::vector<mass_properties> body_props;
			std::vector<vbd::velocity_motor_constraint> motors;
			std::vector<vbd::joint_constraint> joints;
			std::vector<vbd::joint_drive_input> joint_inputs;
			std::vector<vbd::impulse_constraint> impulses;
		};

		struct [[= system_state<"Physics GPU Upload">{}, = deferred_system{}]] data {
			std::uint64_t built_generation = 0;
			std::uint64_t uploaded_joints_generation = 0;
			std::uint64_t uploaded_joint_inputs_generation = 0;
			std::uint32_t uploaded_body_count = 0;
			std::uint32_t uploaded_joint_count = 0;
			bool force_full_joints = false;
			std::vector<joint_definition> joints;
			std::vector<std::uint32_t> joint_slots;
			std::vector<std::uint32_t> joint_drive_slots;
			std::size_t joint_drive_count = 0;
			[[= shared]] std::flat_map<id, std::uint32_t> joint_gpu_slots;
			[[= shared]] std::uint64_t joint_gpu_slots_generation = 0;
			std::vector<std::pair<id, std::uint32_t>> joint_body_index_entries;
			std::flat_map<id, std::uint32_t> body_index;
			std::vector<std::pair<id, std::uint32_t>> body_index_entries;
			std::vector<std::uint32_t> motor_body_slots;
			std::vector<std::uint32_t> wake_body_slots;
			std::array<upload_scratch, 3> scratch;
			std::size_t scratch_slot = 0;
		};

		[[= system_run<>{}]]
		auto run(
			data& d,
			shared_view<physics::data> phys,
			channel_write<gpu_upload_report, gpu_upload_payload, vbd::solver_upload> out,
			read<transform_component> transform,
			read<motion_component> motion,
			read<collision_component> collision,
			read<motor_component> motor,
			read<joint_drive_component> drives,
			read<muscle_component> muscles
		) -> async::task<>;
	}

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

	auto capacities_from_settings(
		const data& d
	) -> vbd::vbd_capacities;

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

	struct body_scan_fill {
		std::span<vbd::body_scan_entry> entries;
		bool sparse = false;

		auto active(std::size_t body_count) const -> bool;
		auto filled(std::size_t body_index) const -> bool;
	};

	auto build_body_states(
		const body_build_view& view,
		const sleep_counter_table& sleep_counters,
		std::vector<vbd::body_state>& bodies,
		std::flat_map<id, std::uint32_t>& id_to_body_index,
		std::vector<std::pair<id, std::uint32_t>>& id_to_body_index_entries,
		std::vector<std::uint8_t>& has_transform,
		const body_scan_fill& scan = {}
	) -> void;

	auto build_body_bounds(
		const body_build_view& view,
		const std::flat_map<id, std::uint32_t>& id_to_body_index,
		std::span<const std::uint8_t> has_transform,
		std::span<vbd::body_state> bodies,
		const body_scan_fill& scan = {}
	) -> void;

	auto build_joint_constraints(
		std::span<joint_definition> definitions,
		const std::flat_map<id, std::uint32_t>& id_to_body_index,
		std::span<const vbd::body_state> bodies,
		std::vector<vbd::joint_constraint>& out,
		std::vector<std::uint32_t>* definition_slots = nullptr
	) -> void;

	struct body_slot_cache {
		std::span<const id> body_owners;
		std::span<const std::uint8_t> has_transform;
		std::span<std::uint32_t> slots;
	};

	auto resolve_body_slot(
		const std::flat_map<id, std::uint32_t>& id_to_body_index,
		const body_slot_cache& cache,
		std::size_t i,
		id owner
	) -> std::optional<std::uint32_t>;

	auto build_motor_constraints(
		std::span<const motor_input> motors,
		const std::flat_map<id, std::uint32_t>& id_to_body_index,
		std::span<const std::uint8_t> body_airborne,
		std::span<vbd::body_state> bodies,
		std::vector<vbd::velocity_motor_constraint>& out,
		const body_slot_cache& cache = {},
		const body_scan_fill& scan = {}
	) -> void;

	auto apply_joint_drive(
		joint_definition& jd,
		const joint_drive_component& drive
	) -> void;

	auto apply_muscle_activation(
		joint_definition& jd,
		const muscle_component& muscle
	) -> bool;

	auto copy_joints_with_inputs(
		shared_view<data> phys,
		read<joint_drive_component>& drives,
		read<muscle_component>& muscles,
		std::vector<joint_definition>& out
	) -> void;

	[[= system_init{}]]
	auto init(
		context& ctx,
		std::optional<shared_view<gpu::context::data>> gpu_s,
		data& d
	) -> async::task<>;

	[[= system_run<>{}, = runs_after_optional<^^gpu::context::data>{}]]
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

	[[= system_run<1>{}]]
	auto ensure_results(
		context& ctx,
		data& d,
		write<collision_component> collision,
		structural<collision_result_component> results
	) -> async::task<>;

	[[= system_run<2>{}]]
	auto integrate(
		context& ctx,
		data& d,
		channel_read<impulse_request, reset_physics_request, rollback_request, rollback_history_request, gpu_upload_report> requests_in,
		channel_write<gpu_solver_frame_info> solver_out,
		write<transform_component> transform,
		write<motion_component> motion,
		read<motor_component> motor,
		write<collision_component> collision,
		write<collision_result_component> results,
		write<hull_definition> hull_definitions
	) -> async::task<>;

	[[= system_frame{}]]
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

	template <access_mode Transform, access_mode Motion>
	auto gather_step_inputs(
		read<motor_component>& motor,
		access<transform_component, Transform>& transform,
		access<motion_component, Motion>& motion,
		std::span<const impulse_request> impulses
	) -> step_inputs;

	auto apply_step_inputs(
		const step_inputs& inputs,
		data& d,
		write<transform_component>& transform,
		write<motion_component>& motion
	) -> void;

	auto step_all_asleep(
		const step_inputs& inputs,
		const data& d,
		write<motion_component>& motion
	) -> bool;

	[[nodiscard]] auto history_steps(
		const data& d
	) -> std::size_t;

	auto snapshot_step(
		data& d,
		write<transform_component>& transform,
		write<motion_component>& motion,
		const step_inputs& inputs
	) -> void;

	auto restore_step(
		data& d,
		const step_snapshot& snap,
		write<transform_component>& transform,
		write<motion_component>& motion
	) -> void;

	auto rollback(
		data& d,
		const rollback_request& request,
		write<transform_component>& transform,
		write<motion_component>& motion,
		write<collision_component>& collision,
		write<collision_result_component>& results
	) -> void;

	auto update_vbd(
		int steps,
		data& d,
		write<transform_component>& transform,
		write<motion_component>& motion,
		write<collision_component>& collision,
		write<collision_result_component>& results,
		std::span<const step_inputs> per_step
	) -> void;

	auto update_vbd_gpu(
		int steps,
		data& d,
		write<transform_component>& transform,
		write<motion_component>& motion,
		write<collision_result_component>& results,
		const step_inputs& inputs,
		std::span<const rollback_request> rollbacks,
		std::span<const gpu_upload_report> reports,
		channel_write<gpu_solver_frame_info> frame_info_out,
		bool reset
	) -> void;
}

template <gse::access_mode Transform, gse::access_mode Motion>
auto gse::physics::gather_step_inputs(read<motor_component>& motor, access<transform_component, Transform>& transform, access<motion_component, Motion>& motion, const std::span<const impulse_request> impulses) -> step_inputs {
	step_inputs inputs;

	const auto motor_ids = motor.owner_ids();
	inputs.motors.reserve(motor.size());
	for (std::size_t i = 0; i < motor.size(); ++i) {
		inputs.motors.push_back({
			.owner = motor_ids[i],
			.motor = motor[i],
		});
	}

	const auto motion_ids = motion.owner_ids();
	for (std::size_t i = 0; i < motion.size(); ++i) {
		const auto& mc = motion[i];
		if (!is_kinematic(mc)) {
			continue;
		}
		const auto* tc = transform.find(motion_ids[i]);
		if (!tc) {
			continue;
		}
		inputs.kinematics.push_back({
			.owner = motion_ids[i],
			.start = *tc,
			.velocity = mc.current_velocity,
			.angular_velocity = mc.angular_velocity,
		});
	}

	inputs.impulses.assign(impulses.begin(), impulses.end());
	return inputs;
}