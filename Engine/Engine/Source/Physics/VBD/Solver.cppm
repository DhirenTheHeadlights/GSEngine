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

	static int frame_count = 0;
	if (frame_count < 30 && !m_graph.contact_constraints().empty()) {
		float max_penetration = 0.f;
		float total_lambda = 0.f;
		int active_contacts = 0;

		for (const auto& c : m_graph.contact_constraints()) {
			const auto& body_a = m_bodies[c.body_a];
			const auto& body_b = m_bodies[c.body_b];

			const vec3<length> world_r_a = rotate_vector(body_a.orientation, c.r_a);
			const vec3<length> world_r_b = rotate_vector(body_b.orientation, c.r_b);

			const vec3<length> p_a = body_a.position + world_r_a;
			const vec3<length> p_b = body_b.position + world_r_b;

			if (const float gap = dot(p_b - p_a, c.normal).as<meters>(); gap < max_penetration) {
				max_penetration = gap;
			}
			if (c.lambda > 0.f) {
				total_lambda += c.lambda;
				active_contacts++;
			}
		}

		log::println(log::level::info, "[VBD] frame={} contacts={} active={} max_pen={:.6f} total_lambda={:.2f}",
			frame_count, m_graph.contact_constraints().size(), active_contacts, max_penetration, total_lambda);
		frame_count++;
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
			for (auto& [gradient, hessian, _, _2] : m_solve_state) {
				gradient = {};
				hessian = mat3(0.f);
			}

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

		update_lagrange_multipliers();
	}

	derive_velocities(sub_dt);

	for (auto& motor : m_graph.motor_constraints()) {
		solve_motor_constraint(motor, sub_dt);
	}

	apply_velocity_corrections();

	for (auto& body : m_bodies) {
		body.position = body.predicted_position;
		if (body.update_orientation) {
			body.orientation = body.predicted_orientation;
		}
	}
}

auto gse::vbd::solver::predict_positions(const time_step sub_dt) -> void {
	constexpr vec3<acceleration> gravity = { 0.f, -9.8f, 0.f };
	const float dt_s = sub_dt.as<seconds>();

	for (auto& body : m_bodies) {
		if (body.locked) {
			body.predicted_position = body.position;
			body.inertia_target = body.position;
			body.predicted_velocity = {};
			body.predicted_orientation = body.orientation;
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
			const quat omega_q{ 0.f, w.x(), w.y(), w.z() };
			const quat delta_q = 0.5f * omega_q * body.orientation;
			body.predicted_orientation = normalize(body.orientation + delta_q * dt_s);
		} else {
			body.predicted_orientation = body.orientation;
			body.predicted_angular_velocity = {};
		}
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
	const float lambda_plus_rho_c = std::min(c.lambda + rho * gap, 0.f);

	const mat3 n_outer_n = outer_product(c.normal, c.normal);

	if (!body_a.locked) {
		solve_state[c.body_a].gradient -= c.normal * lambda_plus_rho_c;
		solve_state[c.body_a].hessian += n_outer_n * rho;
	}

	if (!body_b.locked) {
		solve_state[c.body_b].gradient += c.normal * lambda_plus_rho_c;
		solve_state[c.body_b].hessian += n_outer_n * rho;
	}
}

auto gse::vbd::solver::perform_local_newton_step(const std::uint32_t body_idx, const body_solve_state& solve_state, const float h_squared) -> void {
	auto& body = m_bodies[body_idx];

	if (body.locked) return;

	if (const float inv_mass = body.inverse_mass().as<per_kilograms>(); inv_mass < 1e-10f) {
		return;
	}

	const float mass = body.mass_value.as<kilograms>();
	const float inertia_weight = mass / h_squared;

	const unitless::vec3 x_diff = (body.predicted_position - body.inertia_target).as<meters>();
	const unitless::vec3 gradient = x_diff * inertia_weight + solve_state.gradient;

	mat3 hessian = mat3(inertia_weight) + solve_state.hessian;

	constexpr float regularization = 1e-6f;
	hessian[0][0] += regularization;
	hessian[1][1] += regularization;
	hessian[2][2] += regularization;

	const mat3 inv_hessian = hessian.inverse();
	const unitless::vec3 delta_x = -(inv_hessian * gradient);

	constexpr float max_step = 0.5f;
	const float step_size = magnitude(delta_x);
	const unitless::vec3 clamped_delta = step_size > max_step ? delta_x * (max_step / step_size) : delta_x;

	body.predicted_position += clamped_delta * meters(1.f);
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

		constexpr float max_lambda = 10000.f;
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

		body.body_velocity = (body.predicted_position - body.old_position) / sub_dt;

		const float linear_damping_factor = std::max(0.f, 1.f - m_config.linear_damping * dt_s);
		body.body_velocity *= linear_damping_factor;

		if (const velocity speed = magnitude(body.body_velocity); speed.as<meters_per_second>() < m_config.velocity_sleep_threshold) {
			body.body_velocity = {};
		}

		if (body.update_orientation) {
			if (const quat delta_q = body.predicted_orientation * conjugate(body.old_orientation); std::abs(delta_q.s()) < 0.9999f) {
				const float angle = 2.f * std::acos(std::clamp(delta_q.s(), -1.f, 1.f));
				if (const float sin_half = std::sqrt(1.f - delta_q.s() * delta_q.s()); sin_half > 1e-6f) {
					const unitless::vec3 axis(delta_q.x() / sin_half, delta_q.y() / sin_half, delta_q.z() / sin_half);
					body.body_angular_velocity = axis * radians_per_second(angle / dt_s);
				}
			}

			constexpr float angular_damping = 2.0f;
			const float angular_damping_factor = std::max(0.f, 1.f - angular_damping * dt_s);
			body.body_angular_velocity *= angular_damping_factor;

			if (magnitude(body.body_angular_velocity).as<radians_per_second>() < m_config.velocity_sleep_threshold) {
				body.body_angular_velocity = {};
			}
		} else {
			body.body_angular_velocity = {};
		}
	}
}

