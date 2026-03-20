export module gse.physics:vbd_solver;

import std;

import gse.math;
import gse.utility;
import :vbd_constraints;
import :vbd_constraint_graph;
import :vbd_contact_cache;
import :motion_component;

export namespace gse::vbd {
	using time_step = time_t<float, seconds>;

	struct solver_config {
		std::uint32_t iterations = 10;
		float alpha = 0.99f;
		stiffness beta = newtons_per_meter(100000.f);
		float gamma = 0.99f;
		bool post_stabilize = true;
		stiffness penalty_min = newtons_per_meter(1.0f);
		stiffness penalty_max = newtons_per_meter(1e9f);
		length collision_margin = meters(0.0005f);
		length stick_threshold = meters(0.01f);
		float friction_coefficient = 0.6f;
		velocity restitution_threshold = meters_per_second(0.5f);
		velocity velocity_sleep_threshold = meters_per_second(0.001f);
		angular_velocity angular_sleep_threshold = radians_per_second(0.05f);
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
		) -> std::vector<body_state>&;

		auto body_states(
		) const -> std::span<const body_state>;

		auto graph(this auto&& self) -> auto& { return self.m_graph; }
	private:
		auto accumulate_contact(
			const contact_constraint& constraint,
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
		) -> void;

		auto accumulate_joint(
			const joint_constraint& constraint,
			std::uint32_t body_idx,
			time_squared h_squared,
			time_step dt,
			float alpha
		) -> void;

		auto update_dual(
			float alpha
		) -> void;

		auto update_joint_dual(
			time_squared h_squared
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
	// MSVC modules ADL workaround: gse::operator*(quat, unitless::vec3) not found from gse::vbd
	auto rotate_axis(
		const quat& q,
		const unitless::vec3& v
	) -> unitless::vec3;

	auto accumulate_geometric_stiffness(
		body_solve_state& state,
		const unitless::vec3& r,
		const unitless::vec3& dir,
		float abs_force
	) -> void;

	auto contact_effective_mass(
		const body_state& body_a,
		const body_state& body_b,
		const vec3<length>& r_aw,
		const vec3<length>& r_bw,
		const unitless::vec3& dir
	) -> mass;
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
	const auto num_bodies = static_cast<std::uint32_t>(m_bodies.size());
	const time_squared h_squared = dt * dt;

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
		const vec3<length> r_aw = rotate_vector(ba.orientation, c.r_a);
		const vec3<length> r_bw = rotate_vector(bb.orientation, c.r_b);
		const vec3<length> p_a = ba.position + r_aw;
		const vec3<length> p_b = bb.position + r_bw;
		const vec3<length> d = p_a - p_b;

		c.c0[0] = dot(c.normal, d) + m_config.collision_margin;
		c.c0[1] = dot(c.tangent_u, d);
		c.c0[2] = dot(c.tangent_v, d);

		const auto va = ba.body_velocity + cross(ba.body_angular_velocity, r_aw);
		const auto vb = bb.body_velocity + cross(bb.body_angular_velocity, r_bw);
		const velocity v_rel_n = dot(c.normal, va - vb);
		c.approach_speed = std::min(v_rel_n, velocity{});
	}

