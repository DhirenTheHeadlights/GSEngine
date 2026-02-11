export module gse.physics:vbd_solver;

import std;

import gse.physics.math;
import gse.utility;
import gse.log;
import :vbd_constraints;
import :vbd_constraint_graph;
import :vbd_contact_cache;
import :motion_component;

export namespace gse::vbd {
	using time_step = time_t<float, seconds>;

	struct solver_config {
		std::uint32_t substeps = 10;
		std::uint32_t iterations = 20;
		float contact_compliance = 0.f;
		float contact_damping = 0.f;
		float friction_coefficient = 0.6f;
		float rho = 1000.f;
		float linear_damping = 1.0f;
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
	private:
		auto solve_substep(
			time_step sub_dt
		) -> void;

		auto predict_positions(
			time_step sub_dt
		) -> void;

		auto accumulate_contact_gradient_hessian(
			const contact_constraint& c,
			float h_squared,
			std::vector<body_solve_state>& solve_state
		) const -> void;

		auto perform_local_newton_step(
			std::uint32_t body_idx,
			const body_solve_state& solve_state,
			float h_squared
		) -> void;

		auto update_lagrange_multipliers(
		) -> void;

		auto derive_velocities(
			time_step sub_dt
		) -> void;

		auto apply_velocity_corrections(
		) -> void;

		auto solve_motor_constraint(
			velocity_motor_constraint& m,
			time_step sub_dt
		) -> void;

		solver_config m_config;
		constraint_graph m_graph;
		std::vector<body_state> m_bodies;
		std::vector<body_solve_state> m_solve_state;
		std::uint32_t m_substep = 0;
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
	m_graph.compute_coloring();

	const time_step sub_dt = dt / static_cast<float>(m_config.substeps);

