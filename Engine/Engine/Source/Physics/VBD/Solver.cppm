export module gse.physics:vbd_solver;

import std;

import gse.core;
import gse.math;
import gse.containers;
import gse.concurrency;
import gse.diag;
import :vbd_constraints;
import :vbd_constraint_graph;
import :vbd_contact_cache;
import :motion_component;
import :contact_manifold;

export namespace gse::vbd {
	using time_step = time_t<float, seconds>;

	struct step_delta {
		length linear{};
		angle angular{};
	};

	struct[[= shaders::shader_struct]] solver_config {
		std::uint32_t iterations = 4;
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
		velocity velocity_sleep_threshold = meters_per_second(0.001f);
		angular_velocity angular_sleep_threshold = radians_per_second(0.05f);
		gap speculative_margin = meters(0.02f);
		length convergence_threshold_linear = meters(0.01f);
		angle convergence_threshold_angular = radians(0.01f);
		std::uint32_t min_iterations = 4;
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
	};

	class solver {
	public:
		solver();

		auto configure(
			const solver_config& cfg
		) -> void;

		auto config() const -> const solver_config&;

		auto begin_frame(
			std::span<const body_state> bodies,
			contact_cache& cache
		) -> void;

		auto seed_previous_velocities(
			std::span<const vec3<velocity>> velocities
		) -> void;

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

		auto update_dual(
			float alpha
		) -> step_delta;

		auto update_joint_dual(
			time_squared h_squared
		) -> step_delta;

		solver_config m_config;
		constraint_graph m_graph;
		std::vector<body_state> m_bodies;
		std::vector<body_solve_state> m_solve_state;

		static constexpr std::uint32_t no_motor = std::numeric_limits<std::uint32_t>::max();
		std::vector<std::uint32_t> m_body_motor_index;
		std::vector<bool> m_body_in_color_group;

		std::vector<vec3<velocity>> m_prev_velocity;
		std::vector<float> m_accel_weight;

		std::vector<frozen_jacobian> m_frozen_jacobians;
	};
}

