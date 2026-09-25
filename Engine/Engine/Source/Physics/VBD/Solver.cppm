export module gse.physics:vbd_solver;

import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.log;
import gse.math;
import std;

import :contact_manifold;
import :vbd_constraint_graph;
import :vbd_constraints;
import :vbd_contact_cache;
import :motion_component;

export namespace gse::vbd {
	using time_step = time_t<float, seconds>;

	struct step_delta {
		length linear{};
		angle angular{};
	};

	struct [[= shaders::shader_struct]] solver_config {
		std::uint32_t iterations = 15;
		float alpha = 0.99f;
		stiffness_per_length beta = newtons_per_meter_squared(100000.f);
		angular_stiffness_per_angle ang_beta = newton_meters_per_radian_squared(100000.f);
		float gamma = 0.99f;
		std::uint32_t post_stabilize = 1;
		stiffness penalty_min = newtons_per_meter(1.0f);
		stiffness penalty_max = newtons_per_meter(1e9f);
		angular_stiffness ang_penalty_min = newton_meters_per_radian(1.0f);
		angular_stiffness ang_penalty_max = newton_meters_per_radian(1e9f);
		gap collision_margin = meters(0.0005f);
		gap stick_threshold = meters(0.01f);
		float friction_coefficient = 0.6f;
		velocity restitution_threshold = meters_per_second(0.5f);
		velocity velocity_sleep_threshold = meters_per_second(0.05f);
		angular_velocity angular_sleep_threshold = radians_per_second(0.05f);
		gap speculative_margin = meters(0.02f);
		gap speculative_margin_floor = meters(0.01f);
		length convergence_threshold_linear = meters(1e-4f);
		angle convergence_threshold_angular = radians(1e-3f);
		float convergence_speed_scale = 0.f;
		std::uint32_t min_iterations = 4;
		std::uint32_t adaptive = 0;
		float linear_damping = 0.f;
		float angular_damping = 0.01f;
		std::uint32_t use_jacobi = 0;
		float jacobi_omega = 0.67f;
		velocity max_linear_speed = meters_per_second(15.f);
		angular_velocity max_angular_speed = radians_per_second(50.f);
		length max_linear_step = meters(0.1f);
		angle max_angular_step = radians(0.5f);
		stiffness linear_regularization = newtons_per_meter(1e-6f);
		angular_stiffness angular_regularization = newton_meters_per_radian(1e-6f);
		std::uint32_t trace_hashes = 0;
		std::uint32_t use_solve_fold = 0;
		std::uint32_t color_cap = 0;
		std::uint32_t sweep_workgroups = 0;
		std::uint32_t trace_body = 0xFFFFFFFFu;
		std::uint32_t trace_island_convergence = 0;
		std::uint32_t island_colored_sweep = 0;
		std::uint32_t island_pack = 0;
		std::uint32_t islands_per_workgroup = 1;
	};

	class solver {
	public:
		auto configure(
			const solver_config& cfg
		) -> void;

		auto config() const -> const solver_config&;

		auto set_color_grain(
			std::size_t grain
		) -> void;

		auto set_ordered_sweep(
			bool enabled
		) -> void;

		auto begin_frame(
			std::span<const body_state> bodies,
			std::span<const id> body_ids,
			contact_cache& cache
		) -> void;

		auto seed_previous_velocities(
			std::span<const vec3<velocity>> velocities
		) -> void;

		auto seed_carried(
			std::span<const vec3<velocity>> velocities,
			std::span<const float> weights
		) -> void;

		auto previous_velocities() const -> std::span<const vec3<velocity>>;

		auto accel_weights() const -> std::span<const float>;

		auto convergence_counts() const -> std::span<const std::uint32_t>;

		auto add_contact_constraint(
			const contact_constraint& c
		) -> void;

		auto add_motor_constraint(
			const velocity_motor_constraint& m
		) -> void;

