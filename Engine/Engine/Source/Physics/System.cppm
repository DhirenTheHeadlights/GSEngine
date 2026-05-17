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

	struct gpu_upload_payload {
		std::vector<vbd::body_state> bodies;
		std::vector<vbd::velocity_motor_constraint> motors;
		std::vector<vbd::joint_constraint> joints;
		std::vector<vbd::impulse_constraint> impulses;
		vbd::solver_config solver_cfg;
		time_t<float, seconds> dt{};
		int steps = 1;
		bool refresh_joints = false;
	};

	struct gpu_body_index_map {
		std::vector<std::pair<id, std::uint32_t>> entries;
	};

	struct gpu_solver_stats {
		bool active = false;
		std::uint32_t motor_count = 0;
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

	class system {
	public:
		struct data {
			static constexpr std::string_view category = "Physics";

			[[=gse::settings::describe<"Step the physics world each frame.">{}]]
			bool update_phys = true;

			[[=gse::settings::describe<"Run the constraint solver on the GPU instead of the CPU.">{}]]
			bool use_gpu_solver = false;

			[[=gse::settings::describe<"Number of constraint solver iterations per substep. Higher values reduce jitter at the cost of frame time.">{}, =gse::settings::range<1, 40>{}]]
			int solver_iterations = 15;

			[[=gse::settings::describe<"Use Jacobi iteration instead of Gauss-Seidel. More parallel-friendly but converges slower.">{}]]
			bool use_jacobi = false;

			[[=gse::settings::describe<"Relaxation factor for the Jacobi solver. Lower values are more stable; higher values converge faster.">{}, =gse::settings::range<0.1f, 1.0f>{}]]
			float jacobi_omega = 0.67f;

			[[=gse::settings::describe<"Number of substeps per simulation tick. More substeps improve stability for fast-moving bodies.">{}, =gse::settings::range<1, 8>{}]]
			int physics_substeps = 2;

			bool gpu_buffers_created = false;
			gpu_solver_stats gpu_stats;
			std::vector<joint_definition> joints;

			vbd::solver vbd_solver;
			vbd::contact_cache contact_cache;
			std::unordered_map<id, std::uint32_t> sleep_counters;
			bool gpu_joints_dirty = true;
			std::uint32_t gpu_uploaded_body_count = 0;
			std::uint32_t gpu_uploaded_joint_count = 0;
			flat_map<id, std::uint32_t> id_to_body_index;

			std::vector<std::uint8_t> body_airborne;
			std::vector<std::uint8_t> body_sleeping;

			vbd::gpu_solver gpu_solver;
		};

		static auto run(
			run_context& ctx,
			const gpu::context::data* gpu_s,
			const asset::data& assets_s,
			data& d
		) -> async::task<>;

		static auto frame(
			frame_context& ctx,
			const gpu::context::data* gpu_s,
			data& d
		) -> async::task<>;

		static auto create_joint(
			data& d,
			const joint_definition& def
		) -> joint_handle;

		static auto remove_joint(
			data& d,
			joint_handle handle
		) -> void;

		static auto query_transform(
			const data& d,
			id entity_id
		) -> std::optional<transform_snapshot>;

		static auto is_airborne(
			const data& d,
			id entity_id
		) -> bool;

		static auto is_sleeping(
			const data& d,
			id entity_id
		) -> bool;

	private:
		struct collision_pair {
			id owner;
			aabb box;
		};

		static auto collect_collision_objects(
			write<transform_component>& transform,
			write<collision_component>& collision
		) -> std::vector<collision_pair>;

		static auto add_scene_contacts_to_solver(
			vbd::solver& solver,
			vbd::contact_cache& contact_cache,
			std::vector<collision_pair>& objects,
			const flat_map<id, std::uint32_t>& id_to_body_index,
			bool update_scene_state,
			write<transform_component>& transform,
			write<motion_component>& motion,
			write<collision_component>& collision,
			write<collision_result_component>* results,
			std::span<std::uint8_t> body_airborne
		) -> void;

		static auto update_vbd(
			int steps,
			data& d,
			write<transform_component>& transform,
			write<motion_component>& motion,
			read<motor_component>& motor,
			write<collision_component>& collision,
			write<collision_result_component>& results,
			std::span<const impulse_request> impulses
		) -> void;

		static auto update_vbd_gpu(
			int steps,
			data& d,
			write<transform_component>& transform,
			write<motion_component>& motion,
			read<motor_component>& motor,
			write<collision_component>& collision,
			write<collision_result_component>& results,
			std::span<const impulse_request> impulses,
			time_t<float, seconds> dt,
			channel_writer& channels
		) -> void;
	};
}