	for (auto& c : m_graph.contact_constraints()) {
		const auto& ba = m_bodies[c.body_a];
		const auto& bb = m_bodies[c.body_b];

		const vec3<length> r_aw = rotate_vector(ba.orientation, c.r_a);
		const vec3<length> r_bw = rotate_vector(bb.orientation, c.r_b);

		const stiffness base_floor = std::max(c.penalty_floor, m_config.penalty_min);
		const bool persistent_active_normal = c.lambda[0] < meters(-1e-3f) && c.c0[0] < meters(-1e-4f);

		const stiffness normal_floor =
			persistent_active_normal
				? std::clamp<stiffness>(
					contact_effective_mass(ba, bb, r_aw, r_bw, c.normal) / h_squared,
					base_floor,
					m_config.penalty_max
				)
				: base_floor;

		const stiffness row_floor[3] = {
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
	}

	for (auto& j : m_graph.joint_constraints()) {
		const auto& ba = m_bodies[j.body_a];
		const auto& bb = m_bodies[j.body_b];
		const vec3<length> r_aw = rotate_vector(ba.orientation, j.local_anchor_a);
		const vec3<length> r_bw = rotate_vector(bb.orientation, j.local_anchor_b);

		for (int k = 0; k < 3; ++k) {
			j.pos_lambda[k] *= m_config.gamma;
		}
		for (int k = 0; k < 3; ++k) {
			j.ang_lambda[k] *= m_config.gamma;
		}
		if (j.limits_enabled) {
			j.limit_lambda *= m_config.gamma;
		}

		const unitless::vec3 dirs[3] = { { 1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f, 1.f } };

		int num_pos_rows = 3;
		if (j.type == joint_type::distance) {
			num_pos_rows = 1;
		}
		else if (j.type == joint_type::slider) {
			num_pos_rows = 2;
		}

		for (int k = 0; k < num_pos_rows; ++k) {
			unitless::vec3 dir;
			if (j.type == joint_type::distance) {
				const vec3<length> d = (ba.position + r_aw) - (bb.position + r_bw);
				const float d_mag = magnitude(d).as<meters>();
				dir = d_mag > 1e-7f ? d.as<meters>() / d_mag : unitless::vec3{ 0.f, 1.f, 0.f };
			}
			else if (j.type == joint_type::slider) {
				const unitless::vec3 axis_w = rotate_axis(ba.orientation, j.local_axis_a);
				unitless::vec3 perp0 = cross(axis_w, unitless::vec3{ 0.f, 1.f, 0.f });
				if (magnitude(perp0) < 1e-6f) {
					perp0 = cross(axis_w, unitless::vec3{ 1.f, 0.f, 0.f });
				}
				perp0 = normalize(perp0);
				dir = k == 0 ? perp0 : normalize(cross(axis_w, perp0));
			}
			else {
				dir = dirs[k];
			}
			const stiffness eff = contact_effective_mass(ba, bb, r_aw, r_bw, dir) / h_squared;
			const stiffness pos_penalty_cap = (j.compliance > inverse_mass{})
				? std::min<stiffness>(1.f / (j.compliance * h_squared), m_config.penalty_max)
				: m_config.penalty_max;
			j.pos_penalty[k] = std::clamp(
				std::max(j.pos_penalty[k] * m_config.gamma, eff),
				m_config.penalty_min, pos_penalty_cap
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
			unitless::vec3 ang_dir;
			if (j.type == joint_type::hinge) {
				const unitless::vec3 axis_a = rotate_axis(ba.orientation, j.local_axis_a);
				unitless::vec3 perp_u = cross(axis_a, unitless::vec3{ 0.f, 1.f, 0.f });
				if (magnitude(perp_u) < 1e-6f) {
					perp_u = cross(axis_a, unitless::vec3{ 1.f, 0.f, 0.f });
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

			const float eff_ang = inv_i_sum > per_kilogram_meter_squared(1e-10f)
				? (1.f / inv_i_sum / h_squared).as<newton_meters>()
				: m_config.penalty_min.as<newtons_per_meter>();

			j.ang_penalty[k] = std::clamp(
				std::max(j.ang_penalty[k] * m_config.gamma, eff_ang),
				m_config.penalty_min.as<newtons_per_meter>(), m_config.penalty_max.as<newtons_per_meter>()
			);
		}

		if (j.limits_enabled) {
			const unitless::vec3 limit_axis = rotate_axis(ba.orientation, j.local_axis_a);
			inverse_inertia inv_i_sum{};

			if (!ba.locked) {
				inv_i_sum += dot(limit_axis, ba.inv_inertia * limit_axis);
			}
			if (!bb.locked) {
				inv_i_sum += dot(limit_axis, bb.inv_inertia * limit_axis);
			}

			const float eff_limit = inv_i_sum > per_kilogram_meter_squared(1e-10f)
				? ((1.f / inv_i_sum) / h_squared).as<newton_meters>()
				: m_config.penalty_min.as<newtons_per_meter>();

			j.limit_penalty = std::clamp(
				std::max(j.limit_penalty * m_config.gamma, eff_limit),
				m_config.penalty_min.as<newtons_per_meter>(), m_config.penalty_max.as<newtons_per_meter>()
			);
		}
	}

	for (auto& j : m_graph.joint_constraints()) {
		const auto& ba = m_bodies[j.body_a];
		const auto& bb = m_bodies[j.body_b];

		const vec3<length> r_aw = rotate_vector(ba.orientation, j.local_anchor_a);
		const vec3<length> r_bw = rotate_vector(bb.orientation, j.local_anchor_b);
		const vec3<length> d = (ba.position + r_aw) - (bb.position + r_bw);

		if (j.type == joint_type::distance) {
			j.pos_c0[0] = meters(magnitude(d).as<meters>()) - j.target_distance;
		}
		else if (j.type == joint_type::fixed || j.type == joint_type::hinge) {
			const unitless::vec3 dirs[3] = { { 1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f, 1.f } };
			for (int k = 0; k < 3; ++k) {
				j.pos_c0[k] = dot(dirs[k], d);
			}

			const quat q_error = bb.orientation * conjugate(ba.orientation) * conjugate(j.rest_orientation);
			const vec3<angle> theta = to_axis_angle(q_error);

			if (j.type == joint_type::fixed) {
				for (int k = 0; k < 3; ++k) {
					j.ang_c0[k] = theta[k];
				}
			}
			else {
				const unitless::vec3 axis_a = rotate_axis(ba.orientation, j.local_axis_a);
				const unitless::vec3 axis_b = rotate_axis(bb.orientation, j.local_axis_b);
				const unitless::vec3 swing_error = cross(axis_a, axis_b);

				unitless::vec3 perp_u = cross(axis_a, unitless::vec3{ 0.f, 1.f, 0.f });

				if (magnitude(perp_u) < 1e-6f) {
					perp_u = cross(axis_a, unitless::vec3{ 1.f, 0.f, 0.f });
				}

				perp_u = normalize(perp_u);
				const unitless::vec3 perp_v = normalize(cross(axis_a, perp_u));
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
			}
		}
		else if (j.type == joint_type::slider) {
			const unitless::vec3 axis_w = normalize(rotate_axis(ba.orientation, j.local_axis_a));
			unitless::vec3 perp0 = cross(axis_w, unitless::vec3{ 0.f, 1.f, 0.f });
			if (magnitude(perp0) < 1e-6f) {
				perp0 = cross(axis_w, unitless::vec3{ 1.f, 0.f, 0.f });
			}
			perp0 = normalize(perp0);
			const unitless::vec3 perp1 = normalize(cross(axis_w, perp0));
			j.pos_c0[0] = dot(perp0, d);
			j.pos_c0[1] = dot(perp1, d);

			const auto slider_theta = to_axis_angle(bb.orientation * conjugate(ba.orientation) * conjugate(j.rest_orientation));
			for (int k = 0; k < 3; ++k) {
				j.ang_c0[k] = slider_theta[k];
			}
		}
	}

	constexpr acceleration gravity_mag = meters_per_second_squared(9.8f);
	const vec3<acceleration> gravity = { meters_per_second_squared(0.f), -gravity_mag, meters_per_second_squared(0.f) };

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

		const velocity delta_vy = body.body_velocity.y() - m_prev_velocity[i].y();
		const acceleration accel_y = delta_vy / dt;
		float aw = std::clamp(-(accel_y / gravity_mag), 0.f, 1.f);
		if (!std::isfinite(aw)) {
			aw = 0.f;
		}
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

		constexpr angular_velocity max_angular_speed = radians_per_second(50.f);
		if (const auto w = magnitude(body.body_angular_velocity); w > max_angular_speed) {
			body.body_angular_velocity *= (max_angular_speed / w);
		}

		body.inertia_target = body.position + body.body_velocity * dt;
		if (body.affected_by_gravity) {
			body.inertia_target += gravity * dt * dt;
		}

		body.initial_position = body.position;
		body.initial_orientation = body.orientation;

		const float aw = m_accel_weight[i];
		body.predicted_position = body.position + body.body_velocity * dt;
		if (body.affected_by_gravity) {
			body.predicted_position += gravity * dt * dt * aw;
		}

		if (body.update_orientation) {
			const vec3<angle> aa = body.body_angular_velocity * dt;
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

		const vec3<length> target = body.initial_position + motor.target_velocity * dt;
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
		if (a.sleeping() && !b.locked && !b.sleeping() && magnitude(b.body_velocity) > wake_thresh) {
			a.sleep_counter = 0;
		}
		if (b.sleeping() && !a.locked && !a.sleeping() && magnitude(a.body_velocity) > wake_thresh) {
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

	const int total_iterations = static_cast<int>(m_config.iterations) + (m_config.post_stabilize ? 1 : 0);

	for (int it = 0; it < total_iterations; ++it) {
		float current_alpha = m_config.alpha;
		if (m_config.post_stabilize) {
			current_alpha = it < static_cast<int>(m_config.iterations) ? 1.0f : 0.0f;
		}

		for (const auto& body_color : m_graph.body_colors()) {
			for (const auto bi : body_color) {
				m_solve_state[bi] = {};
				for (const auto ci : m_graph.body_contact_indices(bi)) {
					accumulate_contact(contacts[ci], bi, h_squared, current_alpha);
				}
				for (const auto ji : m_graph.body_joint_indices(bi)) {
					accumulate_joint(joints[ji], bi, h_squared, dt, current_alpha);
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
				for (const auto ji : m_graph.body_joint_indices(motor.body_index)) {
					accumulate_joint(joints[ji], motor.body_index, h_squared, dt, current_alpha);
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
				accumulate_joint(joints[ji], i, h_squared, dt, current_alpha);
			}
			perform_newton_step(i, h_squared);
		}

		if (it < static_cast<int>(m_config.iterations)) {
			update_dual(current_alpha);
		}

		if (it == static_cast<int>(m_config.iterations) - 1) {
			update_joint_dual(h_squared);
			for (auto& body : m_bodies) {
				if (!body.locked && !body.sleeping() && body.mass_value > mass{}) {
					body.body_velocity = (body.predicted_position - body.initial_position) / dt;

					if (body.update_orientation) {
						body.body_angular_velocity = difference_axis_angle(body.initial_orientation, body.predicted_orientation) / dt;
					}
				}
			}

			{
				for (const auto& c : m_graph.contact_constraints()) {
					if (c.restitution <= 0.f) {
						continue;
					}
					if (c.lambda[0] >= length{}) {
						continue;
					}
					if (c.approach_speed >= -m_config.restitution_threshold) {
						continue;
					}

					auto& ba = m_bodies[c.body_a];
					auto& bb = m_bodies[c.body_b];

					const vec3<length> rAW = rotate_vector(ba.orientation, c.r_a);
					const vec3<length> rBW = rotate_vector(bb.orientation, c.r_b);

					const auto va = ba.body_velocity + cross(ba.body_angular_velocity, rAW);
					const auto vb = bb.body_velocity + cross(bb.body_angular_velocity, rBW);
					const velocity v_rel_after = dot(c.normal, va - vb);

					const float speed_ratio = std::clamp(
						(-c.approach_speed - m_config.restitution_threshold) / m_config.restitution_threshold,
						0.f, 1.f
					);
					const velocity v_target = -c.restitution * speed_ratio * c.approach_speed;
					const velocity dv = v_target - v_rel_after;

					if (dv <= velocity{}) {
						continue;
					}

					const gse::inverse_mass w_a = ba.locked ? gse::inverse_mass{} : 1.f / ba.mass_value;
					const gse::inverse_mass w_b = bb.locked ? gse::inverse_mass{} : 1.f / bb.mass_value;
					const gse::inverse_mass w_total = w_a + w_b;

					if (w_total <= gse::inverse_mass{}) {
						continue;
					}

					const auto impulse = c.normal * (dv / w_total);

					if (!ba.locked) {
						ba.body_velocity += impulse * w_a;
					}
					if (!bb.locked) {
						bb.body_velocity -= impulse * w_b;
					}
				}
			}
		}
	}

	for (auto& body : m_bodies) {
		if (body.locked) {
			continue;
		}
		if (body.sleeping()) {
			continue;
		}

		if (magnitude(body.body_velocity) < m_config.velocity_sleep_threshold &&
			magnitude(body.body_angular_velocity) < m_config.angular_sleep_threshold) {
			++body.sleep_counter;
		}
		else {
			body.sleep_counter /= 2;
		}

		body.position = body.predicted_position;
		if (body.update_orientation) {
			body.orientation = body.predicted_orientation;
		}
	}
}

auto gse::vbd::solver::end_frame(std::vector<body_state>& bodies, contact_cache& cache) -> void {
	for (const auto& c : m_graph.contact_constraints()) {
		const length friction_bound = abs(c.lambda[0]) * c.friction_coeff;
		const length tangential_lambda = hypot(c.lambda[1], c.lambda[2]);
		const length tangential_gap = hypot(c.c0[1], c.c0[2]);

		const bool sticking =
			c.lambda[0] < meters(-1e-3f) &&
			tangential_gap < m_config.stick_threshold &&
			tangential_lambda < friction_bound;

		cache.store(c.body_a, c.body_b, c.feature, cached_lambda{
			.lambda = c.lambda,
			.penalty = c.penalty,
			.normal = c.normal,
			.tangent_u = c.tangent_u,
			.tangent_v = c.tangent_v,
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

auto gse::vbd::solver::accumulate_contact(const contact_constraint& constraint, const std::uint32_t body_idx, const time_squared h_squared, const float alpha) -> void {
	const auto& body_a = m_bodies[constraint.body_a];
	const auto& body_b = m_bodies[constraint.body_b];
	const bool is_a = (body_idx == constraint.body_a);

	const vec3<length> r_aw = rotate_vector(body_a.predicted_orientation, constraint.r_a);
	const vec3<length> r_bw = rotate_vector(body_b.predicted_orientation, constraint.r_b);
	const vec3<length> p_a = body_a.predicted_position + r_aw;
	const vec3<length> p_b = body_b.predicted_position + r_bw;
	const vec3<length> d = p_a - p_b;

	length cn[3];
	cn[0] = dot(constraint.normal, d) + m_config.collision_margin;
	cn[1] = dot(constraint.tangent_u, d);
	cn[2] = dot(constraint.tangent_v, d);

	length c[3];
	for (int i = 0; i < 3; i++) {
		c[i] = cn[i] - constraint.c0[i] * alpha;
	}

	float f[3];
	f[0] = std::min(constraint.penalty[0].as<newtons_per_meter>() * c[0].as<meters>() + constraint.lambda[0].as<meters>(), 0.f);
	const length friction_bound = abs(constraint.lambda[0]) * constraint.friction_coeff;
	f[1] = std::clamp(constraint.penalty[1].as<newtons_per_meter>() * c[1].as<meters>() + constraint.lambda[1].as<meters>(), -friction_bound.as<meters>(), friction_bound.as<meters>());
	f[2] = std::clamp(constraint.penalty[2].as<newtons_per_meter>() * c[2].as<meters>() + constraint.lambda[2].as<meters>(), -friction_bound.as<meters>(), friction_bound.as<meters>());

	const float sign = is_a ? 1.f : -1.f;
	const unitless::vec3 r = (is_a ? r_aw : r_bw).as<meters>();
	const unitless::vec3 dirs[3] = { constraint.normal, constraint.tangent_u, constraint.tangent_v };

	for (int i = 0; i < 3; i++) {
		if (i > 0 && friction_bound <= meters(1e-10f)) {
			continue;
		}
		if (std::abs(f[i]) < 1e-12f && constraint.penalty[i] < newtons_per_meter(1e-6f)) {
			continue;
		}

		const unitless::vec3 j_lin = dirs[i] * sign;
		const unitless::vec3 j_ang = cross(r, dirs[i]) * sign;

		m_solve_state[body_idx].gradient += j_lin * f[i];
		m_solve_state[body_idx].hessian += outer_product(j_lin, j_lin) * constraint.penalty[i].as<newtons_per_meter>();

		if (m_bodies[body_idx].update_orientation) {
			m_solve_state[body_idx].angular_gradient += j_ang * f[i];
			m_solve_state[body_idx].angular_hessian += outer_product(j_ang, j_ang) * constraint.penalty[i].as<newtons_per_meter>();
			m_solve_state[body_idx].hessian_xtheta += outer_product(j_lin, j_ang) * constraint.penalty[i].as<newtons_per_meter>();
		}
	}
}

auto gse::vbd::solver::accumulate_motor(const velocity_motor_constraint& m, const time_squared h_squared) -> void {
	const auto& body = m_bodies[m.body_index];
	if (body.locked || body.sleeping()) {
		return;
	}

	const float alpha = std::max(m.compliance, 1e-6f);
	const float mass_f = body.mass_value.as<kilograms>();
	const float inertia_weight = mass_f / h_squared.as<seconds_squared>();
	const float stiffness_f = inertia_weight / alpha;

	const unitless::vec3 diff = (body.predicted_position - body.motor_target).as<meters>();

	unitless::vec3 masked_diff = diff;
	if (m.horizontal_only) {
		masked_diff[1] = 0.f;
	}

	unitless::vec3 motor_gradient = masked_diff * stiffness_f;
	if (const float grad_mag = magnitude(motor_gradient); grad_mag > m.max_force.as<newtons>()) {
		motor_gradient *= (m.max_force.as<newtons>() / grad_mag);
	}

	const float grad_before = magnitude(motor_gradient);

	const auto& contacts = m_graph.contact_constraints();
	for (const auto ci : m_graph.body_contact_indices(m.body_index)) {
		const auto& c = contacts[ci];
		const auto other = (c.body_a == m.body_index) ? c.body_b : c.body_a;
		const auto& other_body = m_bodies[other];

		const bool blocking_body =
			other_body.locked ||
			other_body.mass_value >= body.mass_value;

		if (!blocking_body) {
			continue;
		}

		const auto& body_a = m_bodies[c.body_a];
		const auto& body_b = m_bodies[c.body_b];

		const vec3<length> r_aw = rotate_vector(body_a.predicted_orientation, c.r_a);
		const vec3<length> r_bw = rotate_vector(body_b.predicted_orientation, c.r_b);
		const vec3<length> p_a = body_a.predicted_position + r_aw;
		const vec3<length> p_b = body_b.predicted_position + r_bw;
		const vec3<length> d = p_a - p_b;

		if (const length normal_gap = dot(c.normal, d) + m_config.collision_margin; normal_gap >= length{} && c.lambda[0] >= meters(-1e-3f)) {
			continue;
		}

		const unitless::vec3 push_dir = (c.body_a == m.body_index) ? c.normal : -c.normal;
		if (const float proj = dot(motor_gradient, push_dir); proj > 0.f) {
			motor_gradient -= push_dir * proj;
		}
	}

	const float grad_after = magnitude(motor_gradient);
	const float motor_scale = (grad_before > 1e-10f) ? (grad_after / grad_before) : 0.f;

	m_solve_state[m.body_index].gradient += motor_gradient;

	mat3 motor_hessian(0.f);
	motor_hessian[0][0] = stiffness_f * motor_scale;
	motor_hessian[1][1] = m.horizontal_only ? 0.f : stiffness_f * motor_scale;
	motor_hessian[2][2] = stiffness_f * motor_scale;

	m_solve_state[m.body_index].hessian += motor_hessian;
}

auto gse::vbd::solver::perform_newton_step(const std::uint32_t body_idx, const time_squared h_squared) -> void {
	auto& body = m_bodies[body_idx];

	if (body.locked || body.sleeping()) {
		return;
	}
	if (body.inverse_mass() < per_kilograms(1e-10f)) {
		return;
	}

	const float inertia_weight = (body.mass_value / h_squared).as<newtons_per_meter>();

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

	const mat3 i_inv = body.inv_inertia.as<internal::dimensionless>();
	const mat3 i = i_inv.inverse();
	const float ang_scale = 1.f / h_squared.as<seconds_squared>();

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

	if (magnitude(delta_theta) > 1e-7f) {
		const vec3<angle> aa = { radians(delta_theta.x()), radians(delta_theta.y()), radians(delta_theta.z()) };
		body.predicted_orientation = normalize(from_axis_angle_vector(aa) * body.predicted_orientation);
	}
}

auto gse::vbd::solver::update_dual(const float alpha) -> void {
	for (auto& con : m_graph.contact_constraints()) {
		const auto& body_a = m_bodies[con.body_a];
		const auto& body_b = m_bodies[con.body_b];

		const vec3<length> rAW = rotate_vector(body_a.predicted_orientation, con.r_a);
		const vec3<length> rBW = rotate_vector(body_b.predicted_orientation, con.r_b);
		const vec3<length> pA = body_a.predicted_position + rAW;
		const vec3<length> pB = body_b.predicted_position + rBW;
		const vec3<length> d = pA - pB;

		length cn[3];
		cn[0] = dot(con.normal, d) + m_config.collision_margin;
		cn[1] = dot(con.tangent_u, d);
		cn[2] = dot(con.tangent_v, d);

		length c[3];
		for (int i = 0; i < 3; i++) {
			c[i] = cn[i] - con.c0[i] * alpha;
		}

		con.lambda[0] = meters(std::min(con.penalty[0].as<newtons_per_meter>() * c[0].as<meters>() + con.lambda[0].as<meters>(), 0.f));

		const length friction_bound = abs(con.lambda[0]) * con.friction_coeff;

		con.lambda[1] = meters(std::clamp(con.penalty[1].as<newtons_per_meter>() * c[1].as<meters>() + con.lambda[1].as<meters>(), -friction_bound.as<meters>(), friction_bound.as<meters>()));
		con.lambda[2] = meters(std::clamp(con.penalty[2].as<newtons_per_meter>() * c[2].as<meters>() + con.lambda[2].as<meters>(), -friction_bound.as<meters>(), friction_bound.as<meters>()));

		if (con.lambda[0] < length{}) {
			con.penalty[0] = std::min(con.penalty[0] + m_config.beta * abs(c[0]).as<meters>(), m_config.penalty_max);
		}

		if (friction_bound > meters(1e-10f)) {
			if (abs(con.lambda[1]) < friction_bound) {
				con.penalty[1] = std::min(con.penalty[1] + m_config.beta * abs(c[1]).as<meters>(), m_config.penalty_max);
			}
			if (abs(con.lambda[2]) < friction_bound) {
				con.penalty[2] = std::min(con.penalty[2] + m_config.beta * abs(c[2]).as<meters>(), m_config.penalty_max);
			}
		}
	}
}

auto gse::vbd::solver::accumulate_joint(const joint_constraint& constraint, const std::uint32_t body_idx, const time_squared h_squared, const time_step dt, const float alpha) -> void {
	const auto& body_a = m_bodies[constraint.body_a];
	const auto& body_b = m_bodies[constraint.body_b];
	const bool is_a = (body_idx == constraint.body_a);
	const float sign = is_a ? 1.f : -1.f;

	const vec3<length> r_aw = rotate_vector(body_a.predicted_orientation, constraint.local_anchor_a);
	const vec3<length> r_bw = rotate_vector(body_b.predicted_orientation, constraint.local_anchor_b);
	const vec3<length> p_a = body_a.predicted_position + r_aw;
	const vec3<length> p_b = body_b.predicted_position + r_bw;
	const vec3<length> d = p_a - p_b;

	const unitless::vec3 r = (is_a ? r_aw : r_bw).as<meters>();

	if (constraint.type == joint_type::distance) {
		const float d_mag = magnitude(d).as<meters>();
		const unitless::vec3 d_hat = d_mag > 1e-7f ? d.as<meters>() / d_mag : unitless::vec3{ 0.f, 1.f, 0.f };
		length c = (meters(d_mag) - constraint.target_distance) - constraint.pos_c0[0] * alpha;

		if (constraint.damping > 0.f) {
			const auto vel_a = body_a.body_velocity + cross(body_a.body_angular_velocity, r_aw);
			const auto vel_b = body_b.body_velocity + cross(body_b.body_angular_velocity, r_bw);
			const velocity v_rel = dot(d_hat, vel_a - vel_b);
			c += v_rel * dt * constraint.damping;
		}

		const float fi = constraint.pos_penalty[0].as<newtons_per_meter>() * c.as<meters>() + constraint.pos_lambda[0].as<meters>();

		const unitless::vec3 j_lin = d_hat * sign;
		const unitless::vec3 j_ang = cross(r, d_hat) * sign;
		const float pen = constraint.pos_penalty[0].as<newtons_per_meter>();

		m_solve_state[body_idx].gradient += j_lin * fi;
		m_solve_state[body_idx].hessian += outer_product(j_lin, j_lin) * pen;

		if (m_bodies[body_idx].update_orientation) {
			m_solve_state[body_idx].angular_gradient += j_ang * fi;
			m_solve_state[body_idx].angular_hessian += outer_product(j_ang, j_ang) * pen;
			m_solve_state[body_idx].hessian_xtheta += outer_product(j_lin, j_ang) * pen;
			accumulate_geometric_stiffness(m_solve_state[body_idx], r, d_hat, std::abs(fi));
		}
	}
	else if (constraint.type == joint_type::fixed || constraint.type == joint_type::hinge) {
		const unitless::vec3 dirs[3] = { { 1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f, 1.f } };

		for (int k = 0; k < 3; ++k) {
			const length c = dot(dirs[k], d) - constraint.pos_c0[k] * alpha;
			const float fi = constraint.pos_penalty[k].as<newtons_per_meter>() * c.as<meters>() + constraint.pos_lambda[k].as<meters>();
			const float pen = constraint.pos_penalty[k].as<newtons_per_meter>();

			const unitless::vec3 j_lin = dirs[k] * sign;
			const unitless::vec3 j_ang = cross(r, dirs[k]) * sign;

			m_solve_state[body_idx].gradient += j_lin * fi;
			m_solve_state[body_idx].hessian += outer_product(j_lin, j_lin) * pen;

			if (m_bodies[body_idx].update_orientation) {
				m_solve_state[body_idx].angular_gradient += j_ang * fi;
				m_solve_state[body_idx].angular_hessian += outer_product(j_ang, j_ang) * pen;
				m_solve_state[body_idx].hessian_xtheta += outer_product(j_lin, j_ang) * pen;
				accumulate_geometric_stiffness(m_solve_state[body_idx], r, dirs[k], std::abs(fi));
			}
		}

		if (m_bodies[body_idx].update_orientation) {
			const quat q_error = body_b.predicted_orientation * conjugate(body_a.predicted_orientation) * conjugate(constraint.rest_orientation);
			const vec3<angle> theta = to_axis_angle(q_error);

			if (constraint.type == joint_type::fixed) {
				for (int k = 0; k < 3; ++k) {
					const angle c_ang = theta[k] - constraint.ang_c0[k] * alpha;
					const angle f_ang = constraint.ang_penalty[k] * c_ang + constraint.ang_lambda[k];

					const unitless::vec3 j_ang = dirs[k] * (-sign);
					m_solve_state[body_idx].angular_gradient += j_ang * f_ang.as<radians>();
					m_solve_state[body_idx].angular_hessian += outer_product(j_ang, j_ang) * constraint.ang_penalty[k];
				}
			}
			else {
				const unitless::vec3 axis_a = rotate_axis(body_a.predicted_orientation, constraint.local_axis_a);
				const unitless::vec3 axis_b = rotate_axis(body_b.predicted_orientation, constraint.local_axis_b);
				const unitless::vec3 swing_error = cross(axis_a, axis_b);

				unitless::vec3 perp_u = cross(axis_a, unitless::vec3{ 0.f, 1.f, 0.f });
				if (magnitude(perp_u) < 1e-6f) {
					perp_u = cross(axis_a, unitless::vec3{ 1.f, 0.f, 0.f });
				}
				perp_u = normalize(perp_u);
				const unitless::vec3 perp_v = normalize(cross(axis_a, perp_u));

				const angle c_u = radians(dot(perp_u, swing_error)) - constraint.ang_c0[0] * alpha;
				const angle c_v = radians(dot(perp_v, swing_error)) - constraint.ang_c0[1] * alpha;

				const angle f_u = constraint.ang_penalty[0] * c_u + constraint.ang_lambda[0];
				const angle f_v = constraint.ang_penalty[1] * c_v + constraint.ang_lambda[1];

				const unitless::vec3 j_ang_u = perp_u * (-sign);
				const unitless::vec3 j_ang_v = perp_v * (-sign);

				m_solve_state[body_idx].angular_gradient += j_ang_u * f_u.as<radians>();
				m_solve_state[body_idx].angular_hessian += outer_product(j_ang_u, j_ang_u) * constraint.ang_penalty[0];

				m_solve_state[body_idx].angular_gradient += j_ang_v * f_v.as<radians>();
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
						const float c_limit_f = c_limit.as<radians>();
						const float f_limit = constraint.limit_penalty * c_limit_f + constraint.limit_lambda.as<radians>();
						const float f_clamped = (c_limit_f < 0.f) ? std::min(f_limit, 0.f) : std::max(f_limit, 0.f);

						const unitless::vec3 j_limit = axis_a * (-sign);
						m_solve_state[body_idx].angular_gradient += j_limit * f_clamped;
						m_solve_state[body_idx].angular_hessian += outer_product(j_limit, j_limit) * constraint.limit_penalty;
					}
				}
			}
		}
	}
	else if (constraint.type == joint_type::slider) {
		const unitless::vec3 axis_w = normalize(rotate_axis(body_a.predicted_orientation, constraint.local_axis_a));
		unitless::vec3 perp0 = cross(axis_w, unitless::vec3{ 0.f, 1.f, 0.f });
		if (magnitude(perp0) < 1e-6f) {
			perp0 = cross(axis_w, unitless::vec3{ 1.f, 0.f, 0.f });
		}
		perp0 = normalize(perp0);
		const unitless::vec3 perp1 = normalize(cross(axis_w, perp0));

		const unitless::vec3 perps[2] = { perp0, perp1 };
		for (int k = 0; k < 2; ++k) {
			const length C = dot(perps[k], d) - constraint.pos_c0[k] * alpha;
			const float fi = constraint.pos_penalty[k].as<newtons_per_meter>() * C.as<meters>() + constraint.pos_lambda[k].as<meters>();
			const float pen = constraint.pos_penalty[k].as<newtons_per_meter>();

			const unitless::vec3 j_lin = perps[k] * sign;
			const unitless::vec3 j_ang = cross(r, perps[k]) * sign;

			m_solve_state[body_idx].gradient += j_lin * fi;
			m_solve_state[body_idx].hessian += outer_product(j_lin, j_lin) * pen;

			if (m_bodies[body_idx].update_orientation) {
				m_solve_state[body_idx].angular_gradient += j_ang * fi;
				m_solve_state[body_idx].angular_hessian += outer_product(j_ang, j_ang) * pen;
				m_solve_state[body_idx].hessian_xtheta += outer_product(j_lin, j_ang) * pen;
				accumulate_geometric_stiffness(m_solve_state[body_idx], r, perps[k], std::abs(fi));
			}
		}

		if (m_bodies[body_idx].update_orientation) {
			const quat q_error = body_b.predicted_orientation * conjugate(body_a.predicted_orientation) * conjugate(constraint.rest_orientation);
			const vec3<angle> theta = to_axis_angle(q_error);
			const unitless::vec3 dirs[3] = { { 1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f, 1.f } };

			for (int k = 0; k < 3; ++k) {
				const angle c_ang = theta[k] - constraint.ang_c0[k] * alpha;
				const angle f_ang = constraint.ang_penalty[k] * c_ang + constraint.ang_lambda[k];

				const unitless::vec3 j_ang = dirs[k] * (-sign);
				m_solve_state[body_idx].angular_gradient += j_ang * f_ang.as<radians>();
				m_solve_state[body_idx].angular_hessian += outer_product(j_ang, j_ang) * constraint.ang_penalty[k];
			}
		}
	}
}

auto gse::vbd::solver::update_joint_dual(const time_squared h_squared) -> void {
	for (auto& j : m_graph.joint_constraints()) {
		const auto& body_a = m_bodies[j.body_a];
		const auto& body_b = m_bodies[j.body_b];

		if ((body_a.locked || body_a.sleeping()) && (body_b.locked || body_b.sleeping())) {
			continue;
		}

		const vec3<length> r_aw = rotate_vector(body_a.predicted_orientation, j.local_anchor_a);
		const vec3<length> r_bw = rotate_vector(body_b.predicted_orientation, j.local_anchor_b);
		const vec3<length> p_a = body_a.predicted_position + r_aw;
		const vec3<length> p_b = body_b.predicted_position + r_bw;
		const vec3<length> d = p_a - p_b;

		if (j.type == joint_type::distance) {
			const float d_mag = magnitude(d).as<meters>();
			const length C = (meters(d_mag) - j.target_distance) - j.pos_c0[0];
			j.pos_lambda[0] = meters(j.pos_penalty[0].as<newtons_per_meter>() * C.as<meters>() + j.pos_lambda[0].as<meters>());
			const stiffness penalty_cap = (j.compliance > inverse_mass{})
				? std::min<stiffness>(1.f / (j.compliance * h_squared), m_config.penalty_max)
				: m_config.penalty_max;
			j.pos_penalty[0] = std::min(j.pos_penalty[0] + m_config.beta * abs(C).as<meters>(), penalty_cap);
		}
		else if (j.type == joint_type::fixed || j.type == joint_type::hinge) {
			for (int k = 0; k < 3; ++k) {
				const unitless::vec3 dirs[3] = { { 1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f, 1.f } };
				const length c = dot(dirs[k], d) - j.pos_c0[k];
				j.pos_lambda[k] = meters(j.pos_penalty[k].as<newtons_per_meter>() * c.as<meters>() + j.pos_lambda[k].as<meters>());
				j.pos_penalty[k] = std::min(j.pos_penalty[k] + m_config.beta * abs(c).as<meters>(), m_config.penalty_max);
			}

			const quat q_error = body_b.predicted_orientation * conjugate(body_a.predicted_orientation) * conjugate(j.rest_orientation);
			const vec3<angle> theta = to_axis_angle(q_error);

			if (j.type == joint_type::fixed) {
				for (int k = 0; k < 3; ++k) {
					const angle c_ang = theta[k] - j.ang_c0[k];
					j.ang_lambda[k] = j.ang_penalty[k] * c_ang + j.ang_lambda[k];
					j.ang_penalty[k] = std::min(j.ang_penalty[k] + m_config.beta.as<newtons_per_meter>() * abs(c_ang).as<radians>(), m_config.penalty_max.as<newtons_per_meter>());
				}
			}
			else {
				const unitless::vec3 axis_a = rotate_axis(body_a.predicted_orientation, j.local_axis_a);
				const unitless::vec3 axis_b = rotate_axis(body_b.predicted_orientation, j.local_axis_b);
				const unitless::vec3 swing_error = cross(axis_a, axis_b);

				unitless::vec3 perp_u = cross(axis_a, unitless::vec3{ 0.f, 1.f, 0.f });
				if (magnitude(perp_u) < 1e-6f) {
					perp_u = cross(axis_a, unitless::vec3{ 1.f, 0.f, 0.f });
				}
				perp_u = normalize(perp_u);
				const unitless::vec3 perp_v = normalize(cross(axis_a, perp_u));

				const angle c_u = radians(dot(perp_u, swing_error)) - j.ang_c0[0];
				const angle c_v = radians(dot(perp_v, swing_error)) - j.ang_c0[1];

				j.ang_lambda[0] = j.ang_penalty[0] * c_u + j.ang_lambda[0];
				j.ang_penalty[0] = std::min(j.ang_penalty[0] + m_config.beta.as<newtons_per_meter>() * abs(c_u).as<radians>(), m_config.penalty_max.as<newtons_per_meter>());

				j.ang_lambda[1] = j.ang_penalty[1] * c_v + j.ang_lambda[1];
				j.ang_penalty[1] = std::min(j.ang_penalty[1] + m_config.beta.as<newtons_per_meter>() * abs(c_v).as<radians>(), m_config.penalty_max.as<newtons_per_meter>());

				if (j.limits_enabled) {
					const angle twist = dot(axis_a, theta);
					const angle lower = j.limit_lower;
					const angle upper = j.limit_upper;

					if (twist < lower) {
						const angle c_limit = (twist - lower) - j.limit_c0;
						j.limit_lambda = std::min(j.limit_penalty * c_limit + j.limit_lambda, angle{});
						j.limit_penalty = std::min(j.limit_penalty + m_config.beta.as<newtons_per_meter>() * abs(c_limit).as<radians>(), m_config.penalty_max.as<newtons_per_meter>());
					}
					else if (twist > upper) {
						const angle c_limit = (twist - upper) - j.limit_c0;
						j.limit_lambda = std::max(j.limit_penalty * c_limit + j.limit_lambda, angle{});
						j.limit_penalty = std::min(j.limit_penalty + m_config.beta.as<newtons_per_meter>() * abs(c_limit).as<radians>(), m_config.penalty_max.as<newtons_per_meter>());
					}
				}
			}
		}
		else if (j.type == joint_type::slider) {
			const unitless::vec3 axis_w = normalize(rotate_axis(body_a.predicted_orientation, j.local_axis_a));
			unitless::vec3 perp0 = cross(axis_w, unitless::vec3{ 0.f, 1.f, 0.f });
			if (magnitude(perp0) < 1e-6f) {
				perp0 = cross(axis_w, unitless::vec3{ 1.f, 0.f, 0.f });
			}
			perp0 = normalize(perp0);
			const unitless::vec3 perp1 = normalize(cross(axis_w, perp0));

			const unitless::vec3 perps[2] = { perp0, perp1 };
			for (int k = 0; k < 2; ++k) {
				const length c = dot(perps[k], d) - j.pos_c0[k];
				j.pos_lambda[k] = meters(j.pos_penalty[k].as<newtons_per_meter>() * c.as<meters>() + j.pos_lambda[k].as<meters>());
				j.pos_penalty[k] = std::min(j.pos_penalty[k] + m_config.beta * abs(c).as<meters>(), m_config.penalty_max);
			}

			const quat q_error = body_b.predicted_orientation * conjugate(body_a.predicted_orientation) * conjugate(j.rest_orientation);
			const vec3<angle> theta = to_axis_angle(q_error);

			for (int k = 0; k < 3; ++k) {
				const angle c_ang = theta[k] - j.ang_c0[k];
				j.ang_lambda[k] = j.ang_penalty[k] * c_ang + j.ang_lambda[k];
				j.ang_penalty[k] = std::min(j.ang_penalty[k] + m_config.beta.as<newtons_per_meter>() * abs(c_ang).as<radians>(), m_config.penalty_max.as<newtons_per_meter>());
			}
		}
	}
}

auto gse::vbd::rotate_axis(const quat& q, const unitless::vec3& v) -> unitless::vec3 {
	const unitless::vec3 u{ q[1], q[2], q[3] };
	const float s = q[0];
	const unitless::vec3 t = 2.f * cross(u, v);
	return v + s * t + cross(u, t);
}

auto gse::vbd::accumulate_geometric_stiffness(body_solve_state& state, const unitless::vec3& r, const unitless::vec3& dir, const float abs_force) -> void {
	const float rd = dot(r, dir);
	for (int j = 0; j < 3; ++j) {
		float col_norm = 0.f;
		for (int i = 0; i < 3; ++i) {
			float h = r[i] * dir[j];
			if (i == j) {
				h -= rd;
			}
			col_norm += std::abs(h);
		}
		state.angular_hessian[j][j] += col_norm * abs_force;
	}
}

auto gse::vbd::contact_effective_mass(const body_state& body_a, const body_state& body_b, const vec3<length>& r_aw, const vec3<length>& r_bw, const unitless::vec3& dir) -> mass {
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
