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
import gse.assets;

import gse.math;
import gse.meta;
import gse.gpu;

import :narrow_phase_collision;
import :motion_component;
import :motion_status_component;
import :motor_component;
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
	};

	using joint_handle = std::uint32_t;

	struct joint_request {
		joint_definition def;
	};

	struct fixed_joint {
		vec3<displacement> anchor_a;
		vec3<displacement> anchor_b;
	};

	struct distance_joint {
		length target;
	};

	struct hinge_joint {
		vec3<displacement> anchor_a;
		vec3<displacement> anchor_b;
		vec3f axis = { 0.f, 1.f, 0.f };
		std::optional<std::pair<angle, angle>> limits;
	};

	struct slider_joint {
		vec3f axis = { 0.f, 1.f, 0.f };
	};

	struct spring_joint {
		length target;
		inverse_mass compliance = per_kilograms(0.01f);
		float damping = 0.5f;
	};

	auto join(
		channel_writer& channels,
		id a,
		id b,
		const fixed_joint& config
	) -> void;

	auto join(
		channel_writer& channels,
		id a,
		id b,
		const distance_joint& config
	) -> void;

	auto join(
		channel_writer& channels,
		id a,
		id b,
		const hinge_joint& config
	) -> void;

	auto join(
		channel_writer& channels,
		id a,
		id b,
		const slider_joint& config
	) -> void;

	auto join(
		channel_writer& channels,
		id a,
		id b,
		const spring_joint& config
	) -> void;

	struct gpu_upload_payload {
		std::vector<vbd::body_state> bodies;
		std::vector<vbd::collision_body_data> collision_data;
		std::vector<float> accel_weights;
		std::vector<vbd::velocity_motor_constraint> motors;
		std::vector<vbd::joint_constraint> joints;
		std::vector<vbd::warm_start_entry> warm_starts;
		std::vector<std::uint32_t> authoritative_body_indices;
		vbd::solver_config solver_cfg;
		time_t<float, seconds> dt{};
		int steps = 1;
		std::vector<id> entity_ids;
		std::uint32_t joint_count = 0;
	};

	struct gpu_body_index_map {
		std::vector<std::pair<id, std::uint32_t>> entries;
	};

	struct gpu_readback_result {
		std::vector<id> entity_ids;
		std::vector<vbd::body_state> gpu_input_bodies;
		std::vector<vbd::body_state> gpu_result_bodies;
		std::vector<vbd::contact_readback_entry> gpu_contacts;
		std::vector<vbd::joint_constraint> gpu_joint_readback;
		std::uint32_t gpu_joint_count = 0;
	};

	struct gpu_solver_stats {
		bool active = false;
		std::uint32_t contact_count = 0;
		std::uint32_t motor_count = 0;
		time_t<float, seconds> solve_time{};
	};

	struct gpu_solver_frame_info {
		const gpu::buffer* snapshot = nullptr;
		gpu::compute_semaphore_state semaphore{};
		std::uint32_t body_count = 0;
		std::uint32_t body_stride = 0;
		std::uint32_t position_offset = 0;
	};

	class system {
	public:
		struct settings {
			static constexpr std::string_view category = "Physics";

			[[=gse::settings::describe<"Step the physics world each frame.">{}]]
			bool update_phys = true;

			[[=gse::settings::describe<"Run the constraint solver on the GPU instead of the CPU.">{}]]
			bool use_gpu_solver = false;

			[[=gse::settings::describe<"Number of constraint solver iterations per substep. Higher values reduce jitter at the cost of frame time.">{}, =gse::settings::range<1, 40>{}]]
			int solver_iterations = 15;

			[[=gse::settings::describe<"Run the CPU and GPU solvers side by side to validate parity. Disable in shipping builds.">{}]]
			bool compare_solvers = false;

			[[=gse::settings::describe<"Use Jacobi iteration instead of Gauss-Seidel. More parallel-friendly but converges slower.">{}]]
			bool use_jacobi = false;

			[[=gse::settings::describe<"Relaxation factor for the Jacobi solver. Lower values are more stable; higher values converge faster.">{}, =gse::settings::range<0.1f, 1.0f>{}]]
			float jacobi_omega = 0.67f;

			[[=gse::settings::describe<"Number of substeps per simulation tick. More substeps improve stability for fast-moving bodies.">{}, =gse::settings::range<1, 8>{}]]
			int physics_substeps = 2;
		};

		struct state {
			bool gpu_buffers_created = false;
			gpu_solver_stats gpu_stats;
			std::vector<joint_definition> joints;
		};

		struct update_data {
			time_t<float, seconds> accumulator{};
			clock tick_clock;
			bool tick_clock_primed = false;
			vbd::solver vbd_solver;
			vbd::contact_cache contact_cache;
			std::unordered_map<id, std::uint32_t> sleep_counters;
			interval_timer<> comparison_timer{ seconds(0.25f) };
			struct solver_comparison_snapshot {
				std::vector<vbd::body_state> cpu_result;
				std::vector<vbd::contact_constraint> cpu_contacts;
				std::vector<vbd::joint_constraint> cpu_joints;
			};
			std::optional<solver_comparison_snapshot> comparison_pending;
			struct gpu_prev_frame {
				std::vector<vbd::body_state> result_bodies;
				std::vector<id> result_entity_ids;
				std::vector<vbd::warm_start_entry> warm_start_contacts;

				gpu_prev_frame() {
					result_bodies.reserve(vbd::max_bodies);
					result_entity_ids.reserve(vbd::max_bodies);
					warm_start_contacts.reserve(vbd::max_contacts);
				}
			} gpu_prev;
			std::optional<gpu_readback_result> completed_readback;
		};

		struct frame_data {
			vbd::gpu_solver gpu_solver;
			struct readback_frame {
				std::vector<id> entity_ids;
				std::vector<vbd::body_state> gpu_input_bodies;
				std::uint32_t gpu_joint_count = 0;
			};
			std::optional<readback_frame> in_flight;
		};

		static auto run(
			run_context& ctx,
			const gpu::context::state* gpu_s,
			const asset::state& assets_s,
			settings& cfg,
			update_data& ud,
			frame_data& fd,
			state& s
		) -> async::task<>;

		static auto frame(
			frame_context& ctx,
			const gpu::context::state* gpu_s,
			const settings& cfg,
			frame_data& fd,
			const state& s
		) -> async::task<>;

		static auto create_joint(
			state& s,
			const joint_definition& def
		) -> joint_handle;

		static auto remove_joint(
			state& s,
			joint_handle handle
		) -> void;

	private:
		struct collision_pair {
			id owner;
		};

		struct contact_compare_key {
			std::uint32_t body_a = 0;
			std::uint32_t body_b = 0;
			std::uint64_t feature_key = 0;

			auto operator==(
				const contact_compare_key&
			) const -> bool = default;
		};

		struct contact_compare_key_hash {
			auto operator()(
				const contact_compare_key& key
			) const noexcept -> std::size_t;
		};

		static auto collect_collision_objects(
			write<transform_component>& transform,
			write<collision_component>& collision
		) -> std::vector<collision_pair>;

		static auto add_scene_contacts_to_solver(
			vbd::solver& solver,
			vbd::contact_cache& contact_cache,
			const std::vector<collision_pair>& objects,
			const flat_map<id, std::uint32_t>& id_to_body_index,
			bool update_scene_state,
			write<transform_component>& transform,
			write<motion_component>& motion,
			write<collision_component>& collision,
			write<collision_result_component>* results,
			write<motion_status_component>* status
		) -> void;

		static auto pack_feature(
			const feature_id& feature
		) -> std::uint64_t;

		static auto unpack_feature(
			std::uint64_t packed
		) -> feature_id;

		static auto build_contact_cache_from_warm_start(
			const std::span<const vbd::warm_start_entry> warm_start_contacts
		) -> vbd::contact_cache;

		static auto invalidate_warm_start_entries(
			std::vector<vbd::warm_start_entry>& warm_start_contacts,
			const std::span<const std::uint32_t> body_indices
		) -> void;

		static auto update_vbd(
			int steps,
			const settings& cfg,
			update_data& ud,
			state& s,
			write<transform_component>& transform,
			write<motion_component>& motion,
			write<motion_status_component>& status,
			read<motor_component>& motor,
			write<collision_component>& collision,
			write<collision_result_component>& results,
			std::span<const impulse_request> impulses
		) -> void;

		static auto update_vbd_gpu(
			int steps,
			const settings& cfg,
			update_data& ud,
			state& s,
			write<transform_component>& transform,
			write<motion_component>& motion,
			write<motion_status_component>& status,
			read<motor_component>& motor,
			write<collision_component>& collision,
			write<collision_result_component>& results,
			std::span<const impulse_request> impulses,
			time_t<float, seconds> dt,
			channel_writer& channels
		) -> void;
	};
}