template <typename Self>
auto gse::vbd::solver::graph(this Self&& self) -> auto& {
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

gse::vbd::solver::solver() {
	m_bodies.reserve(limits.max_bodies);
	m_solve_state.reserve(limits.max_bodies);
	m_body_motor_index.reserve(limits.max_bodies);
	m_body_in_color_group.reserve(limits.max_bodies);
	m_prev_velocity.reserve(limits.max_bodies);
	m_accel_weight.reserve(limits.max_bodies);
	m_frozen_jacobians.reserve(limits.max_contacts);
}

auto gse::vbd::solver::configure(const solver_config& cfg) -> void {
	m_config = cfg;
}

auto gse::vbd::solver::config() const -> const solver_config& {
	return m_config;
}

auto gse::vbd::solver::begin_frame(const std::span<const body_state> bodies, contact_cache& cache) -> void {
	m_bodies.assign(bodies.begin(), bodies.end());
	m_solve_state.resize(m_bodies.size());
	m_graph.clear();

	cache.age_and_prune();
}

auto gse::vbd::solver::seed_previous_velocities(const std::span<const vec3<velocity>> velocities) -> void {
	m_prev_velocity.assign(velocities.begin(), velocities.end());
	m_accel_weight.assign(velocities.size(), 0.f);
}

auto gse::vbd::solver::add_contact_constraint(const contact_constraint& c) -> void {
	m_graph.add_contact(c);
}

auto gse::vbd::solver::add_motor_constraint(const velocity_motor_constraint& m) -> void {
	m_graph.add_motor(m);
}

auto gse::vbd::solver::add_joint_constraint(const joint_constraint& j) -> void {
	m_graph.add_joint(j);
}

auto gse::vbd::solver::solve(const time_step dt) -> void {
	trace::scope_guard sg_solve{ trace_id<"vbd::solve">() };

	const auto num_bodies = static_cast<std::uint32_t>(m_bodies.size());
	const time_squared h_squared = dt * dt;

	{
		trace::scope_guard sg{ trace_id<"vbd::coloring">() };
		static_vector<bool, limits.max_bodies> locked;
		for (std::uint32_t i = 0; i < num_bodies; ++i) {
			locked.push_back(m_bodies[i].locked);
		}
		m_graph.compute_coloring(num_bodies, locked.span());
	}

	m_body_motor_index.assign(num_bodies, no_motor);
	for (std::uint32_t mi = 0; mi < m_graph.motor_constraints().size(); ++mi) {
		m_body_motor_index[m_graph.motor_constraints()[mi].body_index] = mi;
	}

	m_body_in_color_group.assign(num_bodies, false);
	for (const auto& bc : m_graph.body_colors()) {
		for (const auto bi : bc) {
			m_body_in_color_group[bi] = true;
		}
	}

	{
		auto& contacts_ref = m_graph.contact_constraints();
		task::coarse_parallel(contacts_ref.size(), 512, [&, this](std::size_t ci) {
			auto& c = contacts_ref[ci];
			const auto& ba = m_bodies[c.body_a];
			const auto& bb = m_bodies[c.body_b];
			const vec3<lever_arm> r_aw = rotate_vector(ba.orientation, c.local_anchor_a);
			const vec3<lever_arm> r_bw = rotate_vector(bb.orientation, c.local_anchor_b);
			const vec3<position> p_a = ba.position + r_aw;
			const vec3<position> p_b = bb.position + r_bw;
			const vec3<displacement> d = p_a - p_b;

			c.c0[0] = dot(c.normal, d) + m_config.collision_margin;
			c.c0[1] = dot(c.tangent_u, d);
			c.c0[2] = dot(c.tangent_v, d);

			const auto va = ba.velocity + cross(ba.angular_velocity, r_aw) / rad;
			const auto vb = bb.velocity + cross(bb.angular_velocity, r_bw) / rad;
			const velocity v_rel_n = dot(c.normal, va - vb);
			c.approach_speed = (v_rel_n < normal_speed{}) ? normal_speed{ v_rel_n } : normal_speed{};
		},
							  trace_id<"vbd::warm_contacts">());
	}

	{
		auto& contacts_ref = m_graph.contact_constraints();
		task::coarse_parallel(contacts_ref.size(), 256, [&, this](std::size_t ci) {
			auto& c = contacts_ref[ci];
			const auto& ba = m_bodies[c.body_a];
			const auto& bb = m_bodies[c.body_b];

			const vec3<lever_arm> r_aw = rotate_vector(ba.orientation, c.local_anchor_a);
			const vec3<lever_arm> r_bw = rotate_vector(bb.orientation, c.local_anchor_b);

			const stiffness base_floor = std::max(c.penalty_floor, m_config.penalty_min);
			const bool active_normal = c.lambda[0] < newtons(-1e-3f) || c.c0[0] < meters(-1e-4f);

			const stiffness normal_floor =
				active_normal
				? std::clamp<stiffness>(
					  contact_effective_mass(ba, bb, r_aw, r_bw, c.normal) / h_squared,
					  base_floor,
					  m_config.penalty_max
				  )
				: base_floor;

			const std::array row_floor = {
				normal_floor,
				c.sticking
					? std::clamp<stiffness>(
						  contact_effective_mass(ba, bb, r_aw, r_bw, c.tangent_u) / h_squared,
						  m_config.penalty_min,
						  m_config.penalty_max
					  )
					: m_config.penalty_min,
				c.sticking
					? std::clamp<stiffness>(
						  contact_effective_mass(ba, bb, r_aw, r_bw, c.tangent_v) / h_squared,
						  m_config.penalty_min,
						  m_config.penalty_max
					  )
					: m_config.penalty_min
			};

			for (int i = 0; i < 3; i++) {
				if (m_config.post_stabilize) {
					c.penalty[i] = std::clamp(c.penalty[i] * m_config.gamma, row_floor[i], m_config.penalty_max);
				}
				else {
					c.lambda[i] *= m_config.alpha * m_config.gamma;
					c.penalty[i] = std::clamp(c.penalty[i] * m_config.gamma, row_floor[i], m_config.penalty_max);
				}
			}
		},
							  trace_id<"vbd::contact_penalty">());
	}

	{
		auto& joints_ref = m_graph.joint_constraints();
		task::coarse_parallel(joints_ref.size(), 32, [&, this](std::size_t ji) {
			warm_start_joint(joints_ref[ji], m_bodies[joints_ref[ji].body_a], m_bodies[joints_ref[ji].body_b], h_squared, m_config);
		},
							  trace_id<"vbd::warm_joints">());
	}

	{
		auto& joints_ref = m_graph.joint_constraints();
		task::coarse_parallel(joints_ref.size(), 32, [&, this](std::size_t ji) {
			compute_joint_c0(joints_ref[ji], m_bodies[joints_ref[ji].body_a], m_bodies[joints_ref[ji].body_b]);
		},
							  trace_id<"vbd::joint_c0">());
	}

	constexpr acceleration gravity_mag = meters_per_second_squared(9.8f);
	const vec3<acceleration> gravity = { meters_per_second_squared(0.f), -gravity_mag, meters_per_second_squared(0.f) };

	if (m_prev_velocity.size() != m_bodies.size()) {
		m_prev_velocity.assign(m_bodies.size(), vec3<velocity>{});
		m_accel_weight.assign(m_bodies.size(), 0.f);
	}

	{
		task::coarse_parallel(num_bodies, 64, [&, this](std::size_t i) {
			auto& body = m_bodies[i];
			if (body.locked || body.sleeping()) {
				m_accel_weight[i] = 0.f;
				m_prev_velocity[i] = body.velocity;
				body.predicted_position = body.position;
				body.inertia_target = body.position;
				body.predicted_orientation = body.orientation;
				body.angular_inertia_target = body.orientation;
				body.motor_target = body.position;
				return;
			}

			const velocity delta_vy = body.velocity.y() - m_prev_velocity[i].y();
			const acceleration accel_y = delta_vy / dt;
			float aw = std::clamp(-(accel_y / gravity_mag), 0.f, 1.f);
			if (!std::isfinite(aw)) {
				aw = 0.f;
			}
			m_accel_weight[i] = aw;
			m_prev_velocity[i] = body.velocity;

			body.velocity *= (1.f - m_config.linear_damping);
			body.angular_velocity *= (1.f - m_config.angular_damping);

			if (const auto v = magnitude(body.velocity); v > m_config.max_linear_speed) {
				body.velocity *= (m_config.max_linear_speed / v);
			}

			if (const auto w = magnitude(body.angular_velocity); w > m_config.max_angular_speed) {
				body.angular_velocity *= (m_config.max_angular_speed / w);
			}

			body.inertia_target = body.position + body.velocity * dt;
			if (body.affected_by_gravity) {
				body.inertia_target += gravity * dt * dt;
			}

			body.old_position = body.position;
			body.old_orientation = body.orientation;

			body.predicted_position = body.position + body.velocity * dt;
			if (body.affected_by_gravity) {
				body.predicted_position += gravity * dt * dt * aw;
			}

			if (body.update_orientation) {
				const vec3<angle> aa = body.angular_velocity * dt;
				if (magnitude(aa) > radians(1e-7f)) {
					body.predicted_orientation = normalize(from_axis_angle_vector(aa) * body.orientation);
				}
				else {
					body.predicted_orientation = body.orientation;
				}
				body.angular_inertia_target = body.predicted_orientation;
			}
			else {
				body.predicted_orientation = body.orientation;
				body.angular_inertia_target = body.orientation;
			}

			body.motor_target = body.inertia_target;
		},
							  trace_id<"vbd::predict_bodies">());
	}

	for (const auto& motor : m_graph.motor_constraints()) {
		auto& body = m_bodies[motor.body_index];
		if (body.locked) {
			continue;
		}

		if (body.sleeping()) {
			if (magnitude(motor.target_velocity) > meters_per_second(0.01f)) {
				body.sleep_counter = 0;
			}
			else {
				continue;
			}
		}

		const vec3<target_position> target = body.old_position + motor.target_velocity * dt;
		if (motor.horizontal_only) {
			body.motor_target.x() = target.x();
			body.motor_target.z() = target.z();
		}
		else {
			body.motor_target = target;
		}
	}

	const velocity wake_thresh = m_config.velocity_sleep_threshold * 10.f;
	for (const auto& c : m_graph.contact_constraints()) {
		auto& a = m_bodies[c.body_a];
		auto& b = m_bodies[c.body_b];
		if (a.sleeping() && !b.locked && !b.sleeping() && magnitude(b.velocity) > wake_thresh) {
			a.sleep_counter = 0;
		}
		if (b.sleeping() && !a.locked && !a.sleeping() && magnitude(a.velocity) > wake_thresh) {
			b.sleep_counter = 0;
		}
	}

	for (const auto& j : m_graph.joint_constraints()) {
		auto& a = m_bodies[j.body_a];
		auto& b = m_bodies[j.body_b];
		if (a.sleeping() && !b.locked && !b.sleeping()) {
			a.sleep_counter = 0;
		}
		if (b.sleeping() && !a.locked && !a.sleeping()) {
			b.sleep_counter = 0;
		}
	}

	const auto& contacts = m_graph.contact_constraints();
	const auto& motors = m_graph.motor_constraints();
	const auto& joints = m_graph.joint_constraints();

	const int num_iterations = static_cast<int>(m_config.iterations);
	const float solve_alpha = m_config.post_stabilize ? 1.0f : m_config.alpha;

	{
		m_frozen_jacobians.resize(contacts.size());
		task::coarse_parallel(contacts.size(), 256, [&, this](std::size_t ci) {
			const auto& c = contacts[ci];
			const auto& ba = m_bodies[c.body_a];
			const auto& bb = m_bodies[c.body_b];

			const vec3<lever_arm> r_aw = rotate_vector(ba.predicted_orientation, c.local_anchor_a);
			const vec3<lever_arm> r_bw = rotate_vector(bb.predicted_orientation, c.local_anchor_b);
			const std::array dirs = { c.normal, c.tangent_u, c.tangent_v };

			m_frozen_jacobians[ci] = {
				.world_r_a = r_aw,
				.world_r_b = r_bw,
				.j_ang_a = { cross(r_aw, dirs[0]), cross(r_aw, dirs[1]), cross(r_aw, dirs[2]) },
				.j_ang_b = { cross(r_bw, dirs[0]), cross(r_bw, dirs[1]), cross(r_bw, dirs[2]) },
			};
		},
							  trace_id<"vbd::frozen_jacobians">());
	}

	auto solve_iteration_gauss_seidel = [&](const float alpha) {
		for (const auto& body_color : m_graph.body_colors()) {
			task::coarse_parallel(body_color.size(), 8, [&, this](std::size_t k) {
				const auto bi = body_color[k];
				m_solve_state[bi] = {};
				for (const auto ci : m_graph.body_contact_indices(bi)) {
					accumulate_contact(contacts[ci], m_frozen_jacobians[ci], bi, h_squared, alpha);
				}
				for (const auto ji : m_graph.body_joint_indices(bi)) {
					accumulate_joint(joints[ji], bi, h_squared, dt, alpha);
				}
				if (const auto mi = m_body_motor_index[bi]; mi != no_motor) {
					accumulate_motor(motors[mi], h_squared);
				}
				perform_newton_step(bi, h_squared);
			},
								  trace_id<"vbd::gs_color_iter">());
		}

		for (const auto& motor : motors) {
			if (!m_body_in_color_group[motor.body_index]) {
				m_solve_state[motor.body_index] = {};
				accumulate_motor(motor, h_squared);
				for (const auto ji : m_graph.body_joint_indices(motor.body_index)) {
					accumulate_joint(joints[ji], motor.body_index, h_squared, dt, alpha);
				}
				perform_newton_step(motor.body_index, h_squared);
			}
		}

		for (std::uint32_t i = 0; i < num_bodies; ++i) {
			if (m_bodies[i].locked || m_bodies[i].sleeping()) {
				continue;
			}
			if (m_body_in_color_group[i]) {
				continue;
			}
			if (m_body_motor_index[i] != no_motor) {
				continue;
			}
			m_solve_state[i] = {};
			for (const auto ji : m_graph.body_joint_indices(i)) {
				accumulate_joint(joints[ji], i, h_squared, dt, alpha);
			}
			perform_newton_step(i, h_squared);
		}
	};

	auto solve_iteration_jacobi = [&](const float alpha) {
		task::coarse_parallel(num_bodies, 32, [&, this](std::size_t bi) {
			if (m_bodies[bi].locked || m_bodies[bi].sleeping()) {
				return;
			}
			m_solve_state[bi] = {};
			for (const auto ci : m_graph.body_contact_indices(bi)) {
				accumulate_contact(contacts[ci], m_frozen_jacobians[ci], static_cast<std::uint32_t>(bi), h_squared, alpha);
			}
			for (const auto ji : m_graph.body_joint_indices(bi)) {
				accumulate_joint(joints[ji], static_cast<std::uint32_t>(bi), h_squared, dt, alpha);
			}
			if (const auto mi = m_body_motor_index[bi]; mi != no_motor) {
				accumulate_motor(motors[mi], h_squared);
			}
		},
							  trace_id<"vbd::jacobi_accum">());

		task::coarse_parallel(num_bodies, 128, [&, this](std::size_t bi) {
			if (m_bodies[bi].locked || m_bodies[bi].sleeping()) {
				return;
			}
			perform_newton_step(static_cast<std::uint32_t>(bi), h_squared);
		},
							  trace_id<"vbd::jacobi_step">());
	};

	auto solve_iteration = [&](const float alpha) {
		if (m_config.use_jacobi) {
			solve_iteration_jacobi(alpha);
		}
		else {
			solve_iteration_gauss_seidel(alpha);
		}
	};

	{
		trace::scope_guard sg{ trace_id<"vbd::iterations">() };
		for (int it = 0; it < num_iterations; ++it) {
			solve_iteration(solve_alpha);
			const auto contact_delta = update_dual(solve_alpha);
			const auto joint_delta = update_joint_dual(h_squared);
			const length linear = std::max(contact_delta.linear, joint_delta.linear);
			const angle angular = std::max(contact_delta.angular, joint_delta.angular);

			if (it + 1 >= static_cast<int>(m_config.min_iterations) &&
				linear < m_config.convergence_threshold_linear &&
				angular < m_config.convergence_threshold_angular) {
				break;
			}
		}
	}

	{
		task::coarse_parallel(m_bodies.size(), 128, [&, this](std::size_t i) {
			auto& body = m_bodies[i];
			if (!body.locked && !body.sleeping() && body.mass > mass{}) {
				body.velocity = (body.predicted_position - body.old_position) / dt;

				if (body.update_orientation) {
					body.angular_velocity = difference_axis_angle(body.old_orientation, body.predicted_orientation) / dt;
				}
			}
		},
							  trace_id<"vbd::velocity_writeback">());
	}

	for (const auto& c : m_graph.contact_constraints()) {
		if (c.restitution <= 0.f) {
			continue;
		}
		if (c.lambda[0] >= force{}) {
			continue;
		}
		if (c.approach_speed >= -m_config.restitution_threshold) {
			continue;
		}

		auto& ba = m_bodies[c.body_a];
		auto& bb = m_bodies[c.body_b];

		const vec3<lever_arm> rAW = rotate_vector(ba.orientation, c.local_anchor_a);
		const vec3<lever_arm> rBW = rotate_vector(bb.orientation, c.local_anchor_b);

		const auto va = ba.velocity + cross(ba.angular_velocity, rAW) / rad;
		const auto vb = bb.velocity + cross(bb.angular_velocity, rBW) / rad;
		const velocity v_rel_after = dot(c.normal, va - vb);

		const float speed_ratio = std::clamp(
			(-c.approach_speed - m_config.restitution_threshold) / m_config.restitution_threshold,
			0.f,
			1.f
		);
		const velocity v_target = -c.restitution * speed_ratio * c.approach_speed;
		const velocity dv = v_target - v_rel_after;

		if (dv <= velocity{}) {
			continue;
		}

		const inverse_mass w_a = ba.locked ? inverse_mass{} : 1.f / ba.mass;
		const inverse_mass w_b = bb.locked ? inverse_mass{} : 1.f / bb.mass;
		const inverse_mass w_total = w_a + w_b;

		if (w_total <= inverse_mass{}) {
			continue;
		}

		const auto impulse = c.normal * (dv / w_total);

		if (!ba.locked) {
			ba.velocity += impulse * w_a;
		}
		if (!bb.locked) {
			bb.velocity -= impulse * w_b;
		}
	}

	if (m_config.post_stabilize) {
		trace::scope_guard sg{ trace_id<"vbd::post_stabilize">() };
		solve_iteration(0.0f);
	}

	{
		task::coarse_parallel(m_bodies.size(), 128, [&, this](std::size_t i) {
			auto& body = m_bodies[i];
			if (body.locked || body.sleeping()) {
				return;
			}

			if (magnitude(body.velocity) < m_config.velocity_sleep_threshold &&
				magnitude(body.angular_velocity) < m_config.angular_sleep_threshold) {
				++body.sleep_counter;
			}
			else {
				body.sleep_counter /= 2;
			}

			body.position = body.predicted_position;
			if (body.update_orientation) {
				body.orientation = body.predicted_orientation;
			}
		},
							  trace_id<"vbd::sleep_writeback">());
	}
}

auto gse::vbd::solver::end_frame(std::vector<body_state>& bodies, contact_cache& cache) -> void {
	for (const auto& c : m_graph.contact_constraints()) {
		const force friction_bound = abs(c.lambda[0]) * c.friction_coeff;
		const force tangential_lambda = hypot(c.lambda[1], c.lambda[2]);
		const length tangential_gap = hypot(c.c0[1], c.c0[2]);

		const bool sticking =
			c.lambda[0] < newtons(-1e-3f) &&
			tangential_gap < m_config.stick_threshold &&
			tangential_lambda < friction_bound;

		cache.store(c.body_a, c.body_b, unpack_feature(c.feature_key), cached_lambda{ .lambda = c.lambda, .penalty = c.penalty, .normal = c.normal, .tangent_u = c.tangent_u, .tangent_v = c.tangent_v, .local_anchor_a = c.local_anchor_a, .local_anchor_b = c.local_anchor_b, .sticking = sticking, .age = 0 });
	}

	bodies.assign(m_bodies.begin(), m_bodies.end());
}

auto gse::vbd::solver::body_states(this auto&& self) -> decltype(auto) {
	return std::span(self.m_bodies);
}

auto gse::vbd::solver::accumulate_contact(const contact_constraint& constraint, const frozen_jacobian& frozen, const std::uint32_t body_idx, const time_squared h_squared, const float alpha) -> void {
	const auto& body_a = m_bodies[constraint.body_a];
	const auto& body_b = m_bodies[constraint.body_b];
	const bool is_a = (body_idx == constraint.body_a);

	const vec3<predicted_position> p_a = body_a.predicted_position + frozen.world_r_a;
	const vec3<predicted_position> p_b = body_b.predicted_position + frozen.world_r_b;
	const vec3<displacement> d = p_a - p_b;

	const vec3<gap> cn = {
		dot(constraint.normal, d) + m_config.collision_margin,
		dot(constraint.tangent_u, d),
		dot(constraint.tangent_v, d)
	};
	const vec3<gap> c = cn - constraint.c0 * alpha;

	const force friction_bound = abs(constraint.lambda[0]) * constraint.friction_coeff;

	const force f0 = std::min<force>(constraint.penalty[0] * c[0] + constraint.lambda[0], force{});
	const force f1 = std::clamp<force>(constraint.penalty[1] * c[1] + constraint.lambda[1], -friction_bound, friction_bound);
	const force f2 = std::clamp<force>(constraint.penalty[2] * c[2] + constraint.lambda[2], -friction_bound, friction_bound);

	const float sign = is_a ? 1.f : -1.f;
	const std::array dirs = { constraint.normal, constraint.tangent_u, constraint.tangent_v };
	const std::array f = { f0, f1, f2 };
	const auto& j_ang_frozen = is_a ? frozen.j_ang_a : frozen.j_ang_b;

	for (int i = 0; i < 3; i++) {
		if (i > 0 && friction_bound <= newtons(1e-10f)) {
			continue;
		}
		if (abs(f[i]) < newtons(1e-12f) && constraint.penalty[i] < newtons_per_meter(1e-6f)) {
			continue;
		}

		const vec3f j_lin = dirs[i] * sign;
		const vec3<length> j_ang = j_ang_frozen[i] * sign;

		m_solve_state[body_idx].gradient += j_lin * f[i];
		m_solve_state[body_idx].hessian += outer_product(j_lin, j_lin) * constraint.penalty[i];

		if (m_bodies[body_idx].update_orientation) {
			m_solve_state[body_idx].angular_gradient += j_ang * f[i];
			m_solve_state[body_idx].angular_hessian += outer_product(j_ang, j_ang) * constraint.penalty[i] / rad;
			m_solve_state[body_idx].hessian_xtheta += outer_product(j_lin, j_ang) * constraint.penalty[i] / rad;
		}
	}
}

auto gse::vbd::solver::accumulate_motor(const velocity_motor_constraint& m, const time_squared h_squared) -> void {
	const auto& body = m_bodies[m.body_index];
	if (body.locked || body.sleeping()) {
		return;
	}

	const float compliance = std::max(m.compliance, 1e-6f);
	const stiffness motor_stiffness = body.mass / h_squared / compliance;

	const vec3<displacement> diff = body.predicted_position - body.motor_target;

	vec3<length> masked_diff = diff;
	if (m.horizontal_only) {
		masked_diff[1] = length{};
	}

	vec3<force> motor_gradient = masked_diff * motor_stiffness;
	if (const force grad_mag = magnitude(motor_gradient); grad_mag > m.max_force) {
		motor_gradient *= (m.max_force / grad_mag);
	}

	const force grad_before = magnitude(motor_gradient);

	const auto& contacts = m_graph.contact_constraints();
	for (const auto ci : m_graph.body_contact_indices(m.body_index)) {
		const auto& c = contacts[ci];
		const auto other = (c.body_a == m.body_index) ? c.body_b : c.body_a;
		const auto& other_body = m_bodies[other];

		const bool blocking_body =
			other_body.locked ||
			other_body.mass >= body.mass;

		if (!blocking_body) {
			continue;
		}

		const auto& body_a = m_bodies[c.body_a];
		const auto& body_b = m_bodies[c.body_b];

		const vec3<lever_arm> r_aw = rotate_vector(body_a.predicted_orientation, c.local_anchor_a);
		const vec3<lever_arm> r_bw = rotate_vector(body_b.predicted_orientation, c.local_anchor_b);
		const vec3<predicted_position> p_a = body_a.predicted_position + r_aw;
		const vec3<predicted_position> p_b = body_b.predicted_position + r_bw;
		const vec3<displacement> d = p_a - p_b;

		if (const length normal_gap = dot(c.normal, d) + m_config.collision_margin; normal_gap >= length{} && c.lambda[0] >= newtons(-1e-3f)) {
			continue;
		}

		const vec3f push_dir = (c.body_a == m.body_index) ? c.normal : -c.normal;
		if (const force proj = dot(motor_gradient, push_dir); proj > force{}) {
			motor_gradient -= push_dir * proj;
		}
	}

	const force grad_after = magnitude(motor_gradient);
	const float motor_scale = (grad_before > newtons(1e-10f)) ? (grad_after / grad_before) : 0.f;

	m_solve_state[m.body_index].gradient += motor_gradient;

	const stiffness scaled_stiffness = motor_stiffness * motor_scale;
	mat3 motor_hessian(scaled_stiffness);
	if (m.horizontal_only) {
		motor_hessian[1][1] = stiffness{};
	}

	m_solve_state[m.body_index].hessian += motor_hessian;
}

auto gse::vbd::solver::perform_newton_step(const std::uint32_t body_idx, const time_squared h_squared) -> step_delta {
	auto& body = m_bodies[body_idx];

	if (body.locked || body.sleeping()) {
		return {};
	}
	if (body.inverse_mass() < per_kilograms(1e-10f)) {
		return {};
	}

	const stiffness inertia_weight = body.mass / h_squared;

	const vec3<force> g_lin = (body.predicted_position - body.inertia_target) * inertia_weight + m_solve_state[body_idx].gradient;

	mat3<stiffness> h_xx = mat3(inertia_weight) + m_solve_state[body_idx].hessian;
	h_xx[0][0] += m_config.linear_regularization;
	h_xx[1][1] += m_config.linear_regularization;
	h_xx[2][2] += m_config.linear_regularization;

	const float omega = m_config.use_jacobi ? m_config.jacobi_omega : 1.f;

	if (!body.update_orientation) {
		const auto inv_h = h_xx.inverse();
		auto delta_x = -(inv_h * g_lin);

		const auto step_size = magnitude(delta_x);
		if (step_size > m_config.max_linear_step) {
			delta_x *= (m_config.max_linear_step / step_size);
		}

		body.predicted_position += delta_x * omega;
		return { step_size, angle{} };
	}

	const quat q_rel = body.predicted_orientation * conjugate(body.angular_inertia_target);
	const vec3<angle> theta_diff = to_axis_angle(q_rel);

	const auto i_body = body.inv_inertia.inverse();
	const mat3<angular_stiffness> ang_inertia_hessian = i_body / h_squared / rad;
	const vec3<torque> g_ang_inertia = ang_inertia_hessian * theta_diff;

	const vec3<torque> g_ang = m_solve_state[body_idx].angular_gradient + g_ang_inertia;

	auto h_tt = m_solve_state[body_idx].angular_hessian + ang_inertia_hessian;
	h_tt[0][0] += m_config.angular_regularization;
	h_tt[1][1] += m_config.angular_regularization;
	h_tt[2][2] += m_config.angular_regularization;

	const auto& h_xt = m_solve_state[body_idx].hessian_xtheta;
	const auto h_tx = h_xt.transpose() * rad;

	const auto h_xx_inv = h_xx.inverse();
	const auto s = h_tt - h_tx * h_xx_inv * h_xt;
	const auto s_inv = s.inverse();

	auto delta_theta = -(s_inv * (g_ang - h_tx * (h_xx_inv * g_lin)));
	auto delta_x = -(h_xx_inv * (g_lin + h_xt * delta_theta));

	const auto lin_step = magnitude(delta_x);
	const auto ang_step = magnitude(delta_theta);

	if (lin_step > m_config.max_linear_step) {
		delta_x *= (m_config.max_linear_step / lin_step);
	}
	if (ang_step > m_config.max_angular_step) {
		delta_theta *= (m_config.max_angular_step / ang_step);
	}

	body.predicted_position += delta_x * omega;

	if (ang_step > radians(1e-7f)) {
		body.predicted_orientation = normalize(from_axis_angle_vector(vec3<angle>(delta_theta) * omega) * body.predicted_orientation);
	}

	return { lin_step, ang_step };
}

auto gse::vbd::solver::update_dual(const float alpha) -> step_delta {
	auto& contacts = m_graph.contact_constraints();
	const auto worker_count = std::max<std::size_t>(1, task::thread_count());
	static_vector<length, 64> per_worker_max;
	for (std::size_t i = 0; i < worker_count; ++i) {
		per_worker_max.push_back(length{});
	}

	task::coarse_parallel(contacts.size(), 512, [&, this](std::size_t ci) {
		auto& con = contacts[ci];
		const auto& body_a = m_bodies[con.body_a];
		const auto& body_b = m_bodies[con.body_b];

		const vec3<lever_arm> rAW = rotate_vector(body_a.predicted_orientation, con.local_anchor_a);
		const vec3<lever_arm> rBW = rotate_vector(body_b.predicted_orientation, con.local_anchor_b);
		const vec3<predicted_position> pA = body_a.predicted_position + rAW;
		const vec3<predicted_position> pB = body_b.predicted_position + rBW;
		const vec3<displacement> d = pA - pB;

		const vec3<gap> cn = {
			dot(con.normal, d) + m_config.collision_margin,
			dot(con.tangent_u, d),
			dot(con.tangent_v, d)
		};
		const vec3<gap> c = cn - con.c0 * alpha;

		con.lambda[0] = std::min<force>(con.penalty[0] * c[0] + con.lambda[0], force{});

		const force friction_bound = abs(con.lambda[0]) * con.friction_coeff;

		con.lambda[1] = std::clamp<force>(con.penalty[1] * c[1] + con.lambda[1], -friction_bound, friction_bound);
		con.lambda[2] = std::clamp<force>(con.penalty[2] * c[2] + con.lambda[2], -friction_bound, friction_bound);

		if (con.lambda[0] < force{}) {
			con.penalty[0] = std::min(con.penalty[0] + m_config.beta * abs(c[0]), m_config.penalty_max);
			const auto worker_idx = task::current_worker().value_or(0);
			per_worker_max[worker_idx] = std::max<length>(per_worker_max[worker_idx], abs(c[0]));
		}

		if (friction_bound > newtons(1e-10f)) {
			if (abs(con.lambda[1]) < friction_bound) {
				con.penalty[1] = std::min(con.penalty[1] + m_config.beta * abs(c[1]), m_config.penalty_max);
			}
			if (abs(con.lambda[2]) < friction_bound) {
				con.penalty[2] = std::min(con.penalty[2] + m_config.beta * abs(c[2]), m_config.penalty_max);
			}
		}
	},
						  trace_id<"vbd::contact_dual_update">());

	step_delta max_violation;
	for (const auto& m : per_worker_max) {
		max_violation.linear = std::max(max_violation.linear, m);
	}
	return max_violation;
}

auto gse::vbd::solver::accumulate_joint(const joint_constraint& constraint, const std::uint32_t body_idx, const time_squared h_squared, const time_step dt, const float alpha) -> void {
	const auto& body_a = m_bodies[constraint.body_a];
	const auto& body_b = m_bodies[constraint.body_b];
	const bool is_a = (body_idx == constraint.body_a);
	const float sign = is_a ? 1.f : -1.f;

	const vec3<lever_arm> r_aw = rotate_vector(body_a.predicted_orientation, constraint.local_anchor_a);
	const vec3<lever_arm> r_bw = rotate_vector(body_b.predicted_orientation, constraint.local_anchor_b);
	const vec3<predicted_position> p_a = body_a.predicted_position + r_aw;
	const vec3<predicted_position> p_b = body_b.predicted_position + r_bw;
	const vec3<displacement> d = p_a - p_b;

	const vec3<lever_arm> r = (is_a ? r_aw : r_bw) * sign;

	if (constraint.type == joint_type::distance) {
		const auto d_mag = magnitude(d);
		const vec3f d_hat = d_mag > meters(1e-7f) ? normalize(d) : axis_y;
		length c = (d_mag - constraint.target_distance) - constraint.pos_c0[0] * alpha;

		if (constraint.damping > 0.f) {
			const auto vel_a = body_a.velocity + cross(body_a.angular_velocity, r_aw) / rad;
			const auto vel_b = body_b.velocity + cross(body_b.angular_velocity, r_bw) / rad;
			const velocity v_rel = dot(d_hat, vel_a - vel_b);
			c += v_rel * dt * constraint.damping;
		}

		const force fi = constraint.pos_penalty[0] * c + constraint.pos_lambda[0];

		const vec3f j_lin = d_hat * sign;
		const vec3<lever_arm> j_ang = cross(r, d_hat);

		m_solve_state[body_idx].gradient += j_lin * fi;
		m_solve_state[body_idx].hessian += outer_product(j_lin, j_lin) * constraint.pos_penalty[0];

		if (m_bodies[body_idx].update_orientation) {
			m_solve_state[body_idx].angular_gradient += j_ang * fi;
			m_solve_state[body_idx].angular_hessian += outer_product(j_ang, j_ang) * constraint.pos_penalty[0] / rad;
			m_solve_state[body_idx].hessian_xtheta += outer_product(j_lin, j_ang) * constraint.pos_penalty[0] / rad;
			accumulate_geometric_stiffness(m_solve_state[body_idx], r, d_hat, abs(fi));
		}
	}
	else if (constraint.type == joint_type::fixed || constraint.type == joint_type::hinge) {
		constexpr std::array dirs = { axis_x, axis_y, axis_z };

		for (int k = 0; k < 3; ++k) {
			const length c = dot(dirs[k], d) - constraint.pos_c0[k] * alpha;
			const force fi = constraint.pos_penalty[k] * c + constraint.pos_lambda[k];

			const vec3f j_lin = dirs[k] * sign;
			const vec3<lever_arm> j_ang = cross(r, dirs[k]);

			m_solve_state[body_idx].gradient += j_lin * fi;
			m_solve_state[body_idx].hessian += outer_product(j_lin, j_lin) * constraint.pos_penalty[k];

			if (m_bodies[body_idx].update_orientation) {
				m_solve_state[body_idx].angular_gradient += j_ang * fi;
				m_solve_state[body_idx].angular_hessian += outer_product(j_ang, j_ang) * constraint.pos_penalty[k] / rad;
				m_solve_state[body_idx].hessian_xtheta += outer_product(j_lin, j_ang) * constraint.pos_penalty[k] / rad;
				accumulate_geometric_stiffness(m_solve_state[body_idx], r, dirs[k], abs(fi));
			}
		}

		if (m_bodies[body_idx].update_orientation) {
			const quat q_error = body_b.predicted_orientation * conjugate(body_a.predicted_orientation) * conjugate(constraint.rest_orientation);
			const vec3<angle> theta = to_axis_angle(q_error);

			if (constraint.type == joint_type::fixed) {
				for (int k = 0; k < 3; ++k) {
					const angle c_ang = theta[k] - constraint.ang_c0[k] * alpha;
					const torque f_ang = constraint.ang_penalty[k] * c_ang + constraint.ang_lambda[k];

					const vec3f j_ang = dirs[k] * (-sign);
					m_solve_state[body_idx].angular_gradient += j_ang * f_ang;
					m_solve_state[body_idx].angular_hessian += outer_product(j_ang, j_ang) * constraint.ang_penalty[k];
				}
			}
			else {
				const vec3f axis_a = rotate_vector(body_a.predicted_orientation, constraint.local_axis_a);
				const vec3f axis_b = rotate_vector(body_b.predicted_orientation, constraint.local_axis_b);
				const vec3f swing_error = cross(axis_a, axis_b);

				vec3f perp_u = cross(axis_a, axis_y);
				if (magnitude(perp_u) < 1e-6f) {
					perp_u = cross(axis_a, axis_x);
				}
				perp_u = normalize(perp_u);
				const vec3f perp_v = normalize(cross(axis_a, perp_u));

				const angle c_u = radians(dot(perp_u, swing_error)) - constraint.ang_c0[0] * alpha;
				const angle c_v = radians(dot(perp_v, swing_error)) - constraint.ang_c0[1] * alpha;

				const torque f_u = constraint.ang_penalty[0] * c_u + constraint.ang_lambda[0];
				const torque f_v = constraint.ang_penalty[1] * c_v + constraint.ang_lambda[1];

				const vec3f j_ang_u = perp_u * (-sign);
				const vec3f j_ang_v = perp_v * (-sign);

				m_solve_state[body_idx].angular_gradient += j_ang_u * f_u;
				m_solve_state[body_idx].angular_hessian += outer_product(j_ang_u, j_ang_u) * constraint.ang_penalty[0];

				m_solve_state[body_idx].angular_gradient += j_ang_v * f_v;
				m_solve_state[body_idx].angular_hessian += outer_product(j_ang_v, j_ang_v) * constraint.ang_penalty[1];

				if (constraint.limits_enabled) {
					const angle twist = dot(axis_a, theta);
					const angle lower = constraint.limit_lower;
					const angle upper = constraint.limit_upper;

					bool limit_active = false;
					angle c_limit = {};

					if (twist < lower) {
						c_limit = (twist - lower) - constraint.limit_c0 * alpha;
						limit_active = true;
					}
					else if (twist > upper) {
						c_limit = (twist - upper) - constraint.limit_c0 * alpha;
						limit_active = true;
					}

					if (limit_active) {
						const torque f_limit = constraint.limit_penalty * c_limit + constraint.limit_lambda;
						const torque f_clamped = (c_limit < angle{}) ? std::min(f_limit, torque{}) : std::max(f_limit, torque{});

						const vec3f j_limit = axis_a * (-sign);
						m_solve_state[body_idx].angular_gradient += j_limit * f_clamped;
						m_solve_state[body_idx].angular_hessian += outer_product(j_limit, j_limit) * constraint.limit_penalty;
					}
				}
			}
		}
	}
	else if (constraint.type == joint_type::slider) {
		const vec3f axis_w = normalize(rotate_vector(body_a.predicted_orientation, constraint.local_axis_a));
		vec3f perp0 = cross(axis_w, axis_y);
		if (magnitude(perp0) < 1e-6f) {
			perp0 = cross(axis_w, axis_x);
		}
		perp0 = normalize(perp0);
		const vec3f perp1 = normalize(cross(axis_w, perp0));

		const std::array perps = { perp0, perp1 };
		for (int k = 0; k < 2; ++k) {
			const length C = dot(perps[k], d) - constraint.pos_c0[k] * alpha;
			const force fi = constraint.pos_penalty[k] * C + constraint.pos_lambda[k];

			const vec3f j_lin = perps[k] * sign;
			const vec3<lever_arm> j_ang = cross(r, perps[k]);

			m_solve_state[body_idx].gradient += j_lin * fi;
			m_solve_state[body_idx].hessian += outer_product(j_lin, j_lin) * constraint.pos_penalty[k];

			if (m_bodies[body_idx].update_orientation) {
				m_solve_state[body_idx].angular_gradient += j_ang * fi;
				m_solve_state[body_idx].angular_hessian += outer_product(j_ang, j_ang) * constraint.pos_penalty[k] / rad;
				m_solve_state[body_idx].hessian_xtheta += outer_product(j_lin, j_ang) * constraint.pos_penalty[k] / rad;
				accumulate_geometric_stiffness(m_solve_state[body_idx], r, perps[k], abs(fi));
			}
		}

		if (m_bodies[body_idx].update_orientation) {
			const quat q_error = body_b.predicted_orientation * conjugate(body_a.predicted_orientation) * conjugate(constraint.rest_orientation);
			const vec3<angle> theta = to_axis_angle(q_error);
			constexpr std::array dirs = { axis_x, axis_y, axis_z };

			for (int k = 0; k < 3; ++k) {
				const angle c_ang = theta[k] - constraint.ang_c0[k] * alpha;
				const torque f_ang = constraint.ang_penalty[k] * c_ang + constraint.ang_lambda[k];

				const vec3f j_ang = dirs[k] * (-sign);
				m_solve_state[body_idx].angular_gradient += j_ang * f_ang;
				m_solve_state[body_idx].angular_hessian += outer_product(j_ang, j_ang) * constraint.ang_penalty[k];
			}
		}
	}
}

auto gse::vbd::solver::update_joint_dual(const time_squared h_squared) -> step_delta {
	auto& joints = m_graph.joint_constraints();
	const auto worker_count = std::max<std::size_t>(1, task::thread_count());
	static_vector<step_delta, 64> per_worker_max;
	for (std::size_t i = 0; i < worker_count; ++i) {
		per_worker_max.push_back({});
	}

	task::coarse_parallel(joints.size(), 256, [&, this](std::size_t ji) {
		auto& j = joints[ji];
		const auto& body_a = m_bodies[j.body_a];
		const auto& body_b = m_bodies[j.body_b];

		if ((body_a.locked || body_a.sleeping()) && (body_b.locked || body_b.sleeping())) {
			return;
		}

		const auto worker_idx = task::current_worker().value_or(0);
		auto track_linear = [&](const length v) {
			per_worker_max[worker_idx].linear = std::max(per_worker_max[worker_idx].linear, abs(v));
		};
		auto track_angular = [&](const angle v) {
			per_worker_max[worker_idx].angular = std::max(per_worker_max[worker_idx].angular, abs(v));
		};

		const vec3<lever_arm> r_aw = rotate_vector(body_a.predicted_orientation, j.local_anchor_a);
		const vec3<lever_arm> r_bw = rotate_vector(body_b.predicted_orientation, j.local_anchor_b);
		const vec3<predicted_position> p_a = body_a.predicted_position + r_aw;
		const vec3<predicted_position> p_b = body_b.predicted_position + r_bw;
		const vec3<displacement> d = p_a - p_b;

		if (j.type == joint_type::distance) {
			const length C = (magnitude(d) - j.target_distance) - j.pos_c0[0];
			j.pos_lambda[0] = j.pos_penalty[0] * C + j.pos_lambda[0];
			const stiffness penalty_cap = (j.compliance > inverse_mass{})
				? std::min<stiffness>(1.f / (j.compliance * h_squared), m_config.penalty_max)
				: m_config.penalty_max;
			j.pos_penalty[0] = std::min(j.pos_penalty[0] + m_config.beta * abs(C), penalty_cap);
			track_linear(C);
		}
		else if (j.type == joint_type::fixed || j.type == joint_type::hinge) {
			for (int k = 0; k < 3; ++k) {
				constexpr std::array dirs = { axis_x, axis_y, axis_z };
				const length c = dot(dirs[k], d) - j.pos_c0[k];
				j.pos_lambda[k] = j.pos_penalty[k] * c + j.pos_lambda[k];
				j.pos_penalty[k] = std::min(j.pos_penalty[k] + m_config.beta * abs(c), m_config.penalty_max);
				track_linear(c);
			}

			const quat q_error = body_b.predicted_orientation * conjugate(body_a.predicted_orientation) * conjugate(j.rest_orientation);
			const vec3<angle> theta = to_axis_angle(q_error);

			if (j.type == joint_type::fixed) {
				for (int k = 0; k < 3; ++k) {
					const angle c_ang = theta[k] - j.ang_c0[k];
					j.ang_lambda[k] = j.ang_penalty[k] * c_ang + j.ang_lambda[k];
					j.ang_penalty[k] = std::min(j.ang_penalty[k] + m_config.ang_beta * abs(c_ang), m_config.ang_penalty_max);
					track_angular(c_ang);
				}
			}
			else {
				const vec3f axis_a = rotate_vector(body_a.predicted_orientation, j.local_axis_a);
				const vec3f axis_b = rotate_vector(body_b.predicted_orientation, j.local_axis_b);
				const vec3f swing_error = cross(axis_a, axis_b);

				vec3f perp_u = cross(axis_a, axis_y);
				if (magnitude(perp_u) < 1e-6f) {
					perp_u = cross(axis_a, axis_x);
				}
				perp_u = normalize(perp_u);
				const vec3f perp_v = normalize(cross(axis_a, perp_u));

				const angle c_u = radians(dot(perp_u, swing_error)) - j.ang_c0[0];
				const angle c_v = radians(dot(perp_v, swing_error)) - j.ang_c0[1];

				j.ang_lambda[0] = j.ang_penalty[0] * c_u + j.ang_lambda[0];
				j.ang_penalty[0] = std::min(j.ang_penalty[0] + m_config.ang_beta * abs(c_u), m_config.ang_penalty_max);
				track_angular(c_u);

				j.ang_lambda[1] = j.ang_penalty[1] * c_v + j.ang_lambda[1];
				j.ang_penalty[1] = std::min(j.ang_penalty[1] + m_config.ang_beta * abs(c_v), m_config.ang_penalty_max);
				track_angular(c_v);

				if (j.limits_enabled) {
					const angle twist = dot(axis_a, theta);
					const angle lower = j.limit_lower;
					const angle upper = j.limit_upper;

					if (twist < lower) {
						const angle c_limit = (twist - lower) - j.limit_c0;
						j.limit_lambda = std::min<torque>(j.limit_penalty * c_limit + j.limit_lambda, torque{});
						j.limit_penalty = std::min(j.limit_penalty + m_config.ang_beta * abs(c_limit), m_config.ang_penalty_max);
						track_angular(c_limit);
					}
					else if (twist > upper) {
						const angle c_limit = (twist - upper) - j.limit_c0;
						j.limit_lambda = std::max<torque>(j.limit_penalty * c_limit + j.limit_lambda, torque{});
						j.limit_penalty = std::min(j.limit_penalty + m_config.ang_beta * abs(c_limit), m_config.ang_penalty_max);
						track_angular(c_limit);
					}
				}
			}
		}
		else if (j.type == joint_type::slider) {
			const vec3f axis_w = normalize(rotate_vector(body_a.predicted_orientation, j.local_axis_a));
			vec3f perp0 = cross(axis_w, axis_y);
			if (magnitude(perp0) < 1e-6f) {
				perp0 = cross(axis_w, axis_x);
			}
			perp0 = normalize(perp0);
			const vec3f perp1 = normalize(cross(axis_w, perp0));

			const std::array perps = { perp0, perp1 };
			for (int k = 0; k < 2; ++k) {
				const length c = dot(perps[k], d) - j.pos_c0[k];
				j.pos_lambda[k] = j.pos_penalty[k] * c + j.pos_lambda[k];
				j.pos_penalty[k] = std::min(j.pos_penalty[k] + m_config.beta * abs(c), m_config.penalty_max);
				track_linear(c);
			}

			const quat q_error = body_b.predicted_orientation * conjugate(body_a.predicted_orientation) * conjugate(j.rest_orientation);
			const vec3<angle> theta = to_axis_angle(q_error);

			for (int k = 0; k < 3; ++k) {
				const angle c_ang = theta[k] - j.ang_c0[k];
				j.ang_lambda[k] = j.ang_penalty[k] * c_ang + j.ang_lambda[k];
				j.ang_penalty[k] = std::min(j.ang_penalty[k] + m_config.ang_beta * abs(c_ang), m_config.ang_penalty_max);
				track_angular(c_ang);
			}
		}
	},
						  trace_id<"vbd::joint_dual_update">());

	step_delta result;
	for (const auto& m : per_worker_max) {
		result.linear = std::max(result.linear, m.linear);
		result.angular = std::max(result.angular, m.angular);
	}
	return result;
}

auto gse::vbd::accumulate_geometric_stiffness(body_solve_state& state, const vec3<lever_arm>& r, const vec3f& dir, const force abs_force) -> void {
	length rd{};
	for (int i = 0; i < 3; ++i) {
		rd += r[i] * dir[i];
	}
	for (int j = 0; j < 3; ++j) {
		length col_norm{};
		for (int i = 0; i < 3; ++i) {
			length h = r[i] * dir[j];
			if (i == j) {
				h -= rd;
			}
			col_norm += abs(h);
		}
		state.angular_hessian[j][j] += col_norm * abs_force / rad;
	}
}

auto gse::vbd::contact_effective_mass(const body_state& body_a, const body_state& body_b, const vec3<lever_arm>& r_aw, const vec3<lever_arm>& r_bw, const vec3f& dir) -> mass {
	inverse_mass inv_mass_sum =
		body_a.inverse_mass() +
		body_b.inverse_mass();

	if (body_a.update_orientation && !body_a.locked) {
		const auto ang_j_a = cross(r_aw, dir);
		inv_mass_sum += dot(cross(body_a.inv_inertia * ang_j_a, r_aw), dir);
	}

	if (body_b.update_orientation && !body_b.locked) {
		const auto ang_j_b = cross(r_bw, dir);
		inv_mass_sum += dot(cross(body_b.inv_inertia * ang_j_b, r_bw), dir);
	}

	if (!gse::isfinite(inv_mass_sum) || inv_mass_sum <= per_kilograms(1e-10f)) {
		return {};
	}

	return 1.f / inv_mass_sum;
}

auto gse::vbd::warm_start_joint(joint_constraint& j, const body_state& ba, const body_state& bb, const time_squared h_squared, const solver_config& cfg) -> void {
	const vec3<lever_arm> r_aw = rotate_vector(ba.orientation, j.local_anchor_a);
	const vec3<lever_arm> r_bw = rotate_vector(bb.orientation, j.local_anchor_b);

	for (int k = 0; k < 3; ++k) {
		j.pos_lambda[k] *= cfg.gamma;
	}
	for (int k = 0; k < 3; ++k) {
		j.ang_lambda[k] *= cfg.gamma;
	}
	if (j.limits_enabled) {
		j.limit_lambda *= cfg.gamma;
	}

	constexpr std::array dirs = { axis_x, axis_y, axis_z };

	int num_pos_rows = 3;
	if (j.type == joint_type::distance) {
		num_pos_rows = 1;
	}
	else if (j.type == joint_type::slider) {
		num_pos_rows = 2;
	}

	for (int k = 0; k < num_pos_rows; ++k) {
		vec3f dir;
		if (j.type == joint_type::distance) {
			const vec3<displacement> d = (ba.position + r_aw) - (bb.position + r_bw);
			dir = magnitude(d) > meters(1e-7f) ? normalize(d) : axis_y;
		}
		else if (j.type == joint_type::slider) {
			const vec3f axis_w = rotate_vector(ba.orientation, j.local_axis_a);
			vec3f perp0 = cross(axis_w, axis_y);
			if (magnitude(perp0) < 1e-6f) {
				perp0 = cross(axis_w, axis_x);
			}
			perp0 = normalize(perp0);
			dir = k == 0 ? perp0 : normalize(cross(axis_w, perp0));
		}
		else {
			dir = dirs[k];
		}
		const stiffness eff = contact_effective_mass(ba, bb, r_aw, r_bw, dir) / h_squared;
		const stiffness pos_penalty_cap = (j.compliance > inverse_mass{})
			? std::min<stiffness>(1.f / (j.compliance * h_squared), cfg.penalty_max)
			: cfg.penalty_max;
		j.pos_penalty[k] = std::clamp(
			std::max(j.pos_penalty[k] * cfg.gamma, eff),
			cfg.penalty_min,
			pos_penalty_cap
		);
	}

	int num_ang_rows = 3;
	if (j.type == joint_type::distance) {
		num_ang_rows = 0;
	}
	else if (j.type == joint_type::hinge) {
		num_ang_rows = 2;
	}

	for (int k = 0; k < num_ang_rows; ++k) {
		vec3f ang_dir;
		if (j.type == joint_type::hinge) {
			const vec3f axis_a = rotate_vector(ba.orientation, j.local_axis_a);
			vec3f perp_u = cross(axis_a, axis_y);
			if (magnitude(perp_u) < 1e-6f) {
				perp_u = cross(axis_a, axis_x);
			}
			perp_u = normalize(perp_u);
			ang_dir = k == 0 ? perp_u : normalize(cross(axis_a, perp_u));
		}
		else {
			ang_dir = dirs[k];
		}

		inverse_inertia inv_i_sum{};
		if (!ba.locked) {
			inv_i_sum += dot(ang_dir, ba.inv_inertia * ang_dir);
		}
		if (!bb.locked) {
			inv_i_sum += dot(ang_dir, bb.inv_inertia * ang_dir);
		}

		const angular_stiffness eff_ang = inv_i_sum > per_kilogram_meter_squared(1e-10f)
			? 1.f / inv_i_sum / h_squared / rad
			: cfg.ang_penalty_min;

		j.ang_penalty[k] = std::clamp(
			std::max(j.ang_penalty[k] * cfg.gamma, eff_ang),
			cfg.ang_penalty_min,
			cfg.ang_penalty_max
		);
	}

	if (j.limits_enabled) {
		const vec3f limit_axis = rotate_vector(ba.orientation, j.local_axis_a);
		inverse_inertia inv_i_sum{};
		if (!ba.locked) {
			inv_i_sum += dot(limit_axis, ba.inv_inertia * limit_axis);
		}
		if (!bb.locked) {
			inv_i_sum += dot(limit_axis, bb.inv_inertia * limit_axis);
		}

		const angular_stiffness eff_limit = inv_i_sum > per_kilogram_meter_squared(1e-10f)
			? 1.f / inv_i_sum / h_squared / rad
			: cfg.ang_penalty_min;

		j.limit_penalty = std::clamp(
			std::max(j.limit_penalty * cfg.gamma, eff_limit),
			cfg.ang_penalty_min,
			cfg.ang_penalty_max
		);
	}
}

auto gse::vbd::compute_joint_c0(joint_constraint& j, const body_state& ba, const body_state& bb) -> void {
	const vec3<lever_arm> r_aw = rotate_vector(ba.orientation, j.local_anchor_a);
	const vec3<lever_arm> r_bw = rotate_vector(bb.orientation, j.local_anchor_b);
	const vec3<displacement> d = (ba.position + r_aw) - (bb.position + r_bw);

	if (j.type == joint_type::distance) {
		j.pos_c0[0] = magnitude(d) - j.target_distance;
		return;
	}

	if (j.type == joint_type::fixed || j.type == joint_type::hinge) {
		constexpr std::array dirs = { axis_x, axis_y, axis_z };
		for (int k = 0; k < 3; ++k) {
			j.pos_c0[k] = dot(dirs[k], d);
		}

		const quat q_error = bb.orientation * conjugate(ba.orientation) * conjugate(j.rest_orientation);
		const vec3<angle> theta = to_axis_angle(q_error);

		if (j.type == joint_type::fixed) {
			for (int k = 0; k < 3; ++k) {
				j.ang_c0[k] = theta[k];
			}
			return;
		}

		const vec3f axis_a = rotate_vector(ba.orientation, j.local_axis_a);
		const vec3f axis_b = rotate_vector(bb.orientation, j.local_axis_b);
		const vec3f swing_error = cross(axis_a, axis_b);

		vec3f perp_u = cross(axis_a, axis_y);
		if (magnitude(perp_u) < 1e-6f) {
			perp_u = cross(axis_a, axis_x);
		}
		perp_u = normalize(perp_u);
		const vec3f perp_v = normalize(cross(axis_a, perp_u));
		j.ang_c0[0] = radians(dot(perp_u, swing_error));
		j.ang_c0[1] = radians(dot(perp_v, swing_error));

		if (j.limits_enabled) {
			const angle twist_angle = dot(axis_a, theta);
			if (twist_angle < j.limit_lower) {
				j.limit_c0 = twist_angle - j.limit_lower;
			}
			else if (twist_angle > j.limit_upper) {
				j.limit_c0 = twist_angle - j.limit_upper;
			}
			else {
				j.limit_c0 = angle{};
			}
		}
		return;
	}

	if (j.type == joint_type::slider) {
		const vec3f axis_w = normalize(rotate_vector(ba.orientation, j.local_axis_a));
		vec3f perp0 = cross(axis_w, axis_y);
		if (magnitude(perp0) < 1e-6f) {
			perp0 = cross(axis_w, axis_x);
		}
		perp0 = normalize(perp0);
		const vec3f perp1 = normalize(cross(axis_w, perp0));
		j.pos_c0[0] = dot(perp0, d);
		j.pos_c0[1] = dot(perp1, d);

		const auto slider_theta = to_axis_angle(bb.orientation * conjugate(ba.orientation) * conjugate(j.rest_orientation));
		for (int k = 0; k < 3; ++k) {
			j.ang_c0[k] = slider_theta[k];
		}
	}
}
