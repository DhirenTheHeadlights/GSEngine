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

import gse.math;
import gse.meta;
import gse.gpu;

import :narrow_phase_collision;
import :motion_component;
import :collision_component;
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

	struct state {
		bool update_phys = true;
		bool use_gpu_solver = false;
		bool gpu_buffers_created = false;
		int solver_iterations = 15;
		bool compare_solvers = false;
		bool use_jacobi = false;
		float jacobi_omega = 0.67f;
		int physics_substeps = 2;
		gpu_solver_stats gpu_stats;
		std::vector<joint_definition> joints;
	};

	auto create_joint(
		state& s,
		const joint_definition& def
	) -> joint_handle;

	auto remove_joint(
		state& s,
		joint_handle handle
	) -> void;

	struct system {
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

		static auto initialize(
			const init_context& phase,
			update_data& ud,
			frame_data& fd,
			state& s
		) -> void;

		static auto update(
			update_context& ctx,
			update_data& ud,
			state& s
		) -> async::task<>;

		static auto frame(
			frame_context& ctx,
			frame_data& fd,
			const state& s
		) -> async::task<>;
	};
}

namespace gse::physics {
	struct collision_pair {
		collision_component* collision;
		motion_component* motion;
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

	auto collect_collision_objects(
		write<motion_component>& motion,
		write<collision_component>& collision
	) -> std::vector<collision_pair>;

	auto add_scene_contacts_to_solver(
		vbd::solver& solver,
		vbd::contact_cache& contact_cache,
		const std::vector<collision_pair>& objects,
		const flat_map<id, std::uint32_t>& id_to_body_index,
		bool update_scene_state
	) -> void;

	auto pack_feature(
		const feature_id& feature
	) -> std::uint64_t;

	auto unpack_feature(
		std::uint64_t packed
	) -> feature_id;

	auto build_contact_cache_from_warm_start(
		const std::span<const vbd::warm_start_entry> warm_start_contacts
	) -> vbd::contact_cache;

	auto invalidate_warm_start_entries(
		std::vector<vbd::warm_start_entry>& warm_start_contacts,
		const std::span<const std::uint32_t> body_indices
	) -> void;

	auto update_vbd(
		int steps,
		system::update_data& ud,
		state& s,
		write<motion_component>& motion,
		write<collision_component>& collision
	) -> void;

	auto update_vbd_gpu(
		int steps,
		system::update_data& ud,
		state& s,
		write<motion_component>& motion,
		write<collision_component>& collision,
		time_t<float, seconds> dt,
		channel_writer& channels
	) -> void;
}
