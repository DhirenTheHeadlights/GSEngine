export module gse.physics:vbd_solver;

import std;

import gse.math;
import gse.utility;
import gse.log;
import :vbd_constraints;
import :vbd_constraint_graph;
import :vbd_contact_cache;
import :motion_component;

export namespace gse::vbd {
	using time_step = time_t<float, seconds>;

	struct solver_config {
		std::uint32_t iterations = 10;
		float alpha = 0.99f;
		float beta = 100000.f;
		float gamma = 0.99f;
		bool post_stabilize = true;
		float penalty_min = 1.0f;
		float penalty_max = 1e9f;
		float collision_margin = 0.0005f;
		float stick_threshold = 0.01f;
		float friction_coefficient = 0.6f;
		float velocity_sleep_threshold = 0.001f;
		length speculative_margin = meters(0.02f);
	};

	class solver {
	public:
		auto configure(
			const solver_config& cfg
		) -> void;

		auto config(
		) const -> const solver_config&;

		auto begin_frame(
			const std::vector<body_state>& bodies,
			contact_cache& cache
		) -> void;

		auto add_contact_constraint(
			const contact_constraint& c
		) -> void;

		auto add_motor_constraint(
			const velocity_motor_constraint& m
		) -> void;

		auto solve(
			time_step dt
		) -> void;

		auto end_frame(
			std::vector<body_state>& bodies,
			contact_cache& cache
		) -> void;

		auto body_states(
		) -> std::vector<body_state>&;

		auto body_states(
		) const -> std::span<const body_state>;

		auto graph(this auto&& self) -> auto& { return self.m_graph; }
	private:
		auto accumulate_contact(
			const contact_constraint& c,
			std::uint32_t body_idx,
			float h_squared,
			float alpha
		) -> void;

		auto accumulate_motor(
			const velocity_motor_constraint& m,
			float h_squared
		) -> void;

		auto perform_newton_step(
			std::uint32_t body_idx,
			float h_squared
		) -> void;

		auto update_dual(
			float alpha
		) -> void;

		solver_config m_config;
		constraint_graph m_graph;
		std::vector<body_state> m_bodies;
		std::vector<body_solve_state> m_solve_state;

		static constexpr std::uint32_t no_motor = std::numeric_limits<std::uint32_t>::max();
		std::vector<std::uint32_t> m_body_motor_index;
		std::vector<bool> m_body_in_color_group;

		std::vector<vec3<velocity>> m_prev_velocity;
		std::vector<float> m_accel_weight;
	};
}

namespace gse::vbd {
	auto to_unitless(const physics::inv_inertia_mat& m) -> mat3 {
		return mat3{ m.data };
	}
}

auto gse::vbd::solver::configure(const solver_config& cfg) -> void {
	m_config = cfg;
}

auto gse::vbd::solver::config() const -> const solver_config& {
	return m_config;
}

auto gse::vbd::solver::begin_frame(const std::vector<body_state>& bodies, contact_cache& cache) -> void {
	m_bodies = bodies;
	m_solve_state.resize(m_bodies.size());
	m_graph.clear();

	cache.age_and_prune();
}

auto gse::vbd::solver::add_contact_constraint(const contact_constraint& c) -> void {
	m_graph.add_contact(c);
}

auto gse::vbd::solver::add_motor_constraint(const velocity_motor_constraint& m) -> void {
	m_graph.add_motor(m);
}