	for (std::uint32_t s = 0; s < m_config.substeps; ++s) {
		solve_substep(sub_dt);
	}

}

auto gse::vbd::solver::end_frame(std::vector<body_state>& bodies, contact_cache& cache) -> void {
	for (const auto& c : m_graph.contact_constraints()) {
		cache.store(c.body_a, c.body_b, c.feature, cached_lambda{
			.lambda = c.lambda,
			.lambda_tangent_u = c.lambda_tangent_u,
			.lambda_tangent_v = c.lambda_tangent_v,
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

auto gse::vbd::solver::solve_substep(const time_step sub_dt) -> void {
	const float dt_s = sub_dt.as<seconds>();
	const float h_squared = dt_s * dt_s;

	for (auto& body : m_bodies) {
		body.old_position = body.position;
		body.old_orientation = body.orientation;
	}

	predict_positions(sub_dt);

	for (std::uint32_t iter = 0; iter < m_config.iterations; ++iter) {
		for (const auto& color : m_graph.colors()) {
			m_solve_state.assign(m_bodies.size(), {});

			for (const auto idx : color) {
				accumulate_contact_gradient_hessian(
					m_graph.contact_constraints()[idx],
					h_squared,
					m_solve_state
				);
			}

			std::unordered_set<std::uint32_t> bodies_in_color;
			for (const auto idx : color) {
				const auto& c = m_graph.contact_constraints()[idx];
				bodies_in_color.insert(c.body_a);
				bodies_in_color.insert(c.body_b);
			}

			for (const auto body_idx : bodies_in_color) {
				perform_local_newton_step(body_idx, m_solve_state[body_idx], h_squared);
			}
		}
	}

	update_lagrange_multipliers();

	derive_velocities(sub_dt);

	for (auto& motor : m_graph.motor_constraints()) {
		solve_motor_constraint(motor, sub_dt);
	}

	constexpr int velocity_iterations = 8;
	for (int vi = 0; vi < velocity_iterations; ++vi) {
		apply_velocity_corrections();
	}

	for (std::uint32_t bi = 0; bi < m_bodies.size(); ++bi) {
		auto& body = m_bodies[bi];
		if (body.locked || body.sleeping()) {
			body.position = body.predicted_position;
			if (body.update_orientation) {
				body.orientation = body.predicted_orientation;
			}
			continue;
		}

		body.position = body.old_position + body.body_velocity * sub_dt;

		if (body.update_orientation) {
			const auto w = body.body_angular_velocity.as<radians_per_second>();
			const float omega_mag = magnitude(w);
			if (omega_mag > 1e-7f) {
				const float angle = omega_mag * dt_s;
				const unitless::vec3 axis = w / omega_mag;
				const float half = angle * 0.5f;
				const quat dq(std::cos(half), std::sin(half) * axis.x(), std::sin(half) * axis.y(), std::sin(half) * axis.z());
				body.orientation = normalize(dq * body.old_orientation);
			} else {
				body.orientation = body.old_orientation;
			}
		}
	}
}

auto gse::vbd::solver::predict_positions(const time_step sub_dt) -> void {
	constexpr vec3<acceleration> gravity = { 0.f, -9.8f, 0.f };
	const float dt_s = sub_dt.as<seconds>();

	for (auto& body : m_bodies) {
		if (body.locked || body.sleeping()) {
			body.predicted_position = body.position;
			body.inertia_target = body.position;
			body.predicted_velocity = {};
			body.predicted_orientation = body.orientation;
			body.angular_inertia_target = body.orientation;
			body.predicted_angular_velocity = {};
			continue;
		}

		if (body.affected_by_gravity) {
			body.predicted_velocity = body.body_velocity + gravity * sub_dt;
		} else {
			body.predicted_velocity = body.body_velocity;
		}
		body.inertia_target = body.position + body.predicted_velocity * sub_dt;
		body.predicted_position = body.inertia_target;

		if (body.update_orientation) {
			body.predicted_angular_velocity = body.body_angular_velocity;
			const unitless::vec3 w = body.body_angular_velocity.as<radians_per_second>();
			const float omega_mag = magnitude(w);
			if (omega_mag > 1e-7f) {
				const float angle = omega_mag * dt_s;
				const unitless::vec3 axis = w / omega_mag;
				const float half = angle * 0.5f;
				const quat dq(std::cos(half), std::sin(half) * axis.x(), std::sin(half) * axis.y(), std::sin(half) * axis.z());
				body.predicted_orientation = normalize(dq * body.orientation);
			} else {
				body.predicted_orientation = body.orientation;
			}
		} else {
			body.predicted_orientation = body.orientation;
			body.predicted_angular_velocity = {};
		}

		body.angular_inertia_target = body.predicted_orientation;
	}
}

auto gse::vbd::solver::accumulate_contact_gradient_hessian(const contact_constraint& c, const float h_squared, std::vector<body_solve_state>& solve_state) const -> void {
	const auto& body_a = m_bodies[c.body_a];
	const auto& body_b = m_bodies[c.body_b];

	const vec3<length> world_r_a = rotate_vector(body_a.predicted_orientation, c.r_a);
	const vec3<length> world_r_b = rotate_vector(body_b.predicted_orientation, c.r_b);

	const vec3<length> p_a = body_a.predicted_position + world_r_a;
	const vec3<length> p_b = body_b.predicted_position + world_r_b;

	const float gap = (dot(p_b - p_a, c.normal) + c.initial_separation).as<meters>();

	if (gap > 0.f) {
		return;
	}

	const float rho = m_config.rho;
	const float effective = std::min(rho * gap - c.lambda, 0.f);

	const mat3 n_outer_n = outer_product(c.normal, c.normal);

	const unitless::vec3 r_a_unitless = world_r_a.as<meters>();
	const unitless::vec3 r_b_unitless = world_r_b.as<meters>();
	const unitless::vec3 r_cross_n_a = cross(r_a_unitless, c.normal);
	const unitless::vec3 r_cross_n_b = cross(r_b_unitless, c.normal);

	if (!body_a.locked) {
		solve_state[c.body_a].gradient -= c.normal * effective;
		solve_state[c.body_a].hessian += n_outer_n * rho;

		if (body_a.update_orientation) {
			solve_state[c.body_a].angular_gradient -= r_cross_n_a * effective;
			solve_state[c.body_a].angular_hessian += outer_product(r_cross_n_a, r_cross_n_a) * rho;
			solve_state[c.body_a].hessian_xtheta += outer_product(c.normal, r_cross_n_a) * rho;
		}
	}

	if (!body_b.locked) {
		solve_state[c.body_b].gradient += c.normal * effective;
		solve_state[c.body_b].hessian += n_outer_n * rho;

		if (body_b.update_orientation) {
			solve_state[c.body_b].angular_gradient += r_cross_n_b * effective;
			solve_state[c.body_b].angular_hessian += outer_product(r_cross_n_b, r_cross_n_b) * rho;
			solve_state[c.body_b].hessian_xtheta += outer_product(c.normal, r_cross_n_b) * rho;
		}
	}
}

auto gse::vbd::solver::perform_local_newton_step(const std::uint32_t body_idx, const body_solve_state& solve_state, const float h_squared) -> void {
	auto& body = m_bodies[body_idx];

	if (body.locked || body.sleeping()) return;

	if (const float inv_mass = body.inverse_mass().as<per_kilograms>(); inv_mass < 1e-10f) {
		return;
	}

	const float mass = body.mass_value.as<kilograms>();
	const float inertia_weight = mass / h_squared;

	const unitless::vec3 x_diff = (body.predicted_position - body.inertia_target).as<meters>();
	const unitless::vec3 g_lin = x_diff * inertia_weight + solve_state.gradient;

	constexpr float reg = 1e-6f;
	mat3 h_xx = mat3(inertia_weight) + solve_state.hessian;
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
	} else {
		theta_diff = q_v * 2.f;
	}

	const mat3 i_inv = to_unitless(body.inv_inertia);
	const mat3 i = i_inv.inverse();
	const float ang_scale = 1.f / h_squared;

	const unitless::vec3 g_ang = i * theta_diff * ang_scale + solve_state.angular_gradient;

	mat3 h_tt = i * ang_scale + solve_state.angular_hessian;
	h_tt[0][0] += reg;
	h_tt[1][1] += reg;
	h_tt[2][2] += reg;

	const mat3& h_xt = solve_state.hessian_xtheta;
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

auto gse::vbd::solver::update_lagrange_multipliers() -> void {
	for (auto& c : m_graph.contact_constraints()) {
		const auto& body_a = m_bodies[c.body_a];
		const auto& body_b = m_bodies[c.body_b];

		const vec3<length> world_r_a = rotate_vector(body_a.predicted_orientation, c.r_a);
		const vec3<length> world_r_b = rotate_vector(body_b.predicted_orientation, c.r_b);

		const vec3<length> p_a = body_a.predicted_position + world_r_a;
		const vec3<length> p_b = body_b.predicted_position + world_r_b;

		const float gap = (dot(p_b - p_a, c.normal) + c.initial_separation).as<meters>();

		constexpr float max_lambda = 1e6f;
		c.lambda = std::clamp(c.lambda - m_config.rho * gap, 0.f, max_lambda);

	}
}

auto gse::vbd::solver::derive_velocities(const time_step sub_dt) -> void {
	const float dt_s = sub_dt.as<seconds>();

	for (auto& body : m_bodies) {
		if (body.locked) {
			body.body_velocity = {};
			body.body_angular_velocity = {};
			continue;
		}

		if (body.sleeping()) {
			body.body_velocity = {};
			body.body_angular_velocity = {};

			const float wake_dist = magnitude((body.predicted_position - body.position).as<meters>());
			if (wake_dist > 0.001f) {
				body.sleep_counter = 0;
			}
			continue;
		}

		body.body_velocity = (body.predicted_position - body.old_position) / sub_dt;

		const float linear_damping_factor = std::max(0.f, 1.f - m_config.linear_damping * dt_s);
		body.body_velocity *= linear_damping_factor;

		const float lin_speed = magnitude(body.body_velocity).as<meters_per_second>();
		const bool lin_at_rest = lin_speed < m_config.velocity_sleep_threshold;

		if (lin_at_rest) {
			body.body_velocity = {};
		}

		bool ang_at_rest = true;
		if (body.update_orientation) {
			const quat delta_q = body.predicted_orientation * conjugate(body.old_orientation);
			float q_s = delta_q.s();
			unitless::vec3 q_v = delta_q.imaginary_part();
			if (q_s < 0.f) { q_s = -q_s; q_v = -q_v; }

			if (q_s < 0.9999f) {
				const float angle = 2.f * std::acos(std::clamp(q_s, 0.f, 1.f));
				const float sin_half = std::sqrt(1.f - q_s * q_s);
				if (sin_half > 1e-6f) {
					body.body_angular_velocity = (q_v / sin_half) * radians_per_second(angle / dt_s);
				} else {
					body.body_angular_velocity = q_v * radians_per_second(2.f / dt_s);
				}
			} else {
				body.body_angular_velocity = q_v * radians_per_second(2.f / dt_s);
			}

			constexpr float angular_damping = 2.0f;
			const float angular_damping_factor = std::max(0.f, 1.f - angular_damping * dt_s);
			body.body_angular_velocity *= angular_damping_factor;

			const float ang_speed = magnitude(body.body_angular_velocity).as<radians_per_second>();
			ang_at_rest = ang_speed < m_config.velocity_sleep_threshold;

			if (ang_at_rest) {
				body.body_angular_velocity = {};
			}
		} else {
			body.body_angular_velocity = {};
		}

		if (lin_at_rest && ang_at_rest) {
			body.sleep_counter++;
		} else {
			body.sleep_counter = 0;
		}
	}

	if (m_substep % (m_config.substeps * 60) == 0) {
		for (std::uint32_t i = 0; i < m_bodies.size(); ++i) {
			const auto& b = m_bodies[i];
			if (b.locked) continue;
			const float lv = magnitude(b.body_velocity).as<meters_per_second>();
			const float av = b.update_orientation ? magnitude(b.body_angular_velocity).as<radians_per_second>() : 0.f;
			log::println(log::level::info,
				"[VBD:sleep] body={} lin_v={:.6f} ang_v={:.6f} sleep_cnt={} sleeping={}",
				i, lv, av, b.sleep_counter, b.sleeping());
		}
	}
	++m_substep;
}

auto gse::vbd::solver::apply_velocity_corrections() -> void {
	if (m_bodies.empty()) return;

	auto& contacts = m_graph.contact_constraints();
	const auto num_contacts = contacts.size();
	if (num_contacts == 0) return;

	static std::mt19937 rng(42);
	std::vector<std::size_t> order(num_contacts);
	std::iota(order.begin(), order.end(), 0uz);
	std::ranges::shuffle(order, rng);

	for (const auto ci : order) {
		auto& c = contacts[ci];
		if (c.lambda <= 0.f) continue;

		auto& body_a = m_bodies[c.body_a];
		auto& body_b = m_bodies[c.body_b];

		const vec3<length> world_r_a = rotate_vector(body_a.predicted_orientation, c.r_a);
		const vec3<length> world_r_b = rotate_vector(body_b.predicted_orientation, c.r_b);

		const float w_a_lin = body_a.locked ? 0.f : body_a.inverse_mass().as<per_kilograms>();
		const float w_b_lin = body_b.locked ? 0.f : body_b.inverse_mass().as<per_kilograms>();

		const mat3 inv_inertia_a = to_unitless(body_a.inv_inertia);
		const mat3 inv_inertia_b = to_unitless(body_b.inv_inertia);

		const unitless::vec3 n_unit(c.normal.x(), c.normal.y(), c.normal.z());
		const unitless::vec3 rcross_a_n = cross(world_r_a.as<meters>(), n_unit);
		const unitless::vec3 rcross_b_n = cross(world_r_b.as<meters>(), n_unit);
		const unitless::vec3 inv_i_rcross_a_n = inv_inertia_a * rcross_a_n;
		const unitless::vec3 inv_i_rcross_b_n = inv_inertia_b * rcross_b_n;
		const float w_rot_a_n = (body_a.locked || !body_a.update_orientation) ? 0.f : dot(rcross_a_n, inv_i_rcross_a_n);
		const float w_rot_b_n = (body_b.locked || !body_b.update_orientation) ? 0.f : dot(rcross_b_n, inv_i_rcross_b_n);
		const float w_total_n = w_a_lin + w_b_lin + w_rot_a_n + w_rot_b_n;

		const vec3<velocity> v_a = body_a.body_velocity + cross(body_a.body_angular_velocity, world_r_a);
		const vec3<velocity> v_b = body_b.body_velocity + cross(body_b.body_angular_velocity, world_r_b);
		const vec3<velocity> rel_vel = v_a - v_b;

		const velocity v_n = dot(rel_vel, c.normal);

		if (const float v_n_val = v_n.as<meters_per_second>(); w_total_n > 1e-10f && v_n_val > m_config.velocity_sleep_threshold) {
			if (const float impulse = -v_n_val / w_total_n; impulse < -1e-10f) {
				if (!body_a.locked) {
					body_a.body_velocity += c.normal * meters_per_second(w_a_lin * impulse);
					if (body_a.update_orientation) {
						body_a.body_angular_velocity += inv_i_rcross_a_n * radians_per_second(impulse);
					}
				}
				if (!body_b.locked) {
					body_b.body_velocity -= c.normal * meters_per_second(w_b_lin * impulse);
					if (body_b.update_orientation) {
						body_b.body_angular_velocity -= inv_i_rcross_b_n * radians_per_second(impulse);
					}
				}
			}
		}

		// Friction (tangential) correction
		const vec3<velocity> v_t = rel_vel - v_n * c.normal;
		const velocity v_t_mag = magnitude(v_t);

		if (v_t_mag.as<meters_per_second>() < 1e-7f) continue;

		const unitless::vec3 t = normalize(v_t);

		const unitless::vec3 rcross_a_t = cross(world_r_a.as<meters>(), t);
		const unitless::vec3 rcross_b_t = cross(world_r_b.as<meters>(), t);

		const unitless::vec3 inv_inertia_rcross_a_t = inv_inertia_a * rcross_a_t;
		const unitless::vec3 inv_inertia_rcross_b_t = inv_inertia_b * rcross_b_t;

		const float w_rot_a_t = (body_a.locked || !body_a.update_orientation) ? 0.f : dot(rcross_a_t, inv_inertia_rcross_a_t);
		const float w_rot_b_t = (body_b.locked || !body_b.update_orientation) ? 0.f : dot(rcross_b_t, inv_inertia_rcross_b_t);

		const float w_total_t = w_a_lin + w_b_lin + w_rot_a_t + w_rot_b_t;
		if (w_total_t < 1e-10f) continue;

		float delta_lambda_t = -v_t_mag.as<meters_per_second>() / w_total_t;

		const float max_friction = c.friction_coeff * c.lambda;
		delta_lambda_t = std::clamp(delta_lambda_t, -max_friction, max_friction);

		if (!body_a.locked) {
			body_a.body_velocity += t * meters_per_second(w_a_lin * delta_lambda_t);
			if (body_a.update_orientation) {
				body_a.body_angular_velocity += inv_inertia_rcross_a_t * radians_per_second(delta_lambda_t);
			}
		}

		if (!body_b.locked) {
			body_b.body_velocity -= t * meters_per_second(w_b_lin * delta_lambda_t);
			if (body_b.update_orientation) {
				body_b.body_angular_velocity -= inv_inertia_rcross_b_t * radians_per_second(delta_lambda_t);
			}
		}
	}
}

auto gse::vbd::solver::solve_motor_constraint(velocity_motor_constraint& m, const time_step sub_dt) -> void {
	auto& body = m_bodies[m.body_index];
	if (body.locked || body.sleeping()) return;

	const float dt_s = sub_dt.as<seconds>();
	const float alpha = m.compliance / (dt_s * dt_s);
	const float w = body.inverse_mass().as<per_kilograms>();
	const float w_total = w + alpha;

	if (w_total < 1e-10f) return;

	for (int axis = 0; axis < 3; ++axis) {
		const float target = m.target_velocity[axis].as<meters_per_second>();
		const float current = body.body_velocity[axis].as<meters_per_second>();
		const float error = target - current;

		if (std::abs(error) < 0.001f) continue;

		const float delta_lambda = (error - alpha * m.lambda[axis]) / w_total;

		const float max_lambda = m.max_force.as<newtons>() * dt_s;

		const float new_lambda = std::clamp(m.lambda[axis] + delta_lambda, -max_lambda, max_lambda);
		const float effective_delta = new_lambda - m.lambda[axis];
		m.lambda[axis] = new_lambda;

		body.body_velocity[axis] += meters_per_second(w * effective_delta);
	}
}
