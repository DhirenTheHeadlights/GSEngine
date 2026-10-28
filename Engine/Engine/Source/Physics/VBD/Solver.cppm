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
import :muscle_component;

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
		std::uint32_t muscle_count = 0;
		std::uint32_t convergence_probe_iterations = 0;
		std::uint32_t convergence_probe_interval = 64;
		std::uint32_t coupled_limit_normalized = 0;
		velocity joint_correction_speed = meters_per_second(0.f);
		vbd_capacities caps{};
		std::uint32_t muscle_reset_activation = 0;
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

		auto seed_accel_weight(
			std::uint32_t body,
			float weight,
			time_step dt
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

		auto add_muscle_constraint(
			const muscle_constraint& m,
			std::span<const muscle_path_point> path
		) -> void;

		auto set_curve_words(
			std::span<const std::uint32_t> words
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
		auto solve_pass(
			time_step dt
		) -> void;

		struct joint_extremes {
			gap worst_gap{};
			gap worst_start_gap{};
			displacement worst_residual{};
			force worst_lambda{};
			std::uint32_t worst_residual_joint = 0;
			std::uint32_t worst_residual_body_a = 0;
			std::uint32_t worst_residual_body_b = 0;
			vec3<angle> worst_residual_coordinates{};
			torque worst_residual_ang_lambda{};
			stiffness worst_residual_penalty{};
		};

		auto joint_extremes_of(
			time_step dt
		) const -> joint_extremes;

		auto report_convergence(
			const solver& reference,
			time_step dt
		) const -> void;

		auto accumulate_contact(
			const contact_constraint& constraint,
			const frozen_jacobian& jacobian,
			std::uint32_t body_idx,
			float alpha,
			bool live_jacobians
		) -> void;

		auto accumulate_motor(
			const velocity_motor_constraint& m,
			time_squared h_squared
		) -> void;

		auto step_colored_body(
			std::uint32_t body_idx,
			time_squared h_squared,
			time_step dt,
			float alpha,
			bool live_jacobians
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

		auto accumulate_muscle(
			const muscle_constraint& constraint,
			std::uint32_t body_idx,
			time_step dt
		) -> void;

		auto accumulate_wrapped_muscle(
			const muscle_constraint& constraint,
			std::uint32_t body_idx,
			time_step dt
		) -> void;

		auto accumulate_joint_drive(
			const joint_constraint& constraint,
			std::uint32_t body_idx,
			const vec3f& axis_world,
			angle theta_axis,
			int axis_index,
			float sign
		) -> void;

		auto muscle_floor_of(
			std::uint32_t body_idx,
			time_step dt
		) const -> stiffness;

		auto floored_penalty(
			stiffness stored,
			std::uint32_t body_a,
			std::uint32_t body_b
		) const -> stiffness;

		auto contact_normal_penalty(
			const contact_constraint& c
		) const -> stiffness;

		auto update_dual(
			float alpha,
			int iteration
		) -> step_delta;

		auto update_joint_dual(
			time_squared h_squared,
			time_step dt,
			float alpha,
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
		std::vector<stiffness> m_muscle_floor;
		std::vector<std::uint32_t> m_curve_words;

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

		std::uint64_t m_probe_step = 0;

		std::vector<frozen_jacobian> m_frozen_jacobians;
		std::array<std::uint32_t, limits.island_convergence_count> m_convergence_counts{};
	};
}

auto gse::vbd::solver::graph(this auto&& self) -> auto& {
	return self.m_graph;
}

export namespace gse::vbd {
	auto gravity_magnitude() -> acceleration;

	auto contact_effective_mass(
		const body_state& body_a,
		const body_state& body_b,
		const vec3<lever_arm>& r_aw,
		const vec3<lever_arm>& r_bw,
		const vec3f& dir
	) -> mass;

	auto joint_effective_mass(
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
		const solver_config& cfg,
		std::span<const std::uint32_t> curves
	) -> void;

	auto compute_joint_c0(
		joint_constraint& j,
		const body_state& body_a,
		const body_state& body_b,
		std::span<const std::uint32_t> curves
	) -> void;

	struct muscle_response {
		force tension;
		stiffness path_stiffness;
	};

	struct muscle_fiber {
		float cos_pennation = 1.f;
		float sin_pennation = 0.f;
		length fiber;
		float fiber_ratio = 0.f;
		velocity fiber_velocity;
		float active = 0.f;
		float active_slope = 0.f;
		float passive = 0.f;
		float passive_slope = 0.f;
		float force_velocity = 0.f;
		float force_velocity_slope = 0.f;
	};

	struct passive_fiber {
		float force = 0.f;
		float slope = 0.f;
	};

	auto passive_fiber_of(
		float fiber_ratio
	) -> passive_fiber;

	auto muscle_fiber_of(
		const muscle_constraint& m,
		length path,
		length previous_path,
		time_step dt
	) -> std::optional<muscle_fiber>;

	auto muscle_tension(
		const muscle_constraint& m,
		length path,
		length previous_path,
		time_step dt
	) -> muscle_response;

	auto damped_fiber_tension(
		const muscle_constraint& m,
		length path,
		length previous_path,
		time_step dt,
		float damping
	) -> muscle_response;

	struct tendon_response {
		force tension;
		stiffness tendon_stiffness;
	};

	struct compliant_fiber {
		length projection;
		muscle_response response;
	};

	auto tendon_shape_of(
		float strain_at_max_force
	) -> float;

	auto tendon_response_of(
		const muscle_constraint& m,
		length tendon
	) -> tendon_response;

	auto compliant_fiber_of(
		const muscle_constraint& m,
		length path,
		length previous_projection,
		time_step dt
	) -> compliant_fiber;

	auto advance_muscle_fiber(
		muscle_constraint& m,
		length path,
		time_step dt
	) -> void;

	auto muscle_isometric_stiffness(
		const muscle_constraint& m,
		time_step dt
	) -> stiffness;

	auto muscle_fiber_path(
		const muscle_constraint& m
	) -> length;

	auto advance_muscle_activation(
		muscle_constraint& m,
		time_step dt
	) -> void;

	struct muscle_body_gradient {
		vec3f linear;
		vec3<angular_jacobian> angular;
	};

	auto muscle_start_path_length(
		std::span<const muscle_path_point> path,
		std::span<const body_state> bodies,
		std::span<const std::uint32_t> curves
	) -> length;

	auto muscle_start_path_length(
		std::span<const muscle_path_point> path,
		std::span<const muscle_path_point> wraps,
		std::span<const body_state> bodies,
		std::span<const std::uint32_t> curves
	) -> length;

	auto muscle_start_linear_gradient(
		std::span<const muscle_path_point> path,
		std::span<const body_state> bodies,
		std::span<const std::uint32_t> curves,
		std::uint32_t body_idx
	) -> vec3f;

	auto muscle_path_length(
		std::span<const muscle_path_point> path,
		std::span<const body_state> bodies,
		std::span<const std::uint32_t> curves
	) -> length;

	auto muscle_length_gradient(
		std::span<const muscle_path_point> path,
		std::span<const body_state> bodies,
		std::span<const std::uint32_t> curves,
		std::uint32_t body_idx
	) -> muscle_body_gradient;

	struct coupled_frame {
		quat child;
		quat target;
		std::uint32_t count = 1;
		vec3<angle> coordinates;
		std::array<vec3f, 2> perpendiculars = {};
		std::array<vec3f, 3> coordinate_axes = {};
		std::array<vec3<angular_jacobian>, 3> translation_rates = {};
		vec3<lever_arm> target_offset;
	};

	auto coupled_frame_of(
		std::span<const std::uint32_t> curves,
		std::uint32_t joint_words,
		const quat& orientation_a,
		const quat& orientation_b
	) -> coupled_frame;
}

namespace gse::vbd {
	struct curve_sample {
		float value = 0.f;
		float slope = 0.f;
	};

	auto curve_float(
		std::span<const std::uint32_t> curves,
		std::uint32_t index
	) -> float;

	auto curve_vec3(
		std::span<const std::uint32_t> curves,
		std::uint32_t index
	) -> vec3f;

	auto curve_quat(
		std::span<const std::uint32_t> curves,
		std::uint32_t index
	) -> quat;

	auto sample_curve(
		std::span<const std::uint32_t> curves,
		std::uint32_t base,
		std::uint32_t header,
		angle theta
	) -> curve_sample;

	auto twist_about(
		const quat& q,
		const vec3f& axis
	) -> angle;

	struct spatial_sample {
		quat rotation = quat(1.f, 0.f, 0.f, 0.f);
		vec3<displacement> translation;
		std::array<vec3f, 3> rotation_rates = {};
		std::array<vec3<angular_jacobian>, 3> translation_rates = {};
	};

	auto sample_spatial(
		std::span<const std::uint32_t> curves,
		std::uint32_t base,
		std::uint32_t spatial,
		const vec3<angle>& coordinates
	) -> spatial_sample;

	auto coupled_initial_coordinates(
		std::span<const std::uint32_t> curves,
		std::uint32_t spatial,
		const quat& relative,
		std::uint32_t count
	) -> vec3<angle>;

	auto coupled_pseudo_inverse(
		const std::array<vec3f, 3>& rates
	) -> std::array<vec3f, 3>;

	auto coupled_coordinates_of(
		std::span<const std::uint32_t> curves,
		std::uint32_t joint_words,
		const quat& orientation_a,
		const quat& orientation_b
	) -> vec3<angle>;

	auto coupled_frame_at(
		std::span<const std::uint32_t> curves,
		std::uint32_t joint_words,
		const quat& orientation_a,
		const quat& orientation_b,
		const vec3<angle>& coordinates
	) -> coupled_frame;

	auto shared_coupled_frame(
		std::optional<coupled_newton>& newton,
		std::span<const std::uint32_t> curves,
		std::span<const body_state> bodies,
		std::uint32_t joint_words,
		std::uint32_t body_a,
		std::uint32_t body_b
	) -> coupled_frame;

	export auto coupled_slot(
		const coupled_frame& frame,
		std::uint32_t coordinate
	) -> int;

	auto coupled_translation_lever(
		const coupled_frame& frame,
		const vec3f& direction
	) -> vec3<angular_jacobian>;

	struct moving_point {
		vec3<lever_arm> offset;
		std::array<vec3f, 3> coordinate_axes = {};
		std::array<vec3<angular_jacobian>, 3> rates = {};
	};

	auto moving_point_lever(
		const moving_point& point,
		const vec3f& direction
	) -> vec3<angular_jacobian>;

	struct moving_frame {
		vec3<lever_arm> anchor;
		quat placed;
		spatial_sample sample;
		std::array<vec3f, 3> coordinate_axes = {};
	};

	auto moving_frame_joint_words(
		std::span<const std::uint32_t> curves,
		std::uint32_t frame_words
	) -> std::uint32_t;

	auto moving_frame_in(
		std::span<const std::uint32_t> curves,
		std::uint32_t frame_words,
		const quat& orientation_a,
		const coupled_frame& joint
	) -> moving_frame;

	auto moving_frame_of(
		std::span<const std::uint32_t> curves,
		std::uint32_t frame_words,
		const quat& orientation_a,
		const quat& orientation_b
	) -> moving_frame;

	auto moving_point_in(
		const moving_frame& frame,
		const vec3<lever_arm>& local_point
	) -> moving_point;

	auto moving_point_of(
		std::span<const std::uint32_t> curves,
		std::uint32_t frame_words,
		const quat& orientation_a,
		const quat& orientation_b,
		const vec3<lever_arm>& local_point
	) -> moving_point;

	auto first_moving_point(
		std::span<const muscle_path_point> path
	) -> const muscle_path_point*;

	auto muscle_frame_of(
		std::span<const muscle_path_point> path,
		std::span<const body_state> bodies,
		std::span<const std::uint32_t> curves
	) -> std::optional<moving_frame>;

	auto muscle_frame_of(
		std::span<const muscle_path_point> path,
		std::span<const body_state> bodies,
		std::span<const std::uint32_t> curves,
		std::optional<coupled_newton>& newton
	) -> std::optional<moving_frame>;

	auto muscle_point_of(
		const muscle_path_point& point,
		std::span<const body_state> bodies,
		const std::optional<moving_frame>& frame
	) -> vec3<predicted_position>;

	auto muscle_start_point_of(
		const muscle_path_point& point,
		std::span<const body_state> bodies,
		std::span<const std::uint32_t> curves
	) -> vec3<position>;

	auto muscle_path_length(
		std::span<const muscle_path_point> path,
		std::span<const body_state> bodies,
		const std::optional<moving_frame>& frame
	) -> length;

	auto muscle_length_gradient(
		std::span<const muscle_path_point> path,
		std::span<const body_state> bodies,
		const std::optional<moving_frame>& frame,
		std::uint32_t body_idx
	) -> muscle_body_gradient;

	struct muscle_row {
		vec3f linear;
		vec3<angular_jacobian> angular;
		vec3<angular_stiffness> geometric;
	};

	auto muscle_row_of(
		std::span<const muscle_path_point> path,
		std::span<const body_state> bodies,
		const std::optional<moving_frame>& frame,
		std::uint32_t body_idx,
		force abs_force
	) -> muscle_row;

	enum class wrap_result : std::uint32_t {
		no_wrap,
		inside,
		wrapped,
		mandatory,
	};

	struct wrap_obstacle {
		vec3<displacement> center;
		quat orientation;
		length radius;
		length half_length;
		vec3f side;
		std::uint32_t first_segment = 0;
		std::uint32_t segment_end = 0;
	};

	struct wrap_tangents {
		wrap_result result = wrap_result::no_wrap;
		vec3<displacement> t1;
		vec3<displacement> t2;
		length arc;
	};

	struct wrap_contact {
		std::uint32_t segment = limits.no_wrap_segment;
		std::uint32_t order = 0;
		vec3<displacement> t1;
		vec3<displacement> t2;
		length arc;
	};

	using wrap_contacts = std::array<wrap_contact, physics::max_muscle_wraps>;

	struct wrap_path {
		wrap_contacts contacts;
		length path_length;
	};

	struct record_pose {
		vec3<displacement> origin;
		quat orientation;
	};

	struct wrap_node {
		vec3<displacement> position;
		vec3<lever_arm> lever;
		std::uint32_t body = 0;
		std::uint32_t wrap = limits.no_wrap_segment;
		bool arc_follows = false;
	};

	struct wrapped_muscle {
		std::array<wrap_node, physics::max_muscle_path_points + 2 * physics::max_muscle_wraps> nodes;
		std::uint32_t count = 0;
		wrap_contacts contacts;
		length path_length;
	};

	export auto wrap_obstacle_of(
		std::span<const std::uint32_t> curves,
		const muscle_path_point& wrap,
		const record_pose& pose
	) -> wrap_obstacle;

	auto wrap_segment(
		const vec3<displacement>& p1,
		const vec3<displacement>& p2,
		const wrap_obstacle& obstacle
	) -> wrap_tangents;

	auto wrap_muscle_path(
		std::span<const vec3<displacement>> points,
		std::span<const wrap_obstacle> obstacles
	) -> wrap_path;

	auto ordered_wrap(
		const wrap_contacts& contacts,
		std::uint32_t wrap_count,
		std::uint32_t rank
	) -> std::uint32_t;

	auto wrapped_length(
		std::span<const vec3<displacement>> points,
		const wrap_contacts& contacts,
		std::uint32_t wrap_count
	) -> length;

	export auto wrapped_muscle_of(
		std::span<const muscle_path_point> path,
		std::span<const muscle_path_point> wraps,
		std::span<const std::uint32_t> curves,
		std::span<const record_pose> poses
	) -> wrapped_muscle;

	export auto start_poses_of(
		std::span<const muscle_path_point> path,
		std::span<const muscle_path_point> wraps,
		std::span<const body_state> bodies
	) -> std::array<record_pose, physics::max_muscle_path_points + physics::max_muscle_wraps>;

	auto predicted_poses_of(
		std::span<const muscle_path_point> path,
		std::span<const muscle_path_point> wraps,
		std::span<const body_state> bodies
	) -> std::array<record_pose, physics::max_muscle_path_points + physics::max_muscle_wraps>;

	auto wrapped_length_gradient(
		const wrapped_muscle& w,
		std::uint32_t body_idx
	) -> muscle_body_gradient;

	auto coupled_perpendiculars(
		const vec3f& axis
	) -> std::array<vec3f, 2>;

	auto body_perpendiculars(
		const quat& orientation,
		const vec3f& local_axis
	) -> std::array<vec3f, 2>;

	auto geometric_stiffness_diagonal(
		const vec3<lever_arm>& r,
		const vec3f& dir,
		force abs_force
	) -> vec3<angular_stiffness>;

	auto accumulate_geometric_stiffness(
		body_solve_state& state,
		const vec3<lever_arm>& r,
		const vec3f& dir,
		force abs_force
	) -> void;

	auto contact_jacobian_of(
		const contact_constraint& c,
		const body_state& body_a,
		const body_state& body_b
	) -> frozen_jacobian;

	struct angular_limit_row {
		vec3f axis;
		angle coordinate;
		angle lower;
		angle upper;
		angle c0;
		float scale = 1.f;
	};

	auto limit_axis_of(
		const joint_constraint& j,
		const quat& orientation_a,
		int row
	) -> vec3f;

	auto limit_row_of(
		const joint_constraint& j,
		const quat& orientation_a,
		const vec3<angle>& theta,
		int row,
		float c0_scale
	) -> angular_limit_row;

	export auto coupled_limit_row(
		const joint_constraint& j,
		const coupled_frame& frame,
		std::uint32_t coordinate,
		float c0_scale,
		bool normalized
	) -> angular_limit_row;

	auto coupled_limit_scale(
		const vec3f& axis,
		bool normalized
	) -> float;

	auto joint_carried_fraction(
		const vec3<gap>& c0,
		float alpha,
		velocity correction_speed,
		time_step dt
	) -> float;

	auto accumulate_drive_row(
		body_solve_state& state,
		const joint_constraint& j,
		const vec3f& axis,
		angle coordinate,
		angle previous,
		int index,
		float sign
	) -> void;

	struct angular_limit_force {
		angle error;
		torque force;
	};

	auto limit_force_of(
		const angular_limit_row& row,
		torque lambda,
		angular_stiffness penalty
	) -> angular_limit_force;

	auto accumulate_angular_limit(
		body_solve_state& state,
		const angular_limit_row& row,
		torque lambda,
		angular_stiffness penalty,
		float sign
	) -> void;

	auto update_angular_limit_dual(
		const angular_limit_row& row,
		torque& lambda,
		angular_stiffness& penalty,
		const solver_config& cfg
	) -> std::optional<angle>;

	auto angular_limit_c0(
		const angular_limit_row& row
	) -> angle;

	auto angular_row_penalty(
		const body_state& body_a,
		const body_state& body_b,
		const vec3f& axis,
		angular_stiffness previous,
		time_squared h_squared,
		const solver_config& cfg
	) -> angular_stiffness;
}

auto gse::vbd::solver::body_states(this auto&& self) -> decltype(auto) {
	return std::span(self.m_bodies);
}

auto gse::vbd::solver::body_ids(this auto&& self) -> decltype(auto) {
	return std::span(self.m_body_ids);
}