auto gse::vbd::solver::apply_velocity_corrections() -> void {
	for (auto& c : m_graph.contact_constraints()) {
		if (c.lambda <= 0.f) continue;

		auto& body_a = m_bodies[c.body_a];
		auto& body_b = m_bodies[c.body_b];

		const vec3<length> world_r_a = rotate_vector(body_a.predicted_orientation, c.r_a);
		const vec3<length> world_r_b = rotate_vector(body_b.predicted_orientation, c.r_b);

		const vec3<velocity> v_a = body_a.body_velocity + cross(body_a.body_angular_velocity, world_r_a);
		const vec3<velocity> v_b = body_b.body_velocity + cross(body_b.body_angular_velocity, world_r_b);
		const vec3<velocity> rel_vel = v_a - v_b;

		const velocity v_n = dot(rel_vel, c.normal);
		const float v_n_val = v_n.as<meters_per_second>();

		const float w_a_lin = body_a.locked ? 0.f : body_a.inverse_mass().as<per_kilograms>();
		const float w_b_lin = body_b.locked ? 0.f : body_b.inverse_mass().as<per_kilograms>();

		if (v_n_val < -m_config.velocity_sleep_threshold) {
			if (const float w_total = w_a_lin + w_b_lin; w_total > 1e-10f) {
				const float delta_v_n = -v_n_val;

				if (!body_a.locked) {
					body_a.body_velocity += c.normal * meters_per_second(w_a_lin / w_total * delta_v_n);
				}
				if (!body_b.locked) {
					body_b.body_velocity -= c.normal * meters_per_second(w_b_lin / w_total * delta_v_n);
				}
			}
		}

		const vec3<velocity> v_t = rel_vel - v_n * c.normal;
		const velocity v_t_mag = magnitude(v_t);

		if (v_t_mag.as<meters_per_second>() < m_config.velocity_sleep_threshold) continue;

		const unitless::vec3 t = normalize(v_t);

		const unitless::vec3 rcross_a_t = cross(world_r_a.as<meters>(), t);
		const unitless::vec3 rcross_b_t = cross(world_r_b.as<meters>(), t);

		const mat3 inv_inertia_a = to_unitless(body_a.inv_inertia);
		const mat3 inv_inertia_b = to_unitless(body_b.inv_inertia);

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
			const velocity delta_v_a = meters_per_second(w_a_lin * delta_lambda_t);
			body_a.body_velocity += t * delta_v_a;

			if (body_a.update_orientation) {
				const unitless::vec3 delta_omega_a = inv_inertia_rcross_a_t * delta_lambda_t;
				body_a.body_angular_velocity += delta_omega_a * radians_per_second(1.f);
			}
		}

		if (!body_b.locked) {
			const velocity delta_v_b = meters_per_second(w_b_lin * delta_lambda_t);
			body_b.body_velocity -= t * delta_v_b;

			if (body_b.update_orientation) {
				const unitless::vec3 delta_omega_b = inv_inertia_rcross_b_t * delta_lambda_t;
				body_b.body_angular_velocity -= delta_omega_b * radians_per_second(1.f);
			}
		}
	}
}

auto gse::vbd::solver::solve_motor_constraint(velocity_motor_constraint& m, const time_step sub_dt) -> void {
	auto& body = m_bodies[m.body_index];
	if (body.locked) return;

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