		auto add_joint_constraint(
			const joint_constraint& j
		) -> void;

		auto solve(
			time_step dt
		) -> void;

		auto end_frame(
			std::vector<body_state>& bodies,
			contact_cache& cache
		) -> void;

		auto body_states(
			this auto&& self
		) -> decltype(auto);

		auto body_ids(
			this auto&& self
		) -> decltype(auto);

		auto graph(
			this auto&& self
		) -> auto&;

	private:
		auto accumulate_contact(
			const contact_constraint& constraint,
			const frozen_jacobian& frozen,
			std::uint32_t body_idx,
			time_squared h_squared,
			float alpha
		) -> void;

		auto accumulate_motor(
			const velocity_motor_constraint& m,
			time_squared h_squared
		) -> void;

		auto step_colored_body(
			std::uint32_t body_idx,
			time_squared h_squared,
			time_step dt,
			float alpha
		) -> void;

		auto perform_newton_step(
			std::uint32_t body_idx,
			time_squared h_squared
		) -> step_delta;

		auto accumulate_joint(
			const joint_constraint& constraint,
			std::uint32_t body_idx,
			time_squared h_squared,
			time_step dt,
			float alpha
		) -> void;

		auto accumulate_joint_drive(
			const joint_constraint& constraint,
			std::uint32_t body_idx,
			const vec3f& axis_world,
			angle theta_axis,
			int axis_index,
			float sign
		) -> void;

		auto update_dual(
			float alpha,
			int iteration
		) -> step_delta;

		auto update_joint_dual(
			time_squared h_squared,
			int iteration
		) -> step_delta;

		auto build_islands(
			std::uint32_t num_bodies
		) -> void;

		solver_config m_config;
		std::size_t m_color_grain = 8;
		bool m_ordered_sweep = false;
		constraint_graph m_graph;
		std::vector<body_state> m_bodies;
		std::vector<id> m_body_ids;
		std::vector<body_solve_state> m_solve_state;

		static constexpr std::uint32_t no_motor = std::numeric_limits<std::uint32_t>::max();
		std::vector<std::uint32_t> m_body_motor_index;
		std::vector<bool> m_body_in_color_group;
		std::vector<std::uint8_t> m_body_inactive;

		std::vector<std::uint32_t> m_island_root;
		std::vector<std::uint8_t> m_island_wake;
		std::vector<std::uint8_t> m_island_calm;

		std::vector<std::uint32_t> m_sweep_order;
		std::vector<std::uint32_t> m_active_contacts;

		std::vector<vec3<velocity>> m_prev_velocity;
		std::vector<float> m_accel_weight;

		std::vector<frozen_jacobian> m_frozen_jacobians;
		std::array<std::uint32_t, limits.island_convergence_count> m_convergence_counts{};
	};
}

auto gse::vbd::solver::graph(this auto&& self) -> auto& {
	return self.m_graph;
}

export namespace gse::vbd {
	auto contact_effective_mass(
		const body_state& body_a,
		const body_state& body_b,
		const vec3<lever_arm>& r_aw,
		const vec3<lever_arm>& r_bw,
		const vec3f& dir
	) -> mass;

	auto warm_start_joint(
		joint_constraint& j,
		const body_state& body_a,
		const body_state& body_b,
		time_squared h_squared,
		const solver_config& cfg
	) -> void;

	auto compute_joint_c0(
		joint_constraint& j,
		const body_state& body_a,
		const body_state& body_b
	) -> void;
}

namespace gse::vbd {
	auto accumulate_geometric_stiffness(
		body_solve_state& state,
		const vec3<lever_arm>& r,
		const vec3f& dir,
		force abs_force
	) -> void;
}

auto gse::vbd::solver::body_states(this auto&& self) -> decltype(auto) {
	return std::span(self.m_bodies);
}

auto gse::vbd::solver::body_ids(this auto&& self) -> decltype(auto) {
	return std::span(self.m_body_ids);
}