auto gse::vbd::solver::solve(const time_step dt) -> void {
	const auto num_bodies = static_cast<std::uint32_t>(m_bodies.size());
	const float dt_s = dt.as<seconds>();
	const float h_squared = dt_s * dt_s;

	std::vector<bool> locked(num_bodies);
	for (std::uint32_t i = 0; i < num_bodies; ++i) {
		locked[i] = m_bodies[i].locked;
	}
	m_graph.compute_coloring(num_bodies, locked);

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

	for (auto& c : m_graph.contact_constraints()) {
		const auto& ba = m_bodies[c.body_a];
		const auto& bb = m_bodies[c.body_b];
		const vec3<length> rAW = rotate_vector(ba.orientation, c.r_a);
		const vec3<length> rBW = rotate_vector(bb.orientation, c.r_b);
		const vec3<length> pA = ba.position + rAW;
		const vec3<length> pB = bb.position + rBW;
		const unitless::vec3 d = (pA - pB).as<meters>();

		c.C0[0] = dot(c.normal, d) + m_config.collision_margin;
		c.C0[1] = dot(c.tangent_u, d);
		c.C0[2] = dot(c.tangent_v, d);
	}

	for (auto& c : m_graph.contact_constraints()) {
		const float floor = std::max(c.penalty_floor, m_config.penalty_min);
		for (int i = 0; i < 3; i++) {
			if (m_config.post_stabilize) {
				c.penalty[i] = std::clamp(c.penalty[i] * m_config.gamma, floor, m_config.penalty_max);
			}
			else {
				c.lambda[i] *= m_config.alpha * m_config.gamma;
				c.penalty[i] = std::clamp(c.penalty[i] * m_config.gamma, floor, m_config.penalty_max);
			}
		}
	}

	constexpr float gravity_mag = 9.8f;
	constexpr vec3<acceleration> gravity = { 0.f, -gravity_mag, 0.f };

	if (m_prev_velocity.size() != m_bodies.size()) {
		m_prev_velocity.assign(m_bodies.size(), vec3<velocity>{});
		m_accel_weight.assign(m_bodies.size(), 0.f);
	}

	for (std::uint32_t i = 0; i < num_bodies; ++i) {
		auto& body = m_bodies[i];
		if (body.locked || body.sleeping()) {
			m_accel_weight[i] = 0.f;
			m_prev_velocity[i] = body.body_velocity;
			continue;
		}

		const float delta_vy = (body.body_velocity.y() - m_prev_velocity[i].y()).as<meters_per_second>();
		const float accel_y = delta_vy / dt_s;
		float aw = std::clamp(-accel_y / gravity_mag, 0.f, 1.f);
		if (!std::isfinite(aw)) aw = 0.f;
		m_accel_weight[i] = aw;

		m_prev_velocity[i] = body.body_velocity;
	}

	for (std::uint32_t i = 0; i < num_bodies; ++i) {
		auto& body = m_bodies[i];
		if (body.locked || body.sleeping()) {
			body.predicted_position = body.position;
			body.inertia_target = body.position;
			body.predicted_orientation = body.orientation;
			body.angular_inertia_target = body.orientation;
			body.motor_target = body.position;
			continue;
		}

		if (float w = magnitude(body.body_angular_velocity).as<radians_per_second>(); w > 50.f) {
			body.body_angular_velocity *= (50.f / w);
		}

		body.inertia_target = body.position + body.body_velocity * dt;
		if (body.affected_by_gravity)
			body.inertia_target += gravity * dt * dt;

		body.initial_position = body.position;
		body.initial_orientation = body.orientation;

		const float aw = m_accel_weight[i];
		body.predicted_position = body.position + body.body_velocity * dt;
		if (body.affected_by_gravity)
			body.predicted_position += gravity * dt * dt * aw;

		if (body.update_orientation) {
			const unitless::vec3 w = body.body_angular_velocity.as<radians_per_second>();
			if (const float omega = magnitude(w); omega > 1e-7f) {
				const float angle = omega * dt_s;
				const unitless::vec3 axis = w / omega;
				const float half = angle * 0.5f;
				const quat dq(std::cos(half), std::sin(half) * axis.x(), std::sin(half) * axis.y(), std::sin(half) * axis.z());
				body.predicted_orientation = normalize(dq * body.orientation);
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
	}

	for (const auto& motor : m_graph.motor_constraints()) {
		auto& body = m_bodies[motor.body_index];
		if (body.locked || body.sleeping()) continue;

		if (body.sleeping() && magnitude(motor.target_velocity).as<meters_per_second>() > 0.01f) {
			body.sleep_counter = 0;
		}

		const vec3<length> target = body.initial_position + motor.target_velocity * dt;
		if (motor.horizontal_only) {
			body.motor_target.x() = target.x();
			body.motor_target.z() = target.z();
		}
		else {
			body.motor_target = target;
		}
	}

	for (const auto& c : m_graph.contact_constraints()) {
		auto& a = m_bodies[c.body_a];
		auto& b = m_bodies[c.body_b];
		if (a.sleeping() && !b.locked && !b.sleeping()) a.sleep_counter = 0;
		if (b.sleeping() && !a.locked && !a.sleeping()) b.sleep_counter = 0;
	}

	const auto& contacts = m_graph.contact_constraints();
	const auto& motors = m_graph.motor_constraints();

	const int total_iterations = static_cast<int>(m_config.iterations) + (m_config.post_stabilize ? 1 : 0);

	for (int it = 0; it < total_iterations; ++it) {
		float current_alpha = m_config.alpha;
		if (m_config.post_stabilize)
			current_alpha = it < static_cast<int>(m_config.iterations) ? 1.0f : 0.0f;

		for (const auto& body_color : m_graph.body_colors()) {
			for (const auto bi : body_color) {
				m_solve_state[bi] = {};
				for (const auto ci : m_graph.body_contact_indices(bi)) {
					accumulate_contact(contacts[ci], bi, h_squared, current_alpha);
				}
				if (const auto mi = m_body_motor_index[bi]; mi != no_motor) {
					accumulate_motor(motors[mi], h_squared);
				}
			}

			for (const auto bi : body_color) {
				perform_newton_step(bi, h_squared);
			}
		}

		for (const auto& motor : motors) {
			if (!m_body_in_color_group[motor.body_index]) {
				m_solve_state[motor.body_index] = {};
				accumulate_motor(motor, h_squared);
				perform_newton_step(motor.body_index, h_squared);
			}
		}

		for (std::uint32_t i = 0; i < num_bodies; ++i) {
			if (m_bodies[i].locked || m_bodies[i].sleeping()) continue;
			if (m_body_in_color_group[i]) continue;
			if (m_body_motor_index[i] != no_motor) continue;
			m_solve_state[i] = {};
			perform_newton_step(i, h_squared);
		}

		if (it < static_cast<int>(m_config.iterations)) {
			update_dual(current_alpha);
		}

		if (it == static_cast<int>(m_config.iterations) - 1) {
			for (auto& body : m_bodies) {
				if (!body.locked && !body.sleeping() && body.mass_value.as<kilograms>() > 0.f) {
					body.body_velocity = (body.predicted_position - body.initial_position) / dt;

					if (body.update_orientation) {
						quat dq = body.predicted_orientation * conjugate(body.initial_orientation);
						float qs = dq.s();
						unitless::vec3 qv = dq.imaginary_part();
						if (qs < 0.f) { qs = -qs; qv = -qv; }

						if (qs < 0.9999f) {
							const float angle = 2.f * std::acos(std::clamp(qs, 0.f, 1.f));
							const float sin_half = std::sqrt(1.f - qs * qs);
							if (sin_half > 1e-6f)
								body.body_angular_velocity = (qv / sin_half) * radians_per_second(angle / dt_s);
							else
								body.body_angular_velocity = qv * radians_per_second(2.f / dt_s);
						}
						else {
							body.body_angular_velocity = qv * radians_per_second(2.f / dt_s);
						}
					}
				}
			}
		}
	}

	for (auto& body : m_bodies) {
		if (body.locked || body.sleeping()) continue;
		body.position = body.predicted_position;
		if (body.update_orientation)
			body.orientation = body.predicted_orientation;
	}
}

auto gse::vbd::solver::end_frame(std::vector<body_state>& bodies, contact_cache& cache) -> void {
	for (const auto& c : m_graph.contact_constraints()) {
		const bool sticking =
			c.lambda[0] < -1e-3f &&
			std::abs(c.C0[1]) < m_config.stick_threshold &&
			std::abs(c.C0[2]) < m_config.stick_threshold;

		cache.store(c.body_a, c.body_b, c.feature, cached_lambda{
			.lambda = { c.lambda[0], c.lambda[1], c.lambda[2] },
			.penalty = { c.penalty[0], c.penalty[1], c.penalty[2] },
			.normal = c.normal,
			.local_anchor_a = c.r_a,
			.local_anchor_b = c.r_b,
			.sticking = sticking,
			.age = 0
		});
	}

	bodies = m_bodies;
}

auto gse::vbd::solver::body_states() -> std::vector<body_state>& {
	return m_bodies;
}

auto gse::vbd::solver::body_states() const -> std::span<const body_state> {
	return m_bodies;
}

auto gse::vbd::solver::accumulate_contact(const contact_constraint& c, const std::uint32_t body_idx, const float h_squared, const float alpha) -> void {
	const auto& body_a = m_bodies[c.body_a];
	const auto& body_b = m_bodies[c.body_b];
	const bool is_a = (body_idx == c.body_a);

	const vec3<length> rAW = rotate_vector(body_a.predicted_orientation, c.r_a);
	const vec3<length> rBW = rotate_vector(body_b.predicted_orientation, c.r_b);
	const vec3<length> pA = body_a.predicted_position + rAW;
	const vec3<length> pB = body_b.predicted_position + rBW;
	const unitless::vec3 d = (pA - pB).as<meters>();

	float Cn[3];
	Cn[0] = dot(c.normal, d) + m_config.collision_margin;
	Cn[1] = dot(c.tangent_u, d);
	Cn[2] = dot(c.tangent_v, d);

	float C[3];
	for (int i = 0; i < 3; i++)
		C[i] = Cn[i] - c.C0[i] * alpha;

	const float friction_bound = std::abs(c.lambda[0]) * c.friction_coeff;

	float f[3];
	f[0] = std::min(c.penalty[0] * C[0] + c.lambda[0], 0.f);
	f[1] = std::clamp(c.penalty[1] * C[1] + c.lambda[1], -friction_bound, friction_bound);
	f[2] = std::clamp(c.penalty[2] * C[2] + c.lambda[2], -friction_bound, friction_bound);

	const float sign = is_a ? 1.f : -1.f;
	const unitless::vec3 r = (is_a ? rAW : rBW).as<meters>();
	const unitless::vec3 dirs[3] = { c.normal, c.tangent_u, c.tangent_v };

	for (int i = 0; i < 3; i++) {
		if (std::abs(f[i]) < 1e-12f && c.penalty[i] < 1e-6f) continue;

		const unitless::vec3 J_lin = dirs[i] * sign;
		const unitless::vec3 J_ang = cross(r, dirs[i]) * sign;

		m_solve_state[body_idx].gradient += J_lin * f[i];
		m_solve_state[body_idx].hessian += outer_product(J_lin, J_lin) * c.penalty[i];

		if (m_bodies[body_idx].update_orientation) {
			m_solve_state[body_idx].angular_gradient += J_ang * f[i];
			m_solve_state[body_idx].angular_hessian += outer_product(J_ang, J_ang) * c.penalty[i];
			m_solve_state[body_idx].hessian_xtheta += outer_product(J_lin, J_ang) * c.penalty[i];
		}
	}
}

auto gse::vbd::solver::accumulate_motor(const velocity_motor_constraint& m, const float h_squared) -> void {
	const auto& body = m_bodies[m.body_index];
	if (body.locked || body.sleeping()) return;

	const float alpha = std::max(m.compliance, 1e-6f);
	const float mass = body.mass_value.as<kilograms>();
	const float inertia_weight = mass / h_squared;
	const float stiffness = inertia_weight / alpha;

	const unitless::vec3 diff = (body.predicted_position - body.motor_target).as<meters>();

	unitless::vec3 masked_diff = diff;
	if (m.horizontal_only) {
		masked_diff[1] = 0.f;
	}

	unitless::vec3 motor_gradient = masked_diff * stiffness;
	if (const float grad_mag = magnitude(motor_gradient); grad_mag > m.max_force.as<newtons>()) {
		motor_gradient *= (m.max_force.as<newtons>() / grad_mag);
	}

	const float grad_before = magnitude(motor_gradient);

	const auto& contacts = m_graph.contact_constraints();
	for (const auto ci : m_graph.body_contact_indices(m.body_index)) {
		const auto& c = contacts[ci];
		const auto other = (c.body_a == m.body_index) ? c.body_b : c.body_a;
		if (m_bodies[other].locked) continue;

		const unitless::vec3 push_dir = (c.body_a == m.body_index) ? c.normal : -c.normal;
		if (const float proj = dot(motor_gradient, push_dir); proj < 0.f) {
			motor_gradient -= push_dir * proj;
		}
	}

	const float grad_after = magnitude(motor_gradient);
	const float motor_scale = (grad_before > 1e-10f) ? (grad_after / grad_before) : 0.f;

	m_solve_state[m.body_index].gradient += motor_gradient;

	mat3 motor_hessian(0.f);
	motor_hessian[0][0] = stiffness * motor_scale;
	motor_hessian[1][1] = m.horizontal_only ? 0.f : stiffness * motor_scale;
	motor_hessian[2][2] = stiffness * motor_scale;

	m_solve_state[m.body_index].hessian += motor_hessian;
}

auto gse::vbd::solver::perform_newton_step(const std::uint32_t body_idx, const float h_squared) -> void {
	auto& body = m_bodies[body_idx];

	if (body.locked || body.sleeping()) return;
	if (body.inverse_mass().as<per_kilograms>() < 1e-10f) return;

	const float mass = body.mass_value.as<kilograms>();
	const float inertia_weight = mass / h_squared;

	const unitless::vec3 x_diff = (body.predicted_position - body.inertia_target).as<meters>();
	const unitless::vec3 g_lin = x_diff * inertia_weight + m_solve_state[body_idx].gradient;

	constexpr float reg = 1e-6f;
	mat3 h_xx = mat3(inertia_weight) + m_solve_state[body_idx].hessian;
	h_xx[0][0] += reg;
	h_xx[1][1] += reg;
	h_xx[2][2] += reg;

	if (!body.update_orientation) {
		const mat3 inv_h = h_xx.inverse();
		unitless::vec3 delta_x = -(inv_h * g_lin);

		constexpr float max_step = 0.5f;
		if (const float step_size = magnitude(delta_x); step_size > max_step) {
			delta_x *= (max_step / step_size);
		}

		body.predicted_position += delta_x * meters(1.f);
		return;
	}

	quat q_rel = body.predicted_orientation * conjugate(body.angular_inertia_target);
	float q_s = q_rel.s();
	unitless::vec3 q_v = q_rel.imaginary_part();
	if (q_s < 0.f) {
		q_s = -q_s;
		q_v = -q_v;
	}

	unitless::vec3 theta_diff;
	if (q_s < 0.9999f) {
		const float angle = 2.f * std::acos(std::clamp(q_s, 0.f, 1.f));
		const float sin_half = std::sqrt(1.f - q_s * q_s);
		theta_diff = (sin_half > 1e-6f) ? q_v * (angle / sin_half) : q_v * 2.f;
	}
	else {
		theta_diff = q_v * 2.f;
	}

	const mat3 i_inv = to_unitless(body.inv_inertia);
	const mat3 i = i_inv.inverse();
	const float ang_scale = 1.f / h_squared;

	const unitless::vec3 g_ang = i * theta_diff * ang_scale + m_solve_state[body_idx].angular_gradient;

	mat3 h_tt = i * ang_scale + m_solve_state[body_idx].angular_hessian;
	h_tt[0][0] += reg;
	h_tt[1][1] += reg;
	h_tt[2][2] += reg;

	const mat3& h_xt = m_solve_state[body_idx].hessian_xtheta;
	const mat3 h_tx = h_xt.transpose();

	const mat3 h_xx_inv = h_xx.inverse();
	const mat3 s = h_tt - h_tx * h_xx_inv * h_xt;
	const mat3 s_inv = s.inverse();

	unitless::vec3 delta_theta = -(s_inv * (g_ang - h_tx * (h_xx_inv * g_lin)));
	unitless::vec3 delta_x = -(h_xx_inv * (g_lin + h_xt * delta_theta));

	constexpr float max_lin_step = 0.5f;
	constexpr float max_ang_step = 0.5f;

	if (const float lin_size = magnitude(delta_x); lin_size > max_lin_step) {
		delta_x *= (max_lin_step / lin_size);
	}
	if (const float ang_size = magnitude(delta_theta); ang_size > max_ang_step) {
		delta_theta *= (max_ang_step / ang_size);
	}

	body.predicted_position += delta_x * meters(1.f);

	if (const float angle = magnitude(delta_theta); angle > 1e-7f) {
		const unitless::vec3 axis = delta_theta / angle;
		const float half_angle = angle * 0.5f;
		const float sh = std::sin(half_angle);
		const float ch = std::cos(half_angle);
		const quat delta_q(ch, sh * axis.x(), sh * axis.y(), sh * axis.z());
		body.predicted_orientation = normalize(delta_q * body.predicted_orientation);
	}
}

auto gse::vbd::solver::update_dual(const float alpha) -> void {
	for (auto& c : m_graph.contact_constraints()) {
		const auto& body_a = m_bodies[c.body_a];
		const auto& body_b = m_bodies[c.body_b];

		const vec3<length> rAW = rotate_vector(body_a.predicted_orientation, c.r_a);
		const vec3<length> rBW = rotate_vector(body_b.predicted_orientation, c.r_b);
		const vec3<length> pA = body_a.predicted_position + rAW;
		const vec3<length> pB = body_b.predicted_position + rBW;
		const unitless::vec3 d = (pA - pB).as<meters>();

		float Cn[3];
		Cn[0] = dot(c.normal, d) + m_config.collision_margin;
		Cn[1] = dot(c.tangent_u, d);
		Cn[2] = dot(c.tangent_v, d);

		float C[3];
		for (int i = 0; i < 3; i++)
			C[i] = Cn[i] - c.C0[i] * alpha;

		c.lambda[0] = std::min(c.penalty[0] * C[0] + c.lambda[0], 0.f);

		const float friction_bound = std::abs(c.lambda[0]) * c.friction_coeff;

		c.lambda[1] = std::clamp(c.penalty[1] * C[1] + c.lambda[1], -friction_bound, friction_bound);
		c.lambda[2] = std::clamp(c.penalty[2] * C[2] + c.lambda[2], -friction_bound, friction_bound);

		if (c.lambda[0] < 0.f) {
			c.penalty[0] = std::min(c.penalty[0] + m_config.beta * std::abs(C[0]), m_config.penalty_max);
		}

		if (friction_bound > 1e-10f) {
			if (std::abs(c.lambda[1]) < friction_bound) {
				c.penalty[1] = std::min(c.penalty[1] + m_config.beta * std::abs(C[1]), m_config.penalty_max);
			}
			if (std::abs(c.lambda[2]) < friction_bound) {
				c.penalty[2] = std::min(c.penalty[2] + m_config.beta * std::abs(C[2]), m_config.penalty_max);
			}
		}
	}
}
