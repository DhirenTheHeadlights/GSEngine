module gse.physics:vbd_solver_impl;

import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.log;
import gse.math;
import std;

import :contact_manifold;
import :motion_component;
import :vbd_constraint_graph;
import :vbd_constraints;
import :vbd_contact_cache;
import :vbd_solver;

auto gse::vbd::solver::configure(const solver_config& cfg) -> void {
	m_config = cfg;
}

auto gse::vbd::solver::set_color_grain(const std::size_t grain) -> void {
	m_color_grain = std::max<std::size_t>(grain, 1);
}

auto gse::vbd::solver::set_ordered_sweep(const bool enabled) -> void {
	m_ordered_sweep = enabled;
}

auto gse::vbd::solver::config() const -> const solver_config& {
	return m_config;
}

auto gse::vbd::solver::begin_frame(const std::span<const body_state> bodies, const std::span<const id> body_ids, contact_cache& cache) -> void {
	m_bodies.assign(bodies.begin(), bodies.end());
	m_body_ids.assign(body_ids.begin(), body_ids.end());
	m_solve_state.resize(m_bodies.size());
	m_graph.clear();

	{
		trace::scope_guard _{ trace_id<"vbd_cpu::cache_advance">() };
		cache.advance();
	}
}

auto gse::vbd::solver::seed_previous_velocities(const std::span<const vec3<velocity>> velocities) -> void {
	m_prev_velocity.assign(velocities.begin(), velocities.end());
	m_accel_weight.assign(velocities.size(), 0.f);
}

auto gse::vbd::solver::seed_carried(const std::span<const vec3<velocity>> velocities, const std::span<const float> weights) -> void {
	m_prev_velocity.assign(velocities.begin(), velocities.end());
	m_accel_weight.assign(weights.begin(), weights.end());
}

auto gse::vbd::solver::seed_accel_weight(const std::uint32_t body, const float weight, const time_step dt) -> void {
	if (m_prev_velocity.size() != m_bodies.size()) {
		m_prev_velocity.assign(m_bodies.size(), vec3<velocity>{});
		m_accel_weight.assign(m_bodies.size(), 0.f);
	}
	const vec3<acceleration> gravity = { meters_per_second_squared(0.f), -gravity_magnitude(), meters_per_second_squared(0.f) };
	m_prev_velocity[body] = m_bodies[body].velocity - gravity * dt * weight;
	m_accel_weight[body] = weight;
}

auto gse::vbd::solver::previous_velocities() const -> std::span<const vec3<velocity>> {
	return m_prev_velocity;
}

auto gse::vbd::solver::accel_weights() const -> std::span<const float> {
	return m_accel_weight;
}

auto gse::vbd::solver::convergence_counts() const -> std::span<const std::uint32_t> {
	return m_convergence_counts;
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

auto gse::vbd::solver::add_muscle_constraint(const muscle_constraint& m, const std::span<const muscle_path_point> path) -> void {
	m_graph.add_muscle(m, path);
}

auto gse::vbd::solver::set_curve_words(const std::span<const std::uint32_t> words) -> void {
	if (words.size() != m_curve_words.size()) {
		m_curve_words.assign(words.begin(), words.end());
	}
}

auto gse::vbd::solver::solve(const time_step dt) -> void {
	const auto probe = m_config.convergence_probe_iterations != 0
		&& m_probe_step++ % std::max(m_config.convergence_probe_interval, 1u) == 0;
	if (!probe) {
		solve_pass(dt);
		return;
	}
	constexpr solver_config defaults{};
	auto reference = *this;
	reference.m_config.iterations = m_config.convergence_probe_iterations;
	reference.m_config.beta = defaults.beta;
	reference.m_config.ang_beta = defaults.ang_beta;
	reference.m_config.gamma = defaults.gamma;
	reference.m_config.adaptive = 0;
	reference.solve_pass(dt);
	solve_pass(dt);
	report_convergence(reference, dt);
}

auto gse::vbd::solver::joint_extremes_of(const time_step dt) const -> joint_extremes {
	joint_extremes out;
	const float target_scale = m_config.post_stabilize ? 0.f : m_config.alpha;
	const auto joints = m_graph.joint_constraints();
	for (std::uint32_t ji = 0; ji < joints.size(); ++ji) {
		const auto& source = joints[ji];
		const auto& body_a = m_bodies[source.body_a];
		const auto& body_b = m_bodies[source.body_b];
		if ((body_a.locked || body_a.sleeping()) && (body_b.locked || body_b.sleeping())) {
			continue;
		}
		auto j = source;
		compute_joint_c0(j, body_a, body_b, m_curve_words);
		out.worst_gap = std::max(out.worst_gap, magnitude(j.pos_c0));
		out.worst_start_gap = std::max(out.worst_start_gap, magnitude(source.pos_c0));
		const float carried = joint_carried_fraction(source.pos_c0, target_scale, m_config.joint_correction_speed, dt);
		if (const displacement residual = magnitude(j.pos_c0 - source.pos_c0 * carried); residual > out.worst_residual) {
			out.worst_residual = residual;
			out.worst_residual_joint = ji;
			out.worst_residual_body_a = source.body_a;
			out.worst_residual_body_b = source.body_b;
			out.worst_residual_coordinates = j.type == joint_type::coupled
				? coupled_coordinates_of(m_curve_words, j.curve_words, body_a.orientation, body_b.orientation)
				: vec3<angle>{};
			out.worst_residual_ang_lambda = magnitude(j.ang_lambda);
			out.worst_residual_penalty = std::max({ j.pos_penalty[0], j.pos_penalty[1], j.pos_penalty[2] });
		}
		out.worst_lambda = std::max(out.worst_lambda, magnitude(j.pos_lambda));
	}
	return out;
}

auto gse::vbd::solver::report_convergence(const solver& reference_solver, const time_step dt) const -> void {
	const std::span<const body_state> reference = reference_solver.m_bodies;
	auto count = 0u;
	length linear_sum{};
	length linear_max{};
	angle angular_sum{};
	angle angular_max{};
	velocity speed_max{};
	for (std::size_t i = 0; i < m_bodies.size(); ++i) {
		const auto& body = m_bodies[i];
		if (body.locked || body.sleeping() || body.mass <= mass{}) {
			continue;
		}
		const length linear = magnitude(body.predicted_position - reference[i].predicted_position);
		const angle angular = magnitude(difference_axis_angle(reference[i].predicted_orientation, body.predicted_orientation));
		linear_sum += linear;
		angular_sum += angular;
		linear_max = std::max(linear_max, linear);
		angular_max = std::max(angular_max, angular);
		speed_max = std::max(speed_max, magnitude(body.velocity - reference[i].velocity));
		++count;
	}
	if (count == 0) {
		return;
	}
	const auto run_joints = joint_extremes_of(dt);
	const auto reference_joints = reference_solver.joint_extremes_of(dt);
	log::println(
		log::category::physics,
		"vbd convergence probe: bodies={} iterations={} reference={} beta={} ang_beta={} gamma={:.4f} linear_mean={:.6e} linear_max={:.6e} angular_mean={:.6e} angular_max={:.6e} velocity_max={:.6e} joint_gap={:.6e} joint_gap_ref={:.6e} joint_lambda={:.6e} joint_lambda_ref={:.6e} joint_start_gap={:.6e} joint_residual={:.6e} joint_residual_ref={:.6e} residual_joint={} residual_joint_ref={} residual_bodies_ref={}-{} residual_coordinates_ref=({:.1f:deg} {:.1f:deg} {:.1f:deg}) residual_ang_lambda_ref={:.3e} residual_penalty_ref={:.3e}",
		count,
		m_config.iterations,
		m_config.convergence_probe_iterations,
		m_config.beta,
		m_config.ang_beta,
		m_config.gamma,
		linear_sum / static_cast<float>(count),
		linear_max,
		angular_sum / static_cast<float>(count),
		angular_max,
		speed_max,
		run_joints.worst_gap,
		reference_joints.worst_gap,
		run_joints.worst_lambda,
		reference_joints.worst_lambda,
		run_joints.worst_start_gap,
		run_joints.worst_residual,
		reference_joints.worst_residual,
		run_joints.worst_residual_joint,
		reference_joints.worst_residual_joint,
		reference_joints.worst_residual_body_a,
		reference_joints.worst_residual_body_b,
		reference_joints.worst_residual_coordinates[0],
		reference_joints.worst_residual_coordinates[1],
		reference_joints.worst_residual_coordinates[2],
		reference_joints.worst_residual_ang_lambda,
		reference_joints.worst_residual_penalty
	);
}

auto gse::vbd::solver::solve_pass(const time_step dt) -> void {
	trace::scope_guard _{ trace_id<"vbd::solve">() };

	const auto num_bodies = static_cast<std::uint32_t>(m_bodies.size());
	const time_squared h_squared = dt * dt;

	m_graph.sort_contacts_canonical();

	{
		trace::scope_guard _{ trace_id<"vbd::coloring">() };
		m_body_inactive.resize(num_bodies);
		for (std::uint32_t i = 0; i < num_bodies; ++i) {
			m_body_inactive[i] = static_cast<std::uint8_t>(m_bodies[i].locked || m_bodies[i].sleeping());
		}
		m_graph.compute_coloring(num_bodies, m_body_inactive);
	}

	{
		const auto& contacts_ref = m_graph.contact_constraints();
		m_active_contacts.clear();
		m_active_contacts.reserve(contacts_ref.size());
		for (std::uint32_t ci = 0; ci < contacts_ref.size(); ++ci) {
			const auto& c = contacts_ref[ci];
			if (m_body_inactive[c.body_a] && m_body_inactive[c.body_b]) {
				continue;
			}
			m_active_contacts.push_back(ci);
		}
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
	for (const auto bi : m_graph.overflow_bodies()) {
		m_body_in_color_group[bi] = true;
	}

	{
		auto& contacts_ref = m_graph.contact_constraints();
		task::coarse_parallel(
			m_active_contacts.size(),
			512,
			[&, this](std::size_t k) {
				auto& c = contacts_ref[m_active_contacts[k]];
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
			trace_id<"vbd::warm_contacts">()
		);
	}

	{
		auto& contacts_ref = m_graph.contact_constraints();
		task::coarse_parallel(
			m_active_contacts.size(),
			256,
			[&, this](std::size_t k) {
				auto& c = contacts_ref[m_active_contacts[k]];
				const auto& ba = m_bodies[c.body_a];
				const auto& bb = m_bodies[c.body_b];

				const vec3<lever_arm> r_aw = rotate_vector(ba.orientation, c.local_anchor_a);
				const vec3<lever_arm> r_bw = rotate_vector(bb.orientation, c.local_anchor_b);

				const stiffness base_floor = std::max(c.penalty_floor, m_config.penalty_min);
				const bool active_normal = c.lambda[0] < newtons(-1e-3f) || c.c0[0] < meters(-1e-4f);

				const stiffness normal_floor = active_normal
					? std::clamp<stiffness>(
						contact_effective_mass(ba, bb, r_aw, r_bw, c.normal) / h_squared,
						base_floor,
						m_config.penalty_max
					  )
					: base_floor;

				const std::array row_floor = {
					normal_floor,
					c.sticking ? std::clamp<stiffness>(
						contact_effective_mass(ba, bb, r_aw, r_bw, c.tangent_u) / h_squared,
						m_config.penalty_min,
						m_config.penalty_max
				)
							   : m_config.penalty_min,
					c.sticking ? std::clamp<stiffness>(
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
			trace_id<"vbd::contact_penalty">()
		);
	}

	{
		auto& joints_ref = m_graph.joint_constraints();
		task::coarse_parallel(
			joints_ref.size(),
			32,
			[&, this](std::size_t ji) {
				warm_start_joint(
					joints_ref[ji],
					m_bodies[joints_ref[ji].body_a],
					m_bodies[joints_ref[ji].body_b],
					h_squared,
					m_config,
					m_curve_words
				);
			},
			trace_id<"vbd::warm_joints">()
		);
	}

	{
		auto& joints_ref = m_graph.joint_constraints();
		task::coarse_parallel(
			joints_ref.size(),
			32,
			[&, this](std::size_t ji) {
				compute_joint_c0(joints_ref[ji], m_bodies[joints_ref[ji].body_a], m_bodies[joints_ref[ji].body_b], m_curve_words);
			},
			trace_id<"vbd::joint_c0">()
		);
	}

	{
		auto& muscles_ref = m_graph.muscle_constraints();
		task::coarse_parallel(
			muscles_ref.size(),
			32,
			[&, this](std::size_t mi) {
				auto& m = muscles_ref[mi];
				advance_muscle_activation(m, dt);
				m.path_length_start = muscle_start_path_length(m_graph.muscle_path(m), m_graph.muscle_wraps(m), m_bodies, m_curve_words);
			},
			trace_id<"vbd::warm_muscles">()
		);
	}

	m_muscle_floor.assign(num_bodies, stiffness{});
	if (!m_graph.muscle_constraints().empty()) {
		task::coarse_parallel(
			num_bodies,
			64,
			[&, this](std::size_t bi) {
				m_muscle_floor[bi] = muscle_floor_of(static_cast<std::uint32_t>(bi), dt);
			},
			trace_id<"vbd::muscle_floor">()
		);
	}

	const acceleration gravity_mag = gravity_magnitude();
	const vec3<acceleration> gravity = { meters_per_second_squared(0.f), -gravity_mag, meters_per_second_squared(0.f) };

	if (m_prev_velocity.size() != m_bodies.size()) {
		m_prev_velocity.assign(
			m_bodies.size(),
			vec3<velocity>{}
		);
		m_accel_weight.assign(m_bodies.size(), 0.f);
	}

	{
		task::coarse_parallel(
			num_bodies,
			64,
			[&, this](std::size_t i) {
				auto& body = m_bodies[i];
				if (body.locked) {
					m_accel_weight[i] = 0.f;
					m_prev_velocity[i] = body.velocity;
					body.old_position = body.position;
					body.old_orientation = body.orientation;
					body.predicted_position = body.position + body.velocity * dt;
					const vec3<angle> aa = body.angular_velocity * dt;
					if (magnitude(aa) > radians(1e-7f)) {
						body.predicted_orientation = normalize(from_axis_angle_vector(aa) * body.orientation);
					}
					else {
						body.predicted_orientation = body.orientation;
					}
					body.inertia_target = body.predicted_position;
					body.angular_inertia_target = body.predicted_orientation;
					body.motor_target = body.predicted_position;
					return;
				}
				if (body.sleeping()) {
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
			trace_id<"vbd::predict_bodies">()
		);
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

	{
		trace::scope_guard _{ trace_id<"vbd::islands">() };
		build_islands(num_bodies);
	}

	const velocity wake_thresh = m_config.velocity_sleep_threshold * 10.f;
	for (std::uint32_t i = 0; i < num_bodies; ++i) {
		const auto& body = m_bodies[i];
		if (body.locked || body.sleeping()) {
			continue;
		}
		if (magnitude(body.velocity) > wake_thresh) {
			m_island_wake[m_island_root[i]] = 1;
		}
	}
	for (std::uint32_t i = 0; i < num_bodies; ++i) {
		auto& body = m_bodies[i];
		if (body.locked || !body.sleeping()) {
			continue;
		}
		if (m_island_wake[m_island_root[i]]) {
			body.sleep_counter = 0;
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

	for (const auto& m : m_graph.muscle_constraints()) {
		const auto path = m_graph.muscle_records(m);
		const bool awake = std::ranges::any_of(path, [this](const muscle_path_point& p) {
			return !m_bodies[p.body].locked && !m_bodies[p.body].sleeping();
		});
		if (!awake) {
			continue;
		}
		for (const auto& p : path) {
			if (m_bodies[p.body].sleeping()) {
				m_bodies[p.body].sleep_counter = 0;
			}
		}
	}

	const auto& contacts = m_graph.contact_constraints();
	const auto& motors = m_graph.motor_constraints();
	const auto& joints = m_graph.joint_constraints();

	const int num_iterations = static_cast<int>(m_config.iterations);
	const float solve_alpha = m_config.post_stabilize ? 1.0f : m_config.alpha;

	{
		m_frozen_jacobians.resize(contacts.size());
		task::coarse_parallel(
			m_active_contacts.size(),
			256,
			[&, this](std::size_t k) {
				const std::uint32_t ci = m_active_contacts[k];
				const auto& c = contacts[ci];
				m_frozen_jacobians[ci] = contact_jacobian_of(c, m_bodies[c.body_a], m_bodies[c.body_b]);
			},
			trace_id<"vbd::frozen_jacobians">()
		);
	}

	if (m_ordered_sweep) {
		m_sweep_order.clear();
		for (std::uint32_t i = 0; i < num_bodies; ++i) {
			if (m_body_inactive[i]) {
				continue;
			}
			m_sweep_order.push_back(i);
		}
		std::ranges::sort(m_sweep_order, [this](const std::uint32_t a, const std::uint32_t b) {
			if (m_bodies[a].position.y() != m_bodies[b].position.y()) {
				return m_bodies[a].position.y() < m_bodies[b].position.y();
			}
			return a < b;
		});
	}

	auto solve_iteration_gauss_seidel = [&](const float alpha) {
		const std::size_t color_grain = m_color_grain;
		const std::size_t parallel_threshold = color_grain * 2;

		for (const auto& body_color : m_graph.body_colors()) {
			if (body_color.size() < parallel_threshold) {
				trace::scope_guard _{ trace_id<"vbd::gs_color_serial">() };
				for (const auto bi : body_color) {
					if (m_graph.is_jointed(bi)) {
						continue;
					}
					step_colored_body(bi, h_squared, dt, alpha, false);
				}
				continue;
			}

			task::coarse_parallel(
				body_color.size(),
				color_grain,
				[&, this](std::size_t k) {
					const auto bi = body_color[k];
					if (m_graph.is_jointed(bi)) {
						return;
					}
					step_colored_body(bi, h_squared, dt, alpha, false);
				},
				trace_id<"vbd::gs_color_iter">()
			);
		}

		if (const auto islands = m_graph.islands(); !islands.empty()) {
			const auto sweep_island = [&, this](const std::vector<std::uint32_t>& island) {
				for (const auto bi : island) {
					step_colored_body(bi, h_squared, dt, alpha, true);
				}
				for (const auto bi : std::views::reverse(island)) {
					step_colored_body(bi, h_squared, dt, alpha, true);
				}
			};

			if (m_graph.islands_contact_disjoint()) {
				task::coarse_parallel(
					islands.size(),
					1,
					[&](std::size_t k) {
						sweep_island(islands[k]);
					},
					trace_id<"vbd::gs_island_iter">()
				);
			}
			else {
				trace::scope_guard _{ trace_id<"vbd::gs_island_serial">() };
				for (const auto& island : islands) {
					sweep_island(island);
				}
			}
		}

		if (!m_graph.overflow_bodies().empty()) {
			trace::scope_guard _{ trace_id<"vbd::gs_overflow_serial">() };
			for (const auto bi : m_graph.overflow_bodies()) {
				step_colored_body(bi, h_squared, dt, alpha, false);
			}
		}

		for (const auto& motor : motors) {
			if (m_bodies[motor.body_index].locked || m_bodies[motor.body_index].sleeping()) {
				continue;
			}
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
		task::coarse_parallel(
			num_bodies,
			32,
			[&, this](std::size_t bi) {
				if (m_bodies[bi].locked || m_bodies[bi].sleeping()) {
					return;
				}
				m_solve_state[bi] = {};
				for (const auto ci : m_graph.body_contact_indices(bi)) {
					accumulate_contact(contacts[ci], m_frozen_jacobians[ci],
						static_cast<std::uint32_t>(bi),
						alpha,
						false);
				}
				for (const auto ji : m_graph.body_joint_indices(bi)) {
					accumulate_joint(joints[ji], static_cast<std::uint32_t>(bi), h_squared, dt, alpha);
				}
				for (const auto mi : m_graph.body_muscle_indices(bi)) {
					accumulate_muscle(m_graph.muscle_constraints()[mi], static_cast<std::uint32_t>(bi), dt);
				}
				if (const auto mi = m_body_motor_index[bi]; mi != no_motor) {
					accumulate_motor(motors[mi], h_squared);
				}
			},
			trace_id<"vbd::jacobi_accum">()
		);

		task::coarse_parallel(
			num_bodies,
			128,
			[&, this](std::size_t bi) {
				if (m_bodies[bi].locked || m_bodies[bi].sleeping()) {
					return;
				}
				perform_newton_step(static_cast<std::uint32_t>(bi), h_squared);
			},
			trace_id<"vbd::jacobi_step">()
		);
	};

	bool sweep_forward = true;

	auto solve_iteration_ordered = [&](const float alpha) {
		trace::scope_guard _{ trace_id<"vbd::ordered_sweep">() };
		if (sweep_forward) {
			for (const auto bi : m_sweep_order) {
				step_colored_body(bi, h_squared, dt, alpha, false);
			}
		}
		else {
			for (const auto bi : m_sweep_order | std::views::reverse) {
				step_colored_body(bi, h_squared, dt, alpha, false);
			}
		}
		sweep_forward = !sweep_forward;
	};

	auto solve_iteration = [&](const float alpha) {
		if (m_ordered_sweep) {
			solve_iteration_ordered(alpha);
		}
		else if (m_config.use_jacobi) {
			solve_iteration_jacobi(alpha);
		}
		else {
			solve_iteration_gauss_seidel(alpha);
		}
	};

	{
		trace::scope_guard _{ trace_id<"vbd::iterations">() };
		const bool adaptive = m_config.adaptive != 0;
		const bool trace_convergence = m_config.trace_island_convergence != 0;
		if (trace_convergence) {
			m_convergence_counts.fill(0);
		}
		length linear_threshold = m_config.convergence_threshold_linear;
		angle angular_threshold = m_config.convergence_threshold_angular;
		if ((adaptive || trace_convergence) && m_config.convergence_speed_scale > 0.f) {
			velocity max_speed{};
			angular_velocity max_angular_speed{};
			for (const auto& body : m_bodies) {
				if (!body.locked && !body.sleeping() && body.mass > mass{}) {
					max_speed = std::max(max_speed, magnitude(body.velocity));
					max_angular_speed = std::max(max_angular_speed, magnitude(body.angular_velocity));
				}
			}
			linear_threshold = std::max(linear_threshold, max_speed * dt * m_config.convergence_speed_scale);
			angular_threshold = std::max(angular_threshold, max_angular_speed * dt * m_config.convergence_speed_scale);
		}
		step_delta previous_violation{
			.linear = meters(std::numeric_limits<float>::max()),
			.angular = radians(std::numeric_limits<float>::max())
		};
		for (int it = 0; it < num_iterations; ++it) {
			solve_iteration(solve_alpha);
			const auto contact_delta = update_dual(solve_alpha, it);
			const auto joint_delta = update_joint_dual(h_squared, dt, solve_alpha, it);
			if (m_config.trace_body != 0xFFFFFFFFu) {
				log::println(
					log::category::physics,
					"cpu residual it {}: contact_lin {:.9f} contact_ang {:.9f} joint_lin {:.9f} joint_ang {:.9f}",
					it,
					contact_delta.linear,
					contact_delta.angular,
					joint_delta.linear,
					joint_delta.angular
				);
			}
			const length linear = std::max(contact_delta.linear, joint_delta.linear);
			const angle angular = std::max(contact_delta.angular, joint_delta.angular);
			if (trace_convergence) {
				const auto slot = std::min<std::uint32_t>(static_cast<std::uint32_t>(it), limits.iteration_trace_slots - 1);
				++m_convergence_counts[slot];
				if (linear < linear_threshold && angular < angular_threshold) {
					++m_convergence_counts[limits.iteration_trace_slots + slot];
				}
			}
			if (!adaptive) {
				continue;
			}
			if (linear < linear_threshold && angular < angular_threshold) {
				break;
			}
			if (linear > previous_violation.linear * 0.99f && angular > previous_violation.angular * 0.99f) {
				break;
			}
			previous_violation = {
				.linear = linear,
				.angular = angular
			};
		}
	}

	{
		task::coarse_parallel(
			m_bodies.size(),
			128,
			[&, this](std::size_t i) {
				auto& body = m_bodies[i];
				if (!body.locked && !body.sleeping() && body.mass > mass{}) {
					body.velocity = (body.predicted_position - body.old_position) / dt;

					if (body.update_orientation) {
						body.angular_velocity = difference_axis_angle(body.old_orientation, body.predicted_orientation) /
							dt;
					}
				}
			},
			trace_id<"vbd::velocity_writeback">()
		);
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

		const vec3<lever_arm> rAW = rotate_vector(ba.predicted_orientation, c.local_anchor_a);
		const vec3<lever_arm> rBW = rotate_vector(bb.predicted_orientation, c.local_anchor_b);

		const auto va = ba.velocity + cross(ba.angular_velocity, rAW) / rad;
		const auto vb = bb.velocity + cross(bb.angular_velocity, rBW) / rad;
		const velocity v_rel_after = dot(c.normal, va - vb);

		const float speed_ratio =
			std::clamp(
				(-c.approach_speed - m_config.restitution_threshold) / m_config.restitution_threshold,
				0.f,
				1.f
			);
		const velocity v_target = -c.restitution * speed_ratio * c.approach_speed;
		const velocity dv = v_target - v_rel_after;

		if (dv <= velocity{}) {
			continue;
		}

		const mass effective = contact_effective_mass(ba, bb, rAW, rBW, c.normal);
		if (effective <= mass{}) {
			continue;
		}

		const vec3<impulse> j = c.normal * (dv * effective);

		if (!ba.locked) {
			ba.velocity += j * ba.inverse_mass();
			if (ba.update_orientation) {
				ba.angular_velocity += ba.inv_inertia * (cross(rAW, j) / rad);
			}
		}
		if (!bb.locked) {
			bb.velocity -= j * bb.inverse_mass();
			if (bb.update_orientation) {
				bb.angular_velocity -= bb.inv_inertia * (cross(rBW, j) / rad);
			}
		}
	}

	if (m_config.post_stabilize) {
		trace::scope_guard _{ trace_id<"vbd::post_stabilize">() };
		solve_iteration(0.0f);
	}

	for (std::uint32_t i = 0; i < num_bodies; ++i) {
		const auto& body = m_bodies[i];
		if (body.locked || body.sleeping()) {
			continue;
		}
		if (magnitude(body.velocity) > wake_thresh) {
			m_island_calm[m_island_root[i]] = 0;
		}
	}

	{
		task::coarse_parallel(
			m_bodies.size(),
			128,
			[&, this](std::size_t i) {
				auto& body = m_bodies[i];
				if (body.sleeping()) {
					return;
				}
				if (body.locked) {
					body.position = body.predicted_position;
					body.orientation = body.predicted_orientation;
					return;
				}

				if (m_island_calm[m_island_root[i]] && magnitude(body.velocity) < m_config.velocity_sleep_threshold && magnitude(body.angular_velocity) < m_config.angular_sleep_threshold) {
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
			trace_id<"vbd::sleep_writeback">()
		);
	}
}

auto gse::vbd::solver::build_islands(const std::uint32_t num_bodies) -> void {
	m_island_root.resize(num_bodies);
	std::ranges::iota(m_island_root, 0u);

	const auto find = [this](std::uint32_t i) {
		while (m_island_root[i] != i) {
			m_island_root[i] = m_island_root[m_island_root[i]];
			i = m_island_root[i];
		}
		return i;
	};

	const auto unite = [&](const std::uint32_t a, const std::uint32_t b) {
		if (m_bodies[a].locked || m_bodies[b].locked) {
			return;
		}
		const auto root_a = find(a);
		const auto root_b = find(b);
		if (root_a != root_b) {
			m_island_root[root_a] = root_b;
		}
	};

	for (const auto& c : m_graph.contact_constraints()) {
		unite(c.body_a, c.body_b);
	}
	for (const auto& j : m_graph.joint_constraints()) {
		unite(j.body_a, j.body_b);
	}

	for (std::uint32_t i = 0; i < num_bodies; ++i) {
		m_island_root[i] = find(i);
	}

	m_island_wake.assign(num_bodies, 0);
	m_island_calm.assign(num_bodies, 1);
}

auto gse::vbd::solver::end_frame(std::vector<body_state>& bodies, contact_cache& cache) -> void {
	{
		trace::scope_guard _{ trace_id<"vbd_cpu::end_frame::store">() };
		cache.reserve(m_graph.contact_constraints().size());

		for (const auto& c : m_graph.contact_constraints()) {
			if (m_body_inactive[c.body_a] && m_body_inactive[c.body_b]) {
				if (c.replayed & 1u) {
					continue;
				}
				if (cache.touch(m_body_ids[c.body_a], m_body_ids[c.body_b], unpack_feature(c.feature_key))) {
					continue;
				}
			}
			const force friction_bound = abs(c.lambda[0]) * c.friction_coeff;
			const force tangential_lambda = hypot(c.lambda[1], c.lambda[2]);
			const length tangential_gap = hypot(c.c0[1], c.c0[2]);

			const bool sticking = c.lambda[0] < newtons(-1e-3f) && tangential_gap < m_config.stick_threshold &&
				tangential_lambda < friction_bound;

			cache.store(
				m_body_ids[c.body_a],
				m_body_ids[c.body_b],
				unpack_feature(c.feature_key),
				cached_lambda{
					.lambda = c.lambda,
					.penalty = c.penalty,
					.normal = c.normal,
					.tangent_u = c.tangent_u,
					.tangent_v = c.tangent_v,
					.local_anchor_a = c.local_anchor_a,
					.local_anchor_b = c.local_anchor_b,
					.sticking = sticking
				}
			);
		}
	}

	{
		trace::scope_guard _{ trace_id<"vbd_cpu::end_frame::bodies_copy">() };
		bodies.assign(m_bodies.begin(), m_bodies.end());
	}
}


auto gse::vbd::solver::accumulate_contact(const contact_constraint& constraint, const frozen_jacobian& jacobian, const std::uint32_t body_idx, const float alpha, const bool live_jacobians) -> void {
	const auto& body_a = m_bodies[constraint.body_a];
	const auto& body_b = m_bodies[constraint.body_b];
	const bool is_a = (body_idx == constraint.body_a);

	const vec3<predicted_position> p_a = body_a.predicted_position + jacobian.world_r_a;
	const vec3<predicted_position> p_b = body_b.predicted_position + jacobian.world_r_b;
	const vec3<displacement> d = p_a - p_b;

	const vec3<gap> cn = { dot(constraint.normal, d) + m_config.collision_margin,
		dot(constraint.tangent_u, d),
		dot(constraint.tangent_v, d) };
	const vec3<gap> c = cn - constraint.c0 * alpha;

	const force friction_bound = abs(constraint.lambda[0]) * constraint.friction_coeff;
	const std::array penalty = { contact_normal_penalty(constraint), constraint.penalty[1], constraint.penalty[2] };

	const force f0 = std::min<force>(
		penalty[0] * c[0] + constraint.lambda[0],
		force{}
	);
	const force f1 = std::clamp<force>(constraint.penalty[1] * c[1] + constraint.lambda[1],
									   -friction_bound,
		friction_bound);
	const force f2 = std::clamp<force>(constraint.penalty[2] * c[2] + constraint.lambda[2],
									   -friction_bound,
		friction_bound);

	const float sign = is_a ? 1.f : -1.f;
	const std::array dirs = { constraint.normal, constraint.tangent_u, constraint.tangent_v };
	const std::array f = { f0, f1, f2 };
	const auto& j_ang_rows = is_a ? jacobian.j_ang_a : jacobian.j_ang_b;
	const auto& lever = is_a ? jacobian.world_r_a : jacobian.world_r_b;

	for (int i = 0; i < 3; i++) {
		if (i > 0 && friction_bound <= newtons(1e-10f)) {
			continue;
		}
		if (abs(f[i]) < newtons(1e-12f) && penalty[i] < newtons_per_meter(1e-6f)) {
			continue;
		}

		const vec3f j_lin = dirs[i] * sign;
		const vec3<angular_jacobian> j_ang = j_ang_rows[i] * sign;

		m_solve_state[body_idx].gradient += j_lin * f[i];
		m_solve_state[body_idx].hessian += outer_product(j_lin, j_lin) * penalty[i];

		if (m_bodies[body_idx].update_orientation) {
			m_solve_state[body_idx].angular_gradient += j_ang * f[i];
			m_solve_state[body_idx].angular_hessian += outer_product(j_ang, j_ang) * penalty[i];
			m_solve_state[body_idx].hessian_xtheta += outer_product(j_lin, j_ang) * penalty[i];
			if (live_jacobians) {
				accumulate_geometric_stiffness(m_solve_state[body_idx], lever, dirs[i], abs(f[i]));
			}
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

		const bool blocking_body = other_body.locked || other_body.mass >= body.mass;

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

auto gse::vbd::solver::step_colored_body(const std::uint32_t body_idx, const time_squared h_squared, const time_step dt, const float alpha, const bool live_jacobians) -> void {
	if (m_bodies[body_idx].locked || m_bodies[body_idx].sleeping()) {
		return;
	}

	const auto& contacts = m_graph.contact_constraints();
	const auto& joints = m_graph.joint_constraints();
	const auto& motors = m_graph.motor_constraints();

	m_solve_state[body_idx] = {};
	for (const auto ci : m_graph.body_contact_indices(body_idx)) {
		const auto& c = contacts[ci];
		accumulate_contact(
			c,
			live_jacobians ? contact_jacobian_of(c, m_bodies[c.body_a], m_bodies[c.body_b]) : m_frozen_jacobians[ci],
			body_idx,
			alpha,
			live_jacobians
		);
	}
	for (const auto ji : m_graph.body_joint_indices(body_idx)) {
		accumulate_joint(joints[ji], body_idx, h_squared, dt, alpha);
	}
	for (const auto mi : m_graph.body_muscle_indices(body_idx)) {
		accumulate_muscle(m_graph.muscle_constraints()[mi], body_idx, dt);
	}
	if (const auto mi = m_body_motor_index[body_idx]; mi != no_motor) {
		accumulate_motor(motors[mi], h_squared);
	}
	perform_newton_step(body_idx, h_squared);
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

	const vec3<force> g_lin =
		(body.predicted_position - body.inertia_target) * inertia_weight + m_solve_state[body_idx].gradient;

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
	const mat3<angular_stiffness> ang_inertia_hessian = i_body / h_squared;
	const vec3<torque> g_ang_inertia = ang_inertia_hessian * theta_diff;

	const vec3<torque> g_ang = m_solve_state[body_idx].angular_gradient + g_ang_inertia;

	auto h_tt = m_solve_state[body_idx].angular_hessian + ang_inertia_hessian;
	h_tt[0][0] += m_config.angular_regularization;
	h_tt[1][1] += m_config.angular_regularization;
	h_tt[2][2] += m_config.angular_regularization;

	const auto& h_xt = m_solve_state[body_idx].hessian_xtheta;
	const auto h_tx = h_xt.transpose();

	const auto h_xx_inv = h_xx.inverse();
	const auto s = h_tt - h_tx * h_xx_inv * h_xt;
	const auto s_inv = s.inverse();

	auto delta_theta = -(s_inv * (g_ang - h_tx * (h_xx_inv * g_lin)));

	const auto ang_step = magnitude(delta_theta);
	if (ang_step > m_config.max_angular_step) {
		delta_theta *= (m_config.max_angular_step / ang_step);
	}

	auto delta_x = -(h_xx_inv * (g_lin + h_xt * delta_theta));

	const auto lin_step = magnitude(delta_x);
	if (lin_step > m_config.max_linear_step) {
		delta_x *= (m_config.max_linear_step / lin_step);
	}

	body.predicted_position += delta_x * omega;

	if (ang_step > radians(1e-7f)) {
		body.predicted_orientation =
			normalize(from_axis_angle_vector(vec3<angle>(delta_theta) * omega) * body.predicted_orientation);
	}

	return { lin_step, ang_step };
}

auto gse::vbd::solver::update_dual(const float alpha, const int iteration) -> step_delta {
	auto& contacts = m_graph.contact_constraints();
	if (m_config.trace_body != 0xFFFFFFFFu) {
		for (const auto k : m_active_contacts) {
			const auto& con = contacts[k];
			if (con.body_a != m_config.trace_body && con.body_b != m_config.trace_body) {
				continue;
			}
			const auto& body_a = m_bodies[con.body_a];
			const auto& body_b = m_bodies[con.body_b];
			const vec3<predicted_position> p_a = body_a.predicted_position + rotate_vector(body_a.predicted_orientation, con.local_anchor_a);
			const vec3<predicted_position> p_b = body_b.predicted_position + rotate_vector(body_b.predicted_orientation, con.local_anchor_b);
			const gap c = dot(con.normal, p_a - p_b) + m_config.collision_margin - con.c0[0] * alpha;
			log::println(
				log::level::debug,
				log::category::physics,
				"cpu dual it {} ci {}: C {:.7f} lambda {:.4f} pen {:.1f}",
				iteration,
				k,
				c,
				con.lambda[0],
				con.penalty[0]
			);
		}
	}
	const auto worker_count = std::max<std::size_t>(1, task::thread_count());
	std::inplace_vector<length, 64> per_worker_max;
	for (std::size_t i = 0; i < worker_count; ++i) {
		per_worker_max.push_back(length{});
	}

	task::coarse_parallel(
		m_active_contacts.size(),
		512,
		[&, this](std::size_t k) {
			auto& con = contacts[m_active_contacts[k]];
			const auto& body_a = m_bodies[con.body_a];
			const auto& body_b = m_bodies[con.body_b];

			const vec3<lever_arm> rAW = rotate_vector(body_a.predicted_orientation, con.local_anchor_a);
			const vec3<lever_arm> rBW = rotate_vector(body_b.predicted_orientation, con.local_anchor_b);
			const vec3<predicted_position> pA = body_a.predicted_position + rAW;
			const vec3<predicted_position> pB = body_b.predicted_position + rBW;
			const vec3<displacement> d = pA - pB;

			const vec3<gap> cn = { dot(con.normal, d) + m_config.collision_margin,
				dot(con.tangent_u, d),
				dot(con.tangent_v, d) };
			const vec3<gap> c = cn - con.c0 * alpha;

			con.lambda[0] = std::min<force>(
				contact_normal_penalty(con) * c[0] + con.lambda[0],
				force{}
			);

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
		trace_id<"vbd::contact_dual_update">()
	);

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

	const auto coupled = constraint.type == joint_type::coupled
		? std::optional(shared_coupled_frame(m_solve_state[body_idx].newton, m_curve_words, m_bodies, constraint.curve_words, constraint.body_a, constraint.body_b))
		: std::nullopt;
	const vec3<lever_arm> r_aw = coupled
		? vec3<lever_arm>(rotate_vector(body_a.predicted_orientation, constraint.local_anchor_a) + coupled->target_offset)
		: rotate_vector(body_a.predicted_orientation, constraint.local_anchor_a);
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

		const stiffness penalty = constraint.compliance > inverse_mass{} ? constraint.pos_penalty[0] : floored_penalty(constraint.pos_penalty[0], constraint.body_a, constraint.body_b);
		const force fi = penalty * c + constraint.pos_lambda[0];

		const vec3f j_lin = d_hat * sign;
		const vec3<angular_jacobian> j_ang = cross(r, d_hat) / rad;

		m_solve_state[body_idx].gradient += j_lin * fi;
		m_solve_state[body_idx].hessian += outer_product(j_lin, j_lin) * penalty;

		if (m_bodies[body_idx].update_orientation) {
			m_solve_state[body_idx].angular_gradient += j_ang * fi;
			m_solve_state[body_idx].angular_hessian += outer_product(j_ang, j_ang) * penalty;
			m_solve_state[body_idx].hessian_xtheta += outer_product(j_lin, j_ang) * penalty;
			accumulate_geometric_stiffness(m_solve_state[body_idx], r, d_hat, abs(fi));
		}
	}
	else if (constraint.type == joint_type::fixed || constraint.type == joint_type::hinge || constraint.type == joint_type::ball || constraint.type == joint_type::universal || constraint.type == joint_type::coupled) {
		constexpr std::array dirs = { axis_x, axis_y, axis_z };
		const float carried = joint_carried_fraction(constraint.pos_c0, alpha, m_config.joint_correction_speed, dt);

		for (int k = 0; k < 3; ++k) {
			const length c = dot(dirs[k], d) - constraint.pos_c0[k] * carried;
			const stiffness penalty = floored_penalty(constraint.pos_penalty[k], constraint.body_a, constraint.body_b);
			const force fi = penalty * c + constraint.pos_lambda[k];

			const vec3f j_lin = dirs[k] * sign;
			vec3<angular_jacobian> j_ang = cross(r, dirs[k]) / rad;
			if (coupled) {
				j_ang -= coupled_translation_lever(*coupled, dirs[k]) * sign;
			}

			m_solve_state[body_idx].gradient += j_lin * fi;
			m_solve_state[body_idx].hessian += outer_product(j_lin, j_lin) * penalty;

			if (m_bodies[body_idx].update_orientation) {
				m_solve_state[body_idx].angular_gradient += j_ang * fi;
				m_solve_state[body_idx].angular_hessian += outer_product(j_ang, j_ang) * penalty;
				m_solve_state[body_idx].hessian_xtheta += outer_product(j_lin, j_ang) * penalty;
				accumulate_geometric_stiffness(m_solve_state[body_idx], r, dirs[k], abs(fi));
			}
		}

		if (m_bodies[body_idx].update_orientation) {
			const quat q_error = body_b.predicted_orientation * conjugate(body_a.predicted_orientation) *
				conjugate(constraint.rest_orientation);
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
			else if (constraint.type == joint_type::universal) {
				const vec3f axis_a = rotate_vector(body_a.predicted_orientation, constraint.local_axis_a);
				const angle c_twist = dot(axis_a, theta) - constraint.ang_c0[0] * alpha;
				const torque f_twist = constraint.ang_penalty[0] * c_twist + constraint.ang_lambda[0];

				const vec3f j_twist = axis_a * (-sign);
				m_solve_state[body_idx].angular_gradient += j_twist * f_twist;
				m_solve_state[body_idx].angular_hessian += outer_product(j_twist, j_twist) * constraint.ang_penalty[0];
			}
			else if (constraint.type == joint_type::ball) {
				for (int k = 0; k < 3; ++k) {
					if (constraint.soft_ang_stiffness[k] <= angular_stiffness{}) {
						continue;
					}
					const vec3f axis_world = rotate_vector(body_a.predicted_orientation, dirs[k]);
					const angle theta_k = dot(axis_world, theta);
					const torque f_soft = constraint.soft_ang_stiffness[k] * theta_k;

					const vec3f j_soft = axis_world * (-sign);
					m_solve_state[body_idx].angular_gradient += j_soft * f_soft;
					m_solve_state[body_idx].angular_hessian +=
						outer_product(j_soft, j_soft) *
						constraint.soft_ang_stiffness[k];
				}
				for (int k = 0; k < 3; ++k) {
					if ((constraint.limited_rows & (1u << k)) == 0) {
						continue;
					}
					accumulate_angular_limit(
						m_solve_state[body_idx],
						limit_row_of(constraint, body_a.predicted_orientation, theta, k, alpha),
						constraint.ang_lambda[k],
						constraint.ang_penalty[k],
						sign
					);
				}
				for (int k = 0; k < 3; ++k) {
					if (constraint.drive_stiffness[k] <= angular_stiffness{}) {
						continue;
					}
					const vec3f axis_world = rotate_vector(body_a.predicted_orientation, dirs[k]);
					accumulate_joint_drive(
						constraint,
						body_idx,
						axis_world,
						dot(axis_world, theta),
						k,
						sign
					);
				}
			}
			else if (coupled) {
				const vec3<angle> phi = to_axis_angle(coupled->child * conjugate(coupled->target));
				const auto& perps = coupled->perpendiculars;
				for (std::uint32_t k = 0; k < 3 - coupled->count; ++k) {
					const angle c_ang = dot(perps[k], phi) - constraint.ang_c0[k] * alpha;
					const torque f_ang = constraint.ang_penalty[k] * c_ang + constraint.ang_lambda[k];

					const vec3f j_ang = perps[k] * (-sign);
					m_solve_state[body_idx].angular_gradient += j_ang * f_ang;
					m_solve_state[body_idx].angular_hessian += outer_product(j_ang, j_ang) * constraint.ang_penalty[k];
				}

				std::optional<coupled_frame> previous;
				for (std::uint32_t k = 0; k < coupled->count; ++k) {
					if (const int slot = coupled_slot(*coupled, k); (constraint.limited_rows & (1u << slot)) != 0) {
						accumulate_angular_limit(
							m_solve_state[body_idx],
							coupled_limit_row(constraint, *coupled, k, alpha, m_config.coupled_limit_normalized != 0),
							constraint.ang_lambda[slot],
							constraint.ang_penalty[slot],
							sign
						);
					}
					if (constraint.drive_stiffness[k] <= angular_stiffness{}) {
						continue;
					}
					if (!previous) {
						previous = constraint.drive_damping > 0.f
							? coupled_frame_of(m_curve_words, constraint.curve_words, body_a.old_orientation, body_b.old_orientation)
							: *coupled;
					}
					accumulate_drive_row(
						m_solve_state[body_idx],
						constraint,
						coupled->coordinate_axes[k],
						coupled->coordinates[k],
						previous->coordinates[k],
						static_cast<int>(k),
						sign
					);
				}
			}
			else {
				const vec3f axis_a = rotate_vector(body_a.predicted_orientation, constraint.local_axis_a);
				const vec3f axis_b = rotate_vector(body_b.predicted_orientation, constraint.local_axis_b);
				const vec3f swing_error = cross(axis_a, axis_b);
				const auto [perp_u, perp_v] = body_perpendiculars(body_a.predicted_orientation, constraint.local_axis_a);

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

				if (constraint.limited_rows != 0) {
					const int row = limits.hinge_limit_row;
					accumulate_angular_limit(
						m_solve_state[body_idx],
						limit_row_of(constraint, body_a.predicted_orientation, theta, row, alpha),
						constraint.ang_lambda[row],
						constraint.ang_penalty[row],
						sign
					);
				}

				if (constraint.drive_stiffness[0] > angular_stiffness{}) {
					accumulate_joint_drive(
						constraint,
						body_idx,
						axis_a,
						dot(axis_a, theta),
						0,
						sign
					);
				}
			}
		}
	}
	else if (constraint.type == joint_type::slider) {
		const auto perps = body_perpendiculars(body_a.predicted_orientation, constraint.local_axis_a);
		for (int k = 0; k < 2; ++k) {
			const length C = dot(perps[k], d) - constraint.pos_c0[k] * alpha;
			const stiffness penalty = floored_penalty(constraint.pos_penalty[k], constraint.body_a, constraint.body_b);
			const force fi = penalty * C + constraint.pos_lambda[k];

			const vec3f j_lin = perps[k] * sign;
			const vec3<angular_jacobian> j_ang = cross(r, perps[k]) / rad;

			m_solve_state[body_idx].gradient += j_lin * fi;
			m_solve_state[body_idx].hessian += outer_product(j_lin, j_lin) * penalty;

			if (m_bodies[body_idx].update_orientation) {
				m_solve_state[body_idx].angular_gradient += j_ang * fi;
				m_solve_state[body_idx].angular_hessian += outer_product(j_ang, j_ang) * penalty;
				m_solve_state[body_idx].hessian_xtheta += outer_product(j_lin, j_ang) * penalty;
				accumulate_geometric_stiffness(m_solve_state[body_idx], r, perps[k], abs(fi));
			}
		}

		if (m_bodies[body_idx].update_orientation) {
			const quat q_error = body_b.predicted_orientation * conjugate(body_a.predicted_orientation) *
				conjugate(constraint.rest_orientation);
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

auto gse::vbd::solver::accumulate_joint_drive(const joint_constraint& constraint, const std::uint32_t body_idx, const vec3f& axis_world, const angle theta_axis, const int axis_index, const float sign) -> void {
	const auto& body_a = m_bodies[constraint.body_a];
	const auto& body_b = m_bodies[constraint.body_b];
	angle theta_prev = theta_axis;
	if (constraint.drive_damping > 0.f) {
		const quat q_error_prev = body_b.old_orientation * conjugate(body_a.old_orientation) *
			conjugate(constraint.rest_orientation);
		theta_prev = dot(axis_world, to_axis_angle(q_error_prev));
	}
	accumulate_drive_row(m_solve_state[body_idx], constraint, axis_world, theta_axis, theta_prev, axis_index, sign);
}

auto gse::vbd::solver::update_joint_dual(const time_squared h_squared, const time_step dt, const float alpha, const int iteration) -> step_delta {
	auto& joints = m_graph.joint_constraints();
	if (m_config.trace_body != 0xFFFFFFFFu) {
		for (std::size_t ji = 0; ji < joints.size(); ++ji) {
			const auto& j = joints[ji];
			if (j.body_a != m_config.trace_body && j.body_b != m_config.trace_body) {
				continue;
			}
			const auto& body_a = m_bodies[j.body_a];
			const auto& body_b = m_bodies[j.body_b];
			const vec3<predicted_position> p_a = body_a.predicted_position + rotate_vector(body_a.predicted_orientation, j.local_anchor_a);
			const vec3<predicted_position> p_b = body_b.predicted_position + rotate_vector(body_b.predicted_orientation, j.local_anchor_b);
			const vec3<displacement> d = p_a - p_b;
			log::println(
				log::category::physics,
				"cpu joint dual it {} ji {} type {}: d {:.7f} c0 {:.7f} lambda {:.4f} pen {:.1f}",
				iteration,
				ji,
				static_cast<int>(j.type),
				d,
				j.pos_c0,
				j.pos_lambda,
				j.pos_penalty
			);
		}
	}
	const auto worker_count = std::max<std::size_t>(1, task::thread_count());
	std::inplace_vector<step_delta, 64> per_worker_max;
	for (std::size_t i = 0; i < worker_count; ++i) {
		per_worker_max.push_back({});
	}

	task::coarse_parallel(
		joints.size(),
		256,
		[&, this](std::size_t ji) {
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

			const auto coupled = j.type == joint_type::coupled
				? std::optional(coupled_frame_of(m_curve_words, j.curve_words, body_a.predicted_orientation, body_b.predicted_orientation))
				: std::nullopt;
			const vec3<lever_arm> r_aw = coupled
				? vec3<lever_arm>(rotate_vector(body_a.predicted_orientation, j.local_anchor_a) + coupled->target_offset)
				: rotate_vector(body_a.predicted_orientation, j.local_anchor_a);
			const vec3<lever_arm> r_bw = rotate_vector(body_b.predicted_orientation, j.local_anchor_b);
			const vec3<predicted_position> p_a = body_a.predicted_position + r_aw;
			const vec3<predicted_position> p_b = body_b.predicted_position + r_bw;
			const vec3<displacement> d = p_a - p_b;

			if (j.type == joint_type::distance) {
				const length C = (magnitude(d) - j.target_distance) - j.pos_c0[0] * alpha;
				j.pos_lambda[0] = (j.compliance > inverse_mass{} ? j.pos_penalty[0] : floored_penalty(j.pos_penalty[0], j.body_a, j.body_b)) * C + j.pos_lambda[0];
				const stiffness penalty_cap = (j.compliance > inverse_mass{})
					? std::min<stiffness>(1.f / (j.compliance * h_squared), m_config.penalty_max)
					: m_config.penalty_max;
				j.pos_penalty[0] = std::min(j.pos_penalty[0] + m_config.beta * abs(C), penalty_cap);
				track_linear(C);
			}
			else if (j.type == joint_type::fixed || j.type == joint_type::hinge || j.type == joint_type::ball || j.type == joint_type::universal || j.type == joint_type::coupled) {
				const float carried = joint_carried_fraction(j.pos_c0, alpha, m_config.joint_correction_speed, dt);
				for (int k = 0; k < 3; ++k) {
					constexpr std::array dirs = { axis_x, axis_y, axis_z };
					const length c = dot(dirs[k], d) - j.pos_c0[k] * carried;
					j.pos_lambda[k] = floored_penalty(j.pos_penalty[k], j.body_a, j.body_b) * c + j.pos_lambda[k];
					j.pos_penalty[k] = std::min(j.pos_penalty[k] + m_config.beta * abs(c), m_config.penalty_max);
					track_linear(c);
				}

				if (j.type == joint_type::ball && j.limited_rows == 0) {
					return;
				}

				const quat q_error = body_b.predicted_orientation * conjugate(body_a.predicted_orientation) *
					conjugate(j.rest_orientation);
				const vec3<angle> theta = to_axis_angle(q_error);

				if (j.type == joint_type::fixed) {
					for (int k = 0; k < 3; ++k) {
						const angle c_ang = theta[k] - j.ang_c0[k] * alpha;
						j.ang_lambda[k] = j.ang_penalty[k] * c_ang + j.ang_lambda[k];
						j.ang_penalty[k] = std::min(j.ang_penalty[k] + m_config.ang_beta * abs(c_ang),
							m_config.ang_penalty_max);
						track_angular(c_ang);
					}
				}
				else if (j.type == joint_type::ball) {
					for (int k = 0; k < 3; ++k) {
						if ((j.limited_rows & (1u << k)) == 0) {
							continue;
						}
						if (const auto c_limit = update_angular_limit_dual(limit_row_of(j, body_a.predicted_orientation, theta, k, alpha), j.ang_lambda[k], j.ang_penalty[k], m_config)) {
							track_angular(*c_limit);
						}
					}
				}
				else if (j.type == joint_type::universal) {
					const vec3f axis_a = rotate_vector(body_a.predicted_orientation, j.local_axis_a);
					const angle c_twist = dot(axis_a, theta) - j.ang_c0[0] * alpha;
					j.ang_lambda[0] = j.ang_penalty[0] * c_twist + j.ang_lambda[0];
					j.ang_penalty[0] = std::min(j.ang_penalty[0] + m_config.ang_beta * abs(c_twist),
						m_config.ang_penalty_max);
					track_angular(c_twist);
				}
				else if (coupled) {
					const vec3<angle> phi = to_axis_angle(coupled->child * conjugate(coupled->target));
					const auto& perps = coupled->perpendiculars;
					for (std::uint32_t k = 0; k < 3 - coupled->count; ++k) {
						const angle c_ang = dot(perps[k], phi) - j.ang_c0[k] * alpha;
						j.ang_lambda[k] = j.ang_penalty[k] * c_ang + j.ang_lambda[k];
						j.ang_penalty[k] = std::min(j.ang_penalty[k] + m_config.ang_beta * abs(c_ang), m_config.ang_penalty_max);
						track_angular(c_ang);
					}

					for (std::uint32_t k = 0; k < coupled->count; ++k) {
						const int slot = coupled_slot(*coupled, k);
						if ((j.limited_rows & (1u << slot)) == 0) {
							continue;
						}
						if (const auto c_limit = update_angular_limit_dual(coupled_limit_row(j, *coupled, k, alpha, m_config.coupled_limit_normalized != 0), j.ang_lambda[slot], j.ang_penalty[slot], m_config)) {
							track_angular(*c_limit);
						}
					}
				}
				else {
					const vec3f axis_a = rotate_vector(body_a.predicted_orientation, j.local_axis_a);
					const vec3f axis_b = rotate_vector(body_b.predicted_orientation, j.local_axis_b);
					const vec3f swing_error = cross(axis_a, axis_b);
					const auto [perp_u, perp_v] = body_perpendiculars(body_a.predicted_orientation, j.local_axis_a);

					const angle c_u = radians(dot(perp_u, swing_error)) - j.ang_c0[0] * alpha;
					const angle c_v = radians(dot(perp_v, swing_error)) - j.ang_c0[1] * alpha;

					j.ang_lambda[0] = j.ang_penalty[0] * c_u + j.ang_lambda[0];
					j.ang_penalty[0] = std::min(j.ang_penalty[0] + m_config.ang_beta * abs(c_u), m_config.ang_penalty_max);
					track_angular(c_u);

					j.ang_lambda[1] = j.ang_penalty[1] * c_v + j.ang_lambda[1];
					j.ang_penalty[1] = std::min(j.ang_penalty[1] + m_config.ang_beta * abs(c_v), m_config.ang_penalty_max);
					track_angular(c_v);

					if (j.limited_rows != 0) {
						const int row = limits.hinge_limit_row;
						if (const auto c_limit = update_angular_limit_dual(limit_row_of(j, body_a.predicted_orientation, theta, row, alpha), j.ang_lambda[row], j.ang_penalty[row], m_config)) {
							track_angular(*c_limit);
						}
					}
				}
			}
			else if (j.type == joint_type::slider) {
				const auto perps = body_perpendiculars(body_a.predicted_orientation, j.local_axis_a);
				for (int k = 0; k < 2; ++k) {
					const length c = dot(perps[k], d) - j.pos_c0[k] * alpha;
					j.pos_lambda[k] = floored_penalty(j.pos_penalty[k], j.body_a, j.body_b) * c + j.pos_lambda[k];
					j.pos_penalty[k] = std::min(j.pos_penalty[k] + m_config.beta * abs(c), m_config.penalty_max);
					track_linear(c);
				}

				const quat q_error = body_b.predicted_orientation * conjugate(body_a.predicted_orientation) *
					conjugate(j.rest_orientation);
				const vec3<angle> theta = to_axis_angle(q_error);

				for (int k = 0; k < 3; ++k) {
					const angle c_ang = theta[k] - j.ang_c0[k] * alpha;
					j.ang_lambda[k] = j.ang_penalty[k] * c_ang + j.ang_lambda[k];
					j.ang_penalty[k] = std::min(j.ang_penalty[k] + m_config.ang_beta * abs(c_ang),
						m_config.ang_penalty_max);
					track_angular(c_ang);
				}
			}
		},
		trace_id<"vbd::joint_dual_update">()
	);

	step_delta result;
	for (const auto& m : per_worker_max) {
		result.linear = std::max(result.linear, m.linear);
		result.angular = std::max(result.angular, m.angular);
	}
	return result;
}

auto gse::vbd::contact_jacobian_of(const contact_constraint& c, const body_state& body_a, const body_state& body_b) -> frozen_jacobian {
	const vec3<lever_arm> r_aw = rotate_vector(body_a.predicted_orientation, c.local_anchor_a);
	const vec3<lever_arm> r_bw = rotate_vector(body_b.predicted_orientation, c.local_anchor_b);
	const std::array dirs = { c.normal, c.tangent_u, c.tangent_v };
	return {
		.world_r_a = r_aw,
		.world_r_b = r_bw,
		.j_ang_a = { cross(r_aw, dirs[0]) / rad,
			cross(r_aw, dirs[1]) / rad,
			cross(r_aw, dirs[2]) / rad },
		.j_ang_b = { cross(r_bw, dirs[0]) / rad,
			cross(r_bw, dirs[1]) / rad,
			cross(r_bw, dirs[2]) / rad },
	};
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
		state.angular_hessian[j][j] += col_norm * abs_force / (rad * rad);
	}
}

auto gse::vbd::limit_axis_of(const joint_constraint& j, const quat& orientation_a, const int row) -> vec3f {
	constexpr std::array dirs = { axis_x, axis_y, axis_z };
	return rotate_vector(orientation_a, j.type == joint_type::ball ? dirs[row] : j.local_axis_a);
}

auto gse::vbd::limit_row_of(const joint_constraint& j, const quat& orientation_a, const vec3<angle>& theta, const int row, const float c0_scale) -> angular_limit_row {
	const vec3f axis = limit_axis_of(j, orientation_a, row);
	return {
		.axis = axis,
		.coordinate = dot(axis, theta),
		.lower = j.limit_lower[row],
		.upper = j.limit_upper[row],
		.c0 = j.ang_c0[row] * c0_scale,
	};
}

auto gse::vbd::limit_force_of(const angular_limit_row& row, const torque lambda, const angular_stiffness penalty) -> angular_limit_force {
	const bool upper_side = row.coordinate >= (row.lower + row.upper) * 0.5f;
	const angle error = ((row.coordinate - (upper_side ? row.upper : row.lower)) - row.c0) * row.scale;
	const torque unclamped = penalty * error + lambda;
	return {
		.error = error,
		.force = upper_side ? std::max(unclamped, torque{}) : std::min(unclamped, torque{}),
	};
}

auto gse::vbd::accumulate_angular_limit(body_solve_state& state, const angular_limit_row& row, const torque lambda, const angular_stiffness penalty, const float sign) -> void {
	const torque force = limit_force_of(row, lambda, penalty).force;
	if (force == torque{}) {
		return;
	}

	const vec3f j_limit = row.axis * (row.scale * -sign);
	state.angular_gradient += j_limit * force;
	state.angular_hessian += outer_product(j_limit, j_limit) * penalty;
}

auto gse::vbd::update_angular_limit_dual(const angular_limit_row& row, torque& lambda, angular_stiffness& penalty, const solver_config& cfg) -> std::optional<angle> {
	const auto [error, force] = limit_force_of(row, lambda, penalty);
	lambda = force;
	if (force == torque{}) {
		return std::nullopt;
	}
	penalty = std::min(penalty + cfg.ang_beta * abs(error), cfg.ang_penalty_max);
	return error;
}

auto gse::vbd::angular_limit_c0(const angular_limit_row& row) -> angle {
	if (row.coordinate < row.lower) {
		return row.coordinate - row.lower;
	}
	if (row.coordinate > row.upper) {
		return row.coordinate - row.upper;
	}
	return {};
}

auto gse::vbd::angular_row_penalty(const body_state& body_a, const body_state& body_b, const vec3f& axis, const angular_stiffness previous, const time_squared h_squared, const solver_config& cfg) -> angular_stiffness {
	std::optional<inverse_inertia> heaviest;
	if (!body_a.locked) {
		heaviest = dot(axis, body_a.inv_inertia * axis);
	}
	if (!body_b.locked) {
		const inverse_inertia w_b = dot(axis, body_b.inv_inertia * axis);
		heaviest = heaviest ? std::min(*heaviest, w_b) : w_b;
	}

	const angular_stiffness effective =
		heaviest && *heaviest > per_kilogram_meter_squared(1e-10f) ? 1.f / *heaviest / h_squared : cfg.ang_penalty_min;

	return std::clamp(std::max(previous, effective), cfg.ang_penalty_min, cfg.ang_penalty_max);
}

auto gse::vbd::curve_float(const std::span<const std::uint32_t> curves, const std::uint32_t index) -> float {
	return std::bit_cast<float>(curves[index]);
}

auto gse::vbd::curve_vec3(const std::span<const std::uint32_t> curves, const std::uint32_t index) -> vec3f {
	return { curve_float(curves, index), curve_float(curves, index + 1), curve_float(curves, index + 2) };
}

auto gse::vbd::curve_quat(const std::span<const std::uint32_t> curves, const std::uint32_t index) -> quat {
	return quat(curve_float(curves, index), curve_float(curves, index + 1), curve_float(curves, index + 2), curve_float(curves, index + 3));
}

auto gse::vbd::sample_curve(const std::span<const std::uint32_t> curves, const std::uint32_t base, const std::uint32_t header, const angle theta) -> curve_sample {
	const std::uint32_t count = curves[header + 5];
	if (count == 0) {
		return {};
	}
	const std::uint32_t first = base + curves[header + 4];
	std::uint32_t at = first;
	for (std::uint32_t i = 1; i < count; ++i) {
		const std::uint32_t candidate = first + i * limits.curve_segment_uints;
		if (theta < radians(curve_float(curves, candidate))) {
			break;
		}
		at = candidate;
	}
	const float u = (theta - radians(curve_float(curves, at))) / rad;
	const float c0 = curve_float(curves, at + 1);
	const float c1 = curve_float(curves, at + 2);
	const float c2 = curve_float(curves, at + 3);
	const float c3 = curve_float(curves, at + 4);
	return {
		.value = c0 + u * (c1 + u * (c2 + u * c3)),
		.slope = c1 + u * (2.f * c2 + u * 3.f * c3),
	};
}

auto gse::vbd::twist_about(const quat& q, const vec3f& axis) -> angle {
	const float sign = q.s() >= 0.f ? 1.f : -1.f;
	return 2.f * atan2(sign * dot(q.imaginary_part(), axis), sign * q.s());
}

auto gse::vbd::sample_spatial(const std::span<const std::uint32_t> curves, const std::uint32_t base, const std::uint32_t spatial, const vec3<angle>& coordinates) -> spatial_sample {
	spatial_sample result;
	for (std::uint32_t k = 0; k < 3; ++k) {
		const std::uint32_t header = spatial + k * limits.curve_axis_uints;
		const vec3f axis = curve_vec3(curves, header);
		const std::uint32_t coordinate = curves[header + 3];
		const auto [value, slope] = sample_curve(curves, base, header, coordinates[coordinate]);
		result.rotation_rates[coordinate] += rotate_vector(result.rotation, axis) * slope;
		result.rotation = result.rotation * from_axis_angle(axis, radians(value));
	}
	for (std::uint32_t k = 3; k < 6; ++k) {
		const std::uint32_t header = spatial + k * limits.curve_axis_uints;
		const vec3f axis = curve_vec3(curves, header);
		const std::uint32_t coordinate = curves[header + 3];
		const auto [value, slope] = sample_curve(curves, base, header, coordinates[coordinate]);
		result.translation += vec3<displacement>(axis * meters(value));
		result.translation_rates[coordinate] += vec3<angular_jacobian>(axis * meters_per_radian(slope));
	}
	return result;
}

auto gse::vbd::coupled_initial_coordinates(const std::span<const std::uint32_t> curves, const std::uint32_t spatial, const quat& relative, const std::uint32_t count) -> vec3<angle> {
	const vec3f a0 = curve_vec3(curves, spatial);
	if (count == 1) {
		return { twist_about(relative, a0), angle{}, angle{} };
	}
	const vec3f a1 = curve_vec3(curves, spatial + limits.curve_axis_uints);
	const vec3f a2 = curve_vec3(curves, spatial + 2 * limits.curve_axis_uints);
	const float handed = dot(a0, cross(a1, a2));
	const vec3f r0 = rotate_vector(relative, a0);
	const vec3f r1 = rotate_vector(relative, a1);
	const vec3f r2 = rotate_vector(relative, a2);
	return {
		atan2(-handed * dot(a1, r2), dot(a2, r2)),
		asin(std::clamp(handed * dot(a0, r2), -1.f, 1.f)),
		atan2(-handed * dot(a0, r1), dot(a0, r0)),
	};
}

auto gse::vbd::coupled_pseudo_inverse(const std::array<vec3f, 3>& rates) -> std::array<vec3f, 3> {
	mat3f gram;
	for (std::size_t c = 0; c < 3; ++c) {
		for (std::size_t r = 0; r < 3; ++r) {
			gram[c][r] = dot(rates[r], rates[c]) + (r == c ? limits.coupled_damping : 0.f);
		}
	}
	const mat3f gram_inverse = gram.inverse();
	std::array<vec3f, 3> rows;
	for (std::size_t k = 0; k < 3; ++k) {
		rows[k] = rates[0] * gram_inverse[k][0] + rates[1] * gram_inverse[k][1] + rates[2] * gram_inverse[k][2];
	}
	return rows;
}

auto gse::vbd::coupled_coordinates_of(const std::span<const std::uint32_t> curves, const std::uint32_t joint_words, const quat& orientation_a, const quat& orientation_b) -> vec3<angle> {
	const quat frame = orientation_a * curve_quat(curves, joint_words);
	const quat child = orientation_b * curve_quat(curves, joint_words + 4);
	const quat relative = conjugate(frame) * child;
	const std::uint32_t spatial = joint_words + curves[joint_words + limits.coupled_spatial_index];
	const std::uint32_t count = curves[joint_words + limits.coupled_coordinate_count_index];

	vec3<angle> coordinates = coupled_initial_coordinates(curves, spatial, relative, count);
	for (std::uint32_t i = 0; i < limits.coupled_coordinate_iterations; ++i) {
		const auto sample = sample_spatial(curves, joint_words, spatial, coordinates);
		const auto rows = coupled_pseudo_inverse(sample.rotation_rates);
		const vec3<angle> phi = to_axis_angle(relative * conjugate(sample.rotation));
		coordinates += vec3<angle>(dot(rows[0], phi), dot(rows[1], phi), dot(rows[2], phi));
	}
	return coordinates;
}

auto gse::vbd::coupled_frame_of(const std::span<const std::uint32_t> curves, const std::uint32_t joint_words, const quat& orientation_a, const quat& orientation_b) -> coupled_frame {
	return coupled_frame_at(curves, joint_words, orientation_a, orientation_b, coupled_coordinates_of(curves, joint_words, orientation_a, orientation_b));
}

auto gse::vbd::shared_coupled_frame(std::optional<coupled_newton>& newton, const std::span<const std::uint32_t> curves, const std::span<const body_state> bodies, const std::uint32_t joint_words, const std::uint32_t body_a, const std::uint32_t body_b) -> coupled_frame {
	const quat& orientation_a = bodies[body_a].predicted_orientation;
	const quat& orientation_b = bodies[body_b].predicted_orientation;
	if (!newton || newton->joint_words != joint_words || newton->body_a != body_a || newton->body_b != body_b) {
		newton = coupled_newton{
			.joint_words = joint_words,
			.body_a = body_a,
			.body_b = body_b,
			.coordinates = coupled_coordinates_of(curves, joint_words, orientation_a, orientation_b),
		};
	}
	return coupled_frame_at(curves, joint_words, orientation_a, orientation_b, newton->coordinates);
}

auto gse::vbd::coupled_frame_at(const std::span<const std::uint32_t> curves, const std::uint32_t joint_words, const quat& orientation_a, const quat& orientation_b, const vec3<angle>& coordinates) -> coupled_frame {
	const quat frame = orientation_a * curve_quat(curves, joint_words);
	const quat child = orientation_b * curve_quat(curves, joint_words + 4);
	const std::uint32_t spatial = joint_words + curves[joint_words + limits.coupled_spatial_index];
	const std::uint32_t count = curves[joint_words + limits.coupled_coordinate_count_index];

	const auto sample = sample_spatial(curves, joint_words, spatial, coordinates);
	const auto rows = coupled_pseudo_inverse(sample.rotation_rates);
	coupled_frame result{
		.child = child,
		.target = frame * sample.rotation,
		.count = count,
		.coordinates = coordinates,
		.perpendiculars = body_perpendiculars(frame, normalize(sample.rotation_rates[0])),
		.target_offset = vec3<lever_arm>(rotate_vector(frame, sample.translation)),
	};
	for (std::size_t k = 0; k < 3; ++k) {
		result.coordinate_axes[k] = rotate_vector(frame, rows[k]);
		result.translation_rates[k] = rotate_vector(frame, sample.translation_rates[k]);
	}
	return result;
}

auto gse::vbd::coupled_slot(const coupled_frame& frame, const std::uint32_t coordinate) -> int {
	return static_cast<int>(3 - frame.count + coordinate);
}

auto gse::vbd::coupled_translation_lever(const coupled_frame& frame, const vec3f& direction) -> vec3<angular_jacobian> {
	vec3<angular_jacobian> lever;
	for (std::size_t k = 0; k < 3; ++k) {
		lever += frame.coordinate_axes[k] * dot(direction, frame.translation_rates[k]);
	}
	return lever;
}

auto gse::vbd::moving_frame_joint_words(const std::span<const std::uint32_t> curves, const std::uint32_t frame_words) -> std::uint32_t {
	return frame_words - curves[frame_words];
}

auto gse::vbd::moving_frame_of(const std::span<const std::uint32_t> curves, const std::uint32_t frame_words, const quat& orientation_a, const quat& orientation_b) -> moving_frame {
	return moving_frame_in(curves, frame_words, orientation_a, coupled_frame_of(curves, moving_frame_joint_words(curves, frame_words), orientation_a, orientation_b));
}

auto gse::vbd::moving_frame_in(const std::span<const std::uint32_t> curves, const std::uint32_t frame_words, const quat& orientation_a, const coupled_frame& joint) -> moving_frame {
	const std::uint32_t joint_words = moving_frame_joint_words(curves, frame_words);
	const vec3<lever_arm> anchor = curve_vec3(curves, frame_words + 1) * meters(1.f);
	return {
		.anchor = rotate_vector(orientation_a, anchor),
		.placed = orientation_a * curve_quat(curves, frame_words + 4),
		.sample = sample_spatial(curves, joint_words, frame_words + limits.curve_frame_header_uints, joint.coordinates),
		.coordinate_axes = joint.coordinate_axes,
	};
}

auto gse::vbd::moving_point_in(const moving_frame& frame, const vec3<lever_arm>& local_point) -> moving_point {
	const vec3<lever_arm> rotated = rotate_vector(frame.sample.rotation, local_point);
	moving_point result{
		.offset = frame.anchor + rotate_vector(frame.placed, rotated + vec3<lever_arm>(frame.sample.translation)),
		.coordinate_axes = frame.coordinate_axes,
	};
	for (std::size_t k = 0; k < 3; ++k) {
		result.rates[k] = rotate_vector(frame.placed, cross(frame.sample.rotation_rates[k], rotated) / rad + frame.sample.translation_rates[k]);
	}
	return result;
}

auto gse::vbd::moving_point_of(const std::span<const std::uint32_t> curves, const std::uint32_t frame_words, const quat& orientation_a, const quat& orientation_b, const vec3<lever_arm>& local_point) -> moving_point {
	return moving_point_in(moving_frame_of(curves, frame_words, orientation_a, orientation_b), local_point);
}

auto gse::vbd::moving_point_lever(const moving_point& point, const vec3f& direction) -> vec3<angular_jacobian> {
	vec3<angular_jacobian> lever;
	for (std::size_t k = 0; k < 3; ++k) {
		lever += point.coordinate_axes[k] * dot(direction, point.rates[k]);
	}
	return lever;
}

auto gse::vbd::coupled_limit_row(const joint_constraint& j, const coupled_frame& frame, const std::uint32_t coordinate, const float c0_scale, const bool normalized) -> angular_limit_row {
	const int slot = coupled_slot(frame, coordinate);
	return {
		.axis = frame.coordinate_axes[coordinate],
		.coordinate = frame.coordinates[coordinate],
		.lower = j.limit_lower[slot],
		.upper = j.limit_upper[slot],
		.c0 = j.ang_c0[slot] * c0_scale,
		.scale = coupled_limit_scale(frame.coordinate_axes[coordinate], normalized),
	};
}

auto gse::vbd::coupled_limit_scale(const vec3f& axis, const bool normalized) -> float {
	return normalized ? 1.f / magnitude(axis) : 1.f;
}

auto gse::vbd::joint_carried_fraction(const vec3<gap>& c0, const float alpha, const velocity correction_speed, const time_step dt) -> float {
	if (correction_speed <= velocity{} || alpha >= 1.f) {
		return alpha;
	}
	const gap carried = magnitude(c0);
	if (carried <= gap{}) {
		return alpha;
	}
	const float closed = correction_speed * dt / carried;
	return std::min(alpha, std::max(0.f, 1.f - closed));
}

auto gse::vbd::accumulate_drive_row(body_solve_state& state, const joint_constraint& j, const vec3f& axis, const angle coordinate, const angle previous, const int index, const float sign) -> void {
	const auto stiffness_k = j.drive_stiffness[index];
	torque f_drive = stiffness_k * (coordinate - j.drive_target[index]);
	auto hessian_stiffness = stiffness_k;
	if (j.drive_damping > 0.f) {
		f_drive += stiffness_k * j.drive_damping * (coordinate - previous);
		hessian_stiffness = stiffness_k * (1.f + j.drive_damping);
	}
	if (j.drive_max_torque > torque{} && abs(f_drive) > j.drive_max_torque) {
		f_drive = std::clamp(f_drive, -j.drive_max_torque, j.drive_max_torque);
		hessian_stiffness = {};
	}

	const vec3f j_drive = axis * (-sign);
	state.angular_gradient += j_drive * f_drive;
	state.angular_hessian += outer_product(j_drive, j_drive) * hessian_stiffness;
}

auto gse::vbd::coupled_perpendiculars(const vec3f& axis) -> std::array<vec3f, 2> {
	vec3f perp_u = cross(axis, axis_y);
	if (magnitude(perp_u) < 1e-6f) {
		perp_u = cross(axis, axis_x);
	}
	perp_u = normalize(perp_u);
	return { perp_u, normalize(cross(axis, perp_u)) };
}

auto gse::vbd::body_perpendiculars(const quat& orientation, const vec3f& local_axis) -> std::array<vec3f, 2> {
	const auto local = coupled_perpendiculars(local_axis);
	return { rotate_vector(orientation, local[0]), rotate_vector(orientation, local[1]) };
}

auto gse::vbd::gravity_magnitude() -> acceleration {
	return meters_per_second_squared(9.8f);
}

auto gse::vbd::contact_effective_mass(const body_state& body_a, const body_state& body_b, const vec3<lever_arm>& r_aw, const vec3<lever_arm>& r_bw, const vec3f& dir) -> mass {
	inverse_mass inv_mass_sum = body_a.inverse_mass() + body_b.inverse_mass();

	if (body_a.update_orientation && !body_a.locked) {
		const auto ang_j_a = cross(r_aw, dir) / rad;
		inv_mass_sum += dot(cross(body_a.inv_inertia * ang_j_a, r_aw), dir) / rad;
	}

	if (body_b.update_orientation && !body_b.locked) {
		const auto ang_j_b = cross(r_bw, dir) / rad;
		inv_mass_sum += dot(cross(body_b.inv_inertia * ang_j_b, r_bw), dir) / rad;
	}

	if (!isfinite(inv_mass_sum) || inv_mass_sum <= per_kilograms(1e-10f)) {
		return {};
	}

	return 1.f / inv_mass_sum;
}

auto gse::vbd::joint_effective_mass(const body_state& body_a, const body_state& body_b, const vec3<lever_arm>& r_aw, const vec3<lever_arm>& r_bw, const vec3f& dir) -> mass {
	const auto side_inverse_mass = [&](const body_state& body, const vec3<lever_arm>& r) {
		inverse_mass w = body.inverse_mass();
		if (body.update_orientation) {
			w += dot(cross(body.inv_inertia * (cross(r, dir) / rad), r), dir) / rad;
		}
		return w;
	};

	std::optional<inverse_mass> heaviest;
	if (!body_a.locked) {
		heaviest = side_inverse_mass(body_a, r_aw);
	}
	if (!body_b.locked) {
		const inverse_mass w_b = side_inverse_mass(body_b, r_bw);
		heaviest = heaviest ? std::min(*heaviest, w_b) : w_b;
	}

	if (!heaviest || !isfinite(*heaviest) || *heaviest <= per_kilograms(1e-10f)) {
		return {};
	}

	return 1.f / *heaviest;
}

auto gse::vbd::warm_start_joint(joint_constraint& j, const body_state& ba, const body_state& bb, const time_squared h_squared, const solver_config& cfg, const std::span<const std::uint32_t> curves) -> void {
	const auto coupled = j.type == joint_type::coupled
		? std::optional(coupled_frame_of(curves, j.curve_words, ba.orientation, bb.orientation))
		: std::nullopt;
	const vec3<lever_arm> r_aw = coupled
		? vec3<lever_arm>(rotate_vector(ba.orientation, j.local_anchor_a) + coupled->target_offset)
		: rotate_vector(ba.orientation, j.local_anchor_a);
	const vec3<lever_arm> r_bw = rotate_vector(bb.orientation, j.local_anchor_b);

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
			dir = body_perpendiculars(ba.orientation, j.local_axis_a)[k];
		}
		else {
			dir = dirs[k];
		}
		const stiffness eff = joint_effective_mass(ba, bb, r_aw, r_bw, dir) / h_squared;
		const stiffness pos_penalty_cap = (j.compliance > inverse_mass{})
			? std::min<stiffness>(1.f / (j.compliance * h_squared), cfg.penalty_max)
			: cfg.penalty_max;
		j.pos_penalty[k] = std::clamp(std::max(j.pos_penalty[k], eff), cfg.penalty_min, pos_penalty_cap);
	}

	int num_ang_rows = 3;
	if (j.type == joint_type::distance || j.type == joint_type::ball) {
		num_ang_rows = 0;
	}
	else if (j.type == joint_type::hinge) {
		num_ang_rows = 2;
	}
	else if (coupled) {
		num_ang_rows = static_cast<int>(3 - coupled->count);
	}
	else if (j.type == joint_type::universal) {
		num_ang_rows = 1;
	}

	for (int k = 0; k < num_ang_rows; ++k) {
		vec3f ang_dir;
		if (coupled) {
			ang_dir = coupled->perpendiculars[k];
		}
		else if (j.type == joint_type::hinge) {
			ang_dir = body_perpendiculars(ba.orientation, j.local_axis_a)[k];
		}
		else if (j.type == joint_type::universal) {
			ang_dir = rotate_vector(ba.orientation, j.local_axis_a);
		}
		else {
			ang_dir = dirs[k];
		}

		j.ang_penalty[k] = angular_row_penalty(ba, bb, ang_dir, j.ang_penalty[k], h_squared, cfg);
	}

	for (int k = 0; k < 3; ++k) {
		if ((j.limited_rows & (1u << k)) == 0) {
			continue;
		}
		vec3f limit_axis = limit_axis_of(j, ba.orientation, k);
		if (coupled) {
			const vec3f& coordinate_axis = coupled->coordinate_axes[static_cast<std::uint32_t>(k) + coupled->count - 3];
			limit_axis = coordinate_axis * coupled_limit_scale(coordinate_axis, cfg.coupled_limit_normalized != 0);
		}
		j.ang_penalty[k] = angular_row_penalty(ba, bb, limit_axis, j.ang_penalty[k] * cfg.gamma, h_squared, cfg);
	}
}

auto gse::vbd::compute_joint_c0(joint_constraint& j, const body_state& ba, const body_state& bb, const std::span<const std::uint32_t> curves) -> void {
	const vec3<lever_arm> r_aw = rotate_vector(ba.orientation, j.local_anchor_a);
	const vec3<lever_arm> r_bw = rotate_vector(bb.orientation, j.local_anchor_b);
	const vec3<displacement> d = (ba.position + r_aw) - (bb.position + r_bw);

	if (j.type == joint_type::distance) {
		j.pos_c0[0] = magnitude(d) - j.target_distance;
		return;
	}

	if (j.type == joint_type::coupled) {
		const auto frame = coupled_frame_of(curves, j.curve_words, ba.orientation, bb.orientation);
		const vec3<displacement> d_target = (ba.position + r_aw + frame.target_offset) - (bb.position + r_bw);
		constexpr std::array dirs = { axis_x, axis_y, axis_z };
		for (int k = 0; k < 3; ++k) {
			j.pos_c0[k] = dot(dirs[k], d_target);
		}
		const vec3<angle> phi = to_axis_angle(frame.child * conjugate(frame.target));
		const auto& perps = frame.perpendiculars;
		for (std::uint32_t k = 0; k < 3 - frame.count; ++k) {
			j.ang_c0[k] = dot(perps[k], phi);
		}
		for (std::uint32_t k = 0; k < frame.count; ++k) {
			if (const int slot = coupled_slot(frame, k); (j.limited_rows & (1u << slot)) != 0) {
				j.ang_c0[slot] = angular_limit_c0(coupled_limit_row(j, frame, k, 0.f, false));
			}
		}
		return;
	}

	if (j.type == joint_type::fixed || j.type == joint_type::hinge || j.type == joint_type::ball || j.type == joint_type::universal) {
		constexpr std::array dirs = { axis_x, axis_y, axis_z };
		for (int k = 0; k < 3; ++k) {
			j.pos_c0[k] = dot(dirs[k], d);
		}

		if (j.type == joint_type::ball && j.limited_rows == 0) {
			return;
		}

		const quat q_error = bb.orientation * conjugate(ba.orientation) * conjugate(j.rest_orientation);
		const vec3<angle> theta = to_axis_angle(q_error);

		for (int k = 0; k < 3; ++k) {
			if ((j.limited_rows & (1u << k)) != 0) {
				j.ang_c0[k] = angular_limit_c0(limit_row_of(j, ba.orientation, theta, k, 0.f));
			}
		}

		if (j.type == joint_type::ball) {
			return;
		}

		if (j.type == joint_type::fixed) {
			for (int k = 0; k < 3; ++k) {
				j.ang_c0[k] = theta[k];
			}
			return;
		}

		if (j.type == joint_type::universal) {
			const vec3f axis_a = rotate_vector(ba.orientation, j.local_axis_a);
			j.ang_c0[0] = dot(axis_a, theta);
			return;
		}

		const vec3f axis_a = rotate_vector(ba.orientation, j.local_axis_a);
		const vec3f axis_b = rotate_vector(bb.orientation, j.local_axis_b);
		const vec3f swing_error = cross(axis_a, axis_b);
		const auto [perp_u, perp_v] = body_perpendiculars(ba.orientation, j.local_axis_a);
		j.ang_c0[0] = radians(dot(perp_u, swing_error));
		j.ang_c0[1] = radians(dot(perp_v, swing_error));
		return;
	}

	if (j.type == joint_type::slider) {
		const auto [perp0, perp1] = body_perpendiculars(ba.orientation, j.local_axis_a);
		j.pos_c0[0] = dot(perp0, d);
		j.pos_c0[1] = dot(perp1, d);

		const auto slider_theta = to_axis_angle(bb.orientation * conjugate(ba.orientation) * conjugate(j.rest_orientation));
		for (int k = 0; k < 3; ++k) {
			j.ang_c0[k] = slider_theta[k];
		}
	}
}

auto gse::vbd::muscle_fiber_of(const muscle_constraint& m, const length path, const length previous_path, const time_step dt) -> std::optional<muscle_fiber> {
	const length tendon_free = path - m.tendon_slack_length;
	if (tendon_free <= length{} || m.optimal_fiber_length <= length{} || m.max_force <= force{}) {
		return std::nullopt;
	}

	muscle_fiber f;
	const length thickness = m.optimal_fiber_length * sin(m.pennation_at_optimal);
	f.fiber = hypot(tendon_free, thickness);
	f.cos_pennation = tendon_free / f.fiber;
	f.sin_pennation = thickness / f.fiber;
	f.fiber_ratio = f.fiber / m.optimal_fiber_length;
	f.fiber_velocity = (path - previous_path) / dt * f.cos_pennation;
	const float velocity_ratio = f.fiber_velocity / m.max_contraction_velocity;

	for (std::size_t i = 0; i < 3; ++i) {
		const float width = muscle.active_width[i] + muscle.active_width_slope[i] * f.fiber_ratio;
		const float offset = (f.fiber_ratio - muscle.active_center[i]) / width;
		const float term = muscle.active_height[i] * libm::exp(-0.5f * offset * offset);
		f.active += term;
		f.active_slope -= term * offset * (1.f - offset * muscle.active_width_slope[i]) / width;
	}

	if (f.fiber_ratio > muscle.passive_zero_ratio) {
		const float rate = muscle.passive_shape / muscle.passive_strain;
		const float scale = 1.f / (libm::exp(muscle.passive_shape) - 1.f);
		const float growth = libm::exp(rate * (f.fiber_ratio - 1.f));
		f.passive = (growth - libm::exp(rate * (muscle.passive_zero_ratio - 1.f))) * scale;
		f.passive_slope = rate * growth * scale;
	}

	const float velocity_arg = muscle.velocity_shape[1] * velocity_ratio + muscle.velocity_shape[2];
	f.force_velocity = muscle.velocity_shape[0] * std::asinh(velocity_arg) + muscle.velocity_shape[3];
	f.force_velocity_slope = muscle.velocity_shape[0] * muscle.velocity_shape[1] / std::sqrt(velocity_arg * velocity_arg + 1.f);
	if (f.force_velocity < 0.f) {
		f.force_velocity = 0.f;
		f.force_velocity_slope = 0.f;
	}
	return f;
}

auto gse::vbd::muscle_tension(const muscle_constraint& m, const length path, const length previous_path, const time_step dt) -> muscle_response {
	const auto f = muscle_fiber_of(m, path, previous_path, dt);
	if (!f) {
		return {};
	}

	const float fiber_force = m.activation * f->active * f->force_velocity + f->passive;
	const float length_slope = m.activation * f->active_slope * f->force_velocity + f->passive_slope;
	const float velocity_slope = m.activation * f->active * f->force_velocity_slope;

	const stiffness path_stiffness = m.max_force * (
		f->cos_pennation * f->cos_pennation * (length_slope / m.optimal_fiber_length + velocity_slope / (m.max_contraction_velocity * dt)) +
		fiber_force * f->sin_pennation * f->sin_pennation / f->fiber
	);

	return {
		.tension = m.max_force * fiber_force * f->cos_pennation,
		.path_stiffness = std::max(path_stiffness, stiffness{}),
	};
}

auto gse::vbd::advance_muscle_activation(muscle_constraint& m, const time_step dt) -> void {
	const float target = std::clamp(m.excitation, 0.f, 1.f);
	const float level = m.activation;
	const float recruitment = 0.5f + 1.5f * level;
	const time_step lag = target > level ? muscle.activation_time * recruitment : muscle.deactivation_time / recruitment;
	m.activation = target + (level - target) * libm::exp(-(dt / lag));
}

auto gse::vbd::first_moving_point(const std::span<const muscle_path_point> path) -> const muscle_path_point* {
	const auto moving = std::ranges::find_if(path, [](const muscle_path_point& point) {
		return point.frame_words != limits.no_curve_words;
	});
	return moving == path.end() ? nullptr : &*moving;
}

auto gse::vbd::muscle_frame_of(const std::span<const muscle_path_point> path, const std::span<const body_state> bodies, const std::span<const std::uint32_t> curves) -> std::optional<moving_frame> {
	const auto* moving = first_moving_point(path);
	if (!moving) {
		return std::nullopt;
	}
	return moving_frame_of(curves, moving->frame_words, bodies[moving->body].predicted_orientation, bodies[moving->frame_body].predicted_orientation);
}

auto gse::vbd::muscle_frame_of(const std::span<const muscle_path_point> path, const std::span<const body_state> bodies, const std::span<const std::uint32_t> curves, std::optional<coupled_newton>& newton) -> std::optional<moving_frame> {
	const auto* moving = first_moving_point(path);
	if (!moving) {
		return std::nullopt;
	}
	const auto joint = shared_coupled_frame(newton, curves, bodies, moving_frame_joint_words(curves, moving->frame_words), moving->body, moving->frame_body);
	return moving_frame_in(curves, moving->frame_words, bodies[moving->body].predicted_orientation, joint);
}

auto gse::vbd::muscle_point_of(const muscle_path_point& point, const std::span<const body_state> bodies, const std::optional<moving_frame>& frame) -> vec3<predicted_position> {
	const auto& body = bodies[point.body];
	if (point.frame_words == limits.no_curve_words) {
		return body.predicted_position + rotate_vector(body.predicted_orientation, point.local_point);
	}
	return body.predicted_position + moving_point_in(*frame, point.local_point).offset;
}

auto gse::vbd::muscle_path_length(const std::span<const muscle_path_point> path, const std::span<const body_state> bodies, const std::optional<moving_frame>& frame) -> length {
	length total{};
	for (std::size_t k = 0; k + 1 < path.size(); ++k) {
		total += magnitude(muscle_point_of(path[k + 1], bodies, frame) - muscle_point_of(path[k], bodies, frame));
	}
	return total;
}

auto gse::vbd::muscle_path_length(const std::span<const muscle_path_point> path, const std::span<const body_state> bodies, const std::span<const std::uint32_t> curves) -> length {
	return muscle_path_length(path, bodies, muscle_frame_of(path, bodies, curves));
}

auto gse::vbd::muscle_start_point_of(const muscle_path_point& point, const std::span<const body_state> bodies, const std::span<const std::uint32_t> curves) -> vec3<position> {
	const auto& body = bodies[point.body];
	if (point.frame_words == limits.no_curve_words) {
		return body.position + rotate_vector(body.orientation, point.local_point);
	}
	return body.position + moving_point_of(curves, point.frame_words, body.orientation, bodies[point.frame_body].orientation, point.local_point).offset;
}

auto gse::vbd::muscle_start_path_length(const std::span<const muscle_path_point> path, const std::span<const body_state> bodies, const std::span<const std::uint32_t> curves) -> length {
	length total{};
	for (std::size_t k = 0; k + 1 < path.size(); ++k) {
		total += magnitude(muscle_start_point_of(path[k + 1], bodies, curves) - muscle_start_point_of(path[k], bodies, curves));
	}
	return total;
}

auto gse::vbd::muscle_start_linear_gradient(const std::span<const muscle_path_point> path, const std::span<const body_state> bodies, const std::span<const std::uint32_t> curves, const std::uint32_t body_idx) -> vec3f {
	const auto segment_direction = [&](const std::size_t from, const std::size_t to) {
		const vec3<displacement> d = muscle_start_point_of(path[to], bodies, curves) - muscle_start_point_of(path[from], bodies, curves);
		return magnitude(d) > meters(1e-7f) ? normalize(d) : vec3f{};
	};

	vec3f g{};
	for (std::size_t k = 0; k < path.size(); ++k) {
		if (path[k].body != body_idx) {
			continue;
		}
		if (k > 0) {
			g += segment_direction(k - 1, k);
		}
		if (k + 1 < path.size()) {
			g -= segment_direction(k, k + 1);
		}
	}
	return g;
}

auto gse::vbd::solver::muscle_floor_of(const std::uint32_t body_idx, const time_step dt) const -> stiffness {
	stiffness floor{};
	for (const auto mi : m_graph.body_muscle_indices(body_idx)) {
		const auto& m = m_graph.muscle_constraints()[mi];
		const stiffness isometric = muscle_tension(m, m.path_length_start, m.path_length_start, dt).path_stiffness;
		if (isometric <= stiffness{}) {
			continue;
		}
		const auto path = m_graph.muscle_path(m);
		const auto wraps = m_graph.muscle_wraps(m);
		const vec3f g = wraps.empty()
			? muscle_start_linear_gradient(path, m_bodies, m_curve_words, body_idx)
			: wrapped_length_gradient(wrapped_muscle_of(path, wraps, m_curve_words, start_poses_of(path, wraps, m_bodies)), body_idx).linear;
		floor += isometric * dot(g, g);
	}
	return std::min(floor, m_config.penalty_max);
}

auto gse::vbd::solver::floored_penalty(const stiffness stored, const std::uint32_t body_a, const std::uint32_t body_b) const -> stiffness {
	if (m_graph.muscle_constraints().empty()) {
		return stored;
	}
	return std::min(std::max({ stored, m_muscle_floor[body_a], m_muscle_floor[body_b] }), m_config.penalty_max);
}

auto gse::vbd::solver::contact_normal_penalty(const contact_constraint& c) const -> stiffness {
	if (c.lambda[0] < newtons(-1e-3f) || c.c0[0] < meters(-1e-4f)) {
		return floored_penalty(c.penalty[0], c.body_a, c.body_b);
	}
	return c.penalty[0];
}

auto gse::vbd::muscle_length_gradient(const std::span<const muscle_path_point> path, const std::span<const body_state> bodies, const std::optional<moving_frame>& frame, const std::uint32_t body_idx) -> muscle_body_gradient {
	const auto segment_direction = [&](const std::size_t from, const std::size_t to) {
		const vec3<displacement> d = muscle_point_of(path[to], bodies, frame) - muscle_point_of(path[from], bodies, frame);
		return magnitude(d) > meters(1e-7f) ? normalize(d) : vec3f{};
	};

	muscle_body_gradient result{};
	for (std::size_t k = 0; k < path.size(); ++k) {
		const bool moving = path[k].frame_words != limits.no_curve_words;
		const bool on_body = path[k].body == body_idx;
		const bool on_frame_body = moving && path[k].frame_body == body_idx;
		if (!on_body && !on_frame_body) {
			continue;
		}
		vec3f g{};
		if (k > 0) {
			g += segment_direction(k - 1, k);
		}
		if (k + 1 < path.size()) {
			g -= segment_direction(k, k + 1);
		}
		if (!moving) {
			const vec3<lever_arm> r = rotate_vector(bodies[body_idx].predicted_orientation, path[k].local_point);
			result.linear += g;
			result.angular += cross(r, g) / rad;
			continue;
		}
		const auto point = moving_point_in(*frame, path[k].local_point);
		const vec3<angular_jacobian> theta_lever = moving_point_lever(point, g);
		if (on_body) {
			result.linear += g;
			result.angular += cross(point.offset, g) / rad - theta_lever;
		}
		if (on_frame_body) {
			result.angular += theta_lever;
		}
	}
	return result;
}

auto gse::vbd::muscle_length_gradient(const std::span<const muscle_path_point> path, const std::span<const body_state> bodies, const std::span<const std::uint32_t> curves, const std::uint32_t body_idx) -> muscle_body_gradient {
	return muscle_length_gradient(path, bodies, muscle_frame_of(path, bodies, curves), body_idx);
}

auto gse::vbd::wrap_obstacle_of(const std::span<const std::uint32_t> curves, const muscle_path_point& wrap, const record_pose& pose) -> wrap_obstacle {
	const auto words = wrap.frame_words;
	const vec3<displacement> center = pose.origin + rotate_vector(pose.orientation, wrap.local_point);
	return {
		.center = center,
		.orientation = pose.orientation * curve_quat(curves, words),
		.radius = meters(curve_float(curves, words + limits.wrap_radius_index)),
		.half_length = meters(curve_float(curves, words + limits.wrap_half_length_index)),
		.side = vec3f(curve_float(curves, words + limits.wrap_side_index), curve_float(curves, words + limits.wrap_side_index + 1), 0.f),
		.first_segment = curves[words + limits.wrap_first_segment_index],
		.segment_end = curves[words + limits.wrap_segment_end_index],
	};
}

auto gse::vbd::wrap_segment(const vec3<displacement>& p1, const vec3<displacement>& p2, const wrap_obstacle& obstacle) -> wrap_tangents {
	const length r = obstacle.radius;
	const length rho1 = hypot(p1.x(), p1.y());
	const length rho2 = hypot(p2.x(), p2.y());
	if (rho1 < r || rho2 < r) {
		return { .result = wrap_result::inside };
	}
	const auto side_of = [&obstacle](const length x, const length y) {
		const length s = x * obstacle.side.x() + y * obstacle.side.y();
		return s > length{} ? 1 : s < length{} ? -1 : 0;
	};

	const vec3<displacement> d = p2 - p1;
	const auto planar_squared = d.x() * d.x() + d.y() * d.y();
	const float t12 = planar_squared > decltype(planar_squared){} ? -(p1.x() * d.x() + p1.y() * d.y()) / planar_squared : 0.f;
	const length near_x = p1.x() + d.x() * t12;
	const length near_y = p1.y() + d.y() * t12;
	const bool hit = near_x * near_x + near_y * near_y < r * r && t12 > 0.f && t12 < 1.f;

	const bool constrained = obstacle.side.x() != 0.f || obstacle.side.y() != 0.f;
	auto result = wrap_result::wrapped;
	if (!constrained) {
		if (!hit) {
			return {};
		}
		result = wrap_result::mandatory;
	}

	const angle phi1 = atan2(p1.y(), p1.x());
	const angle phi2 = atan2(p2.y(), p2.x());
	const angle a1 = acos(r / rho1);
	const angle a2 = acos(r / rho2);
	const std::array<angle, 4> candidate = { phi1 + a1, phi1 - a1, phi2 + a2, phi2 - a2 };
	std::uint32_t first = 0;
	std::uint32_t second = 3;
	bool long_wrap = false;
	if (constrained) {
		const int s1 = side_of(p1.x(), p1.y());
		const int s2 = side_of(p2.x(), p2.y());
		if (s1 == 1 || s2 == 1) {
			if (hit) {
				result = wrap_result::mandatory;
			}
			else if (s1 != s2 && side_of(near_x, near_y) != 1) {
				result = wrap_result::wrapped;
			}
			else {
				return {};
			}
		}
		std::array<bool, 4> on{};
		for (std::size_t k = 0; k < on.size(); ++k) {
			on[k] = side_of(r * cos(candidate[k]), r * sin(candidate[k])) == 1;
		}
		const angle alpha = abs(wrap_angle(candidate[3] - candidate[0]));
		const angle beta = abs(wrap_angle(candidate[2] - candidate[1]));
		const bool short_pair = on[0] && on[1] ? !on[2] : on[0] ? true : on[1] ? false : on[2] ? false : on[3] ? true : alpha > beta;
		first = short_pair ? 0 : 1;
		second = short_pair ? 3 : 2;
		const length t1x = r * cos(candidate[first]);
		const length t1y = r * sin(candidate[first]);
		const length t2x = r * cos(candidate[second]);
		const length t2y = r * sin(candidate[second]);
		long_wrap = (t1x - p1.x() + t2x - p2.x()) * (t1x + t2x) + (t1y - p1.y() + t2y - p2.y()) * (t1y + t2y) < decltype(t1x * t1x){};
	}
	else {
		const auto dot_of = [&candidate](const std::uint32_t a, const std::uint32_t b) {
			return cos(candidate[a]) * cos(candidate[b]) + sin(candidate[a]) * sin(candidate[b]);
		};
		const std::array<float, 4> values = { dot_of(0, 2), dot_of(0, 3), dot_of(1, 2), dot_of(1, 3) };
		std::uint32_t best = 3;
		if (values[0] > values[1] && values[0] > values[2] && values[0] > values[3]) {
			best = 0;
		}
		else if (values[1] > values[2] && values[1] > values[3]) {
			best = 1;
		}
		else if (values[2] > values[3]) {
			best = 2;
		}
		first = best < 2 ? 0 : 1;
		second = best % 2 == 0 ? 2 : 3;
	}

	const angle full_turn = radians(2.f * std::numbers::pi_v<float>);
	const angle ang1 = candidate[first];
	const angle ang2 = candidate[second];
	const angle theta = atan2(sin(ang2 - ang1), cos(ang2 - ang1));
	const angle sweep = long_wrap ? full_turn - theta : abs(theta);
	const length l1 = sqrt(std::max(rho1 * rho1 - r * r, decltype(r * r){}));
	const length l2 = sqrt(std::max(rho2 * rho2 - r * r, decltype(r * r){}));
	const length arc_planar = r * (sweep / rad);
	const length planar = l1 + arc_planar + l2;
	const length dz = p2.z() - p1.z();
	const length z1 = p1.z() + dz * (l1 / planar);
	const length z2 = p1.z() + dz * ((l1 + arc_planar) / planar);
	const length half = obstacle.half_length;
	if ((z1 < -half || z1 > half) && (z2 < -half || z2 > half)) {
		return {};
	}
	return {
		.result = result,
		.t1 = vec3<displacement>(r * cos(ang1), r * sin(ang1), z1),
		.t2 = vec3<displacement>(r * cos(ang2), r * sin(ang2), z2),
		.arc = hypot(arc_planar, z2 - z1),
	};
}

auto gse::vbd::wrapped_length(const std::span<const vec3<displacement>> points, const wrap_contacts& contacts, const std::uint32_t wrap_count) -> length {
	length total{};
	vec3<displacement> previous = points[0];
	for (std::uint32_t k = 0; k + 1 < points.size(); ++k) {
		for (std::uint32_t order = 0; order < wrap_count; ++order) {
			for (std::uint32_t w = 0; w < wrap_count; ++w) {
				const auto& c = contacts[w];
				if (c.segment != k || c.order != order) {
					continue;
				}
				total += magnitude(c.t1 - previous) + c.arc;
				previous = c.t2;
			}
		}
		total += magnitude(points[k + 1] - previous);
		previous = points[k + 1];
	}
	return total;
}

auto gse::vbd::wrap_muscle_path(const std::span<const vec3<displacement>> points, const std::span<const wrap_obstacle> obstacles) -> wrap_contacts {
	wrap_contacts contacts{};
	const auto count = static_cast<std::uint32_t>(obstacles.size());
	std::array<std::uint32_t, physics::max_muscle_wraps> order = { 0, 1 };
	std::array<wrap_result, physics::max_muscle_wraps> results{};
	const std::uint32_t passes = count < 2 ? 1 : limits.wrap_passes;
	std::optional<length> last;
	for (std::uint32_t pass = 0; pass < passes; ++pass) {
		for (std::uint32_t i = 0; i < count; ++i) {
			const auto a = order[i];
			const auto b = 1 - a;
			const auto& obstacle = obstacles[a];
			contacts[a] = {};
			results[i] = wrap_result::no_wrap;
			std::optional<wrap_contact> best;
			length best_change = meters(std::numeric_limits<float>::max());
			bool mandatory = false;
			for (std::uint32_t k = obstacle.first_segment; k < obstacle.segment_end && !mandatory; ++k) {
				const bool shared = count == 2 && contacts[b].segment == k;
				for (std::uint32_t piece = 0; piece < (shared ? 2u : 1u); ++piece) {
					const vec3<displacement> from = shared && piece == 1 ? contacts[b].t2 : points[k];
					const vec3<displacement> to = shared && piece == 0 ? contacts[b].t1 : points[k + 1];
					const vec3<displacement> p1 = inverse_rotate_vector(obstacle.orientation, from - obstacle.center);
					const vec3<displacement> p2 = inverse_rotate_vector(obstacle.orientation, to - obstacle.center);
					const auto t = wrap_segment(p1, p2, obstacle);
					results[i] = t.result;
					if (t.result != wrap_result::wrapped && t.result != wrap_result::mandatory) {
						continue;
					}
					const length change = magnitude(t.t1 - p1) + t.arc + magnitude(p2 - t.t2) - magnitude(p2 - p1);
					if (t.result == wrap_result::mandatory || change < best_change) {
						best_change = change;
						best = wrap_contact{
							.segment = k,
							.order = shared ? piece : 0u,
							.t1 = obstacle.center + rotate_vector(obstacle.orientation, t.t1),
							.t2 = obstacle.center + rotate_vector(obstacle.orientation, t.t2),
							.arc = t.arc,
						};
					}
					if (t.result == wrap_result::mandatory) {
						mandatory = true;
						break;
					}
				}
			}
			if (best) {
				contacts[a] = *best;
				if (count == 2 && contacts[b].segment == best->segment) {
					contacts[b].order = 1 - best->order;
				}
			}
		}
		const length total = wrapped_length(points, contacts, count);
		if (last && abs(total - *last) < limits.wrap_pass_tolerance) {
			break;
		}
		last = total;
		if (pass == 0 && count > 1 && results[0] == wrap_result::no_wrap && results[1] == wrap_result::inside) {
			std::swap(order[0], order[1]);
			contacts[0] = {};
		}
	}
	return contacts;
}

auto gse::vbd::start_poses_of(const std::span<const muscle_path_point> path, const std::span<const muscle_path_point> wraps, const std::span<const body_state> bodies) -> std::array<record_pose, physics::max_muscle_path_points + physics::max_muscle_wraps> {
	const auto& first = bodies[path[0].body];
	const vec3<position> anchor = first.position + rotate_vector(first.orientation, path[0].local_point);
	std::array<record_pose, physics::max_muscle_path_points + physics::max_muscle_wraps> poses{};
	for (std::size_t k = 0; k < path.size() + wraps.size(); ++k) {
		const auto& body = bodies[k < path.size() ? path[k].body : wraps[k - path.size()].body];
		poses[k] = {
			.origin = body.position - anchor,
			.orientation = body.orientation,
		};
	}
	return poses;
}

auto gse::vbd::predicted_poses_of(const std::span<const muscle_path_point> path, const std::span<const muscle_path_point> wraps, const std::span<const body_state> bodies) -> std::array<record_pose, physics::max_muscle_path_points + physics::max_muscle_wraps> {
	const auto& first = bodies[path[0].body];
	const vec3<predicted_position> anchor = first.predicted_position + rotate_vector(first.predicted_orientation, path[0].local_point);
	std::array<record_pose, physics::max_muscle_path_points + physics::max_muscle_wraps> poses{};
	for (std::size_t k = 0; k < path.size() + wraps.size(); ++k) {
		const auto& body = bodies[k < path.size() ? path[k].body : wraps[k - path.size()].body];
		poses[k] = {
			.origin = body.predicted_position - anchor,
			.orientation = body.predicted_orientation,
		};
	}
	return poses;
}

auto gse::vbd::wrapped_muscle_of(const std::span<const muscle_path_point> path, const std::span<const muscle_path_point> wraps, const std::span<const std::uint32_t> curves, const std::span<const record_pose> poses) -> wrapped_muscle {
	std::array<vec3<displacement>, physics::max_muscle_path_points> points{};
	std::array<vec3<lever_arm>, physics::max_muscle_path_points> levers{};
	for (std::size_t k = 0; k < path.size(); ++k) {
		levers[k] = rotate_vector(poses[k].orientation, path[k].local_point);
		points[k] = poses[k].origin + levers[k];
	}
	std::array<wrap_obstacle, physics::max_muscle_wraps> obstacles{};
	for (std::size_t w = 0; w < wraps.size(); ++w) {
		obstacles[w] = wrap_obstacle_of(curves, wraps[w], poses[path.size() + w]);
	}

	wrapped_muscle result{
		.contacts = wrap_muscle_path(std::span(points).first(path.size()), std::span(obstacles).first(wraps.size())),
	};
	const auto push = [&result](const wrap_node& node) {
		result.nodes[result.count++] = node;
	};
	const auto wrap_count = static_cast<std::uint32_t>(wraps.size());
	for (std::uint32_t k = 0; k < path.size(); ++k) {
		push({ .position = points[k], .lever = levers[k], .body = path[k].body });
		for (std::uint32_t order = 0; order < wrap_count; ++order) {
			for (std::uint32_t w = 0; w < wrap_count; ++w) {
				const auto& c = result.contacts[w];
				if (c.segment != k || c.order != order) {
					continue;
				}
				const auto& origin = poses[path.size() + w].origin;
				push({ .position = c.t1, .lever = c.t1 - origin, .body = wraps[w].body, .wrap = w, .arc_follows = true });
				push({ .position = c.t2, .lever = c.t2 - origin, .body = wraps[w].body, .wrap = w });
			}
		}
	}
	return result;
}

auto gse::vbd::wrapped_path_length(const wrapped_muscle& w) -> length {
	length total{};
	for (std::uint32_t i = 1; i < w.count; ++i) {
		const auto& previous = w.nodes[i - 1];
		if (previous.arc_follows) {
			continue;
		}
		const auto& node = w.nodes[i];
		const length straight = magnitude(node.position - previous.position);
		total += node.arc_follows ? straight + w.contacts[node.wrap].arc : straight;
	}
	return total;
}

auto gse::vbd::wrapped_length_gradient(const wrapped_muscle& w, const std::uint32_t body_idx) -> muscle_body_gradient {
	muscle_body_gradient result{};
	for (std::uint32_t i = 0; i < w.count; ++i) {
		const auto& node = w.nodes[i];
		if (node.body != body_idx) {
			continue;
		}
		vec3f g{};
		if (i > 0 && !w.nodes[i - 1].arc_follows) {
			const vec3<displacement> d = node.position - w.nodes[i - 1].position;
			if (magnitude(d) > meters(1e-7f)) {
				g += normalize(d);
			}
		}
		if (i + 1 < w.count && !node.arc_follows) {
			const vec3<displacement> d = w.nodes[i + 1].position - node.position;
			if (magnitude(d) > meters(1e-7f)) {
				g -= normalize(d);
			}
		}
		result.linear += g;
		result.angular += cross(node.lever, g) / rad;
	}
	return result;
}

auto gse::vbd::muscle_start_path_length(const std::span<const muscle_path_point> path, const std::span<const muscle_path_point> wraps, const std::span<const body_state> bodies, const std::span<const std::uint32_t> curves) -> length {
	if (wraps.empty()) {
		return muscle_start_path_length(path, bodies, curves);
	}
	return wrapped_path_length(wrapped_muscle_of(path, wraps, curves, start_poses_of(path, wraps, bodies)));
}

auto gse::vbd::solver::accumulate_wrapped_muscle(const muscle_constraint& constraint, const std::uint32_t body_idx, const time_step dt) -> void {
	const auto path = m_graph.muscle_path(constraint);
	const auto wraps = m_graph.muscle_wraps(constraint);
	const auto wrapped = wrapped_muscle_of(path, wraps, m_curve_words, predicted_poses_of(path, wraps, m_bodies));
	const auto [tension, path_stiffness] = muscle_tension(constraint, wrapped_path_length(wrapped), constraint.path_length_start, dt);
	if (tension <= force{} && path_stiffness <= stiffness{}) {
		return;
	}

	const auto [g_lin, g_ang] = wrapped_length_gradient(wrapped, body_idx);
	auto& state = m_solve_state[body_idx];
	state.gradient += g_lin * tension;
	state.hessian += outer_product(g_lin, g_lin) * path_stiffness;

	if (!m_bodies[body_idx].update_orientation) {
		return;
	}
	state.angular_gradient += g_ang * tension;
	state.angular_hessian += outer_product(g_ang, g_ang) * path_stiffness;
	state.hessian_xtheta += outer_product(g_lin, g_ang) * path_stiffness;

	for (std::uint32_t i = 0; i < wrapped.count; ++i) {
		const auto& node = wrapped.nodes[i];
		if (node.body != body_idx) {
			continue;
		}
		const bool has_previous = i > 0 && !wrapped.nodes[i - 1].arc_follows;
		const bool has_next = i + 1 < wrapped.count && !node.arc_follows;
		for (const auto neighbour : { i - 1, i + 1 }) {
			if ((neighbour == i - 1 && !has_previous) || (neighbour == i + 1 && !has_next)) {
				continue;
			}
			const vec3<displacement> d = node.position - wrapped.nodes[neighbour].position;
			if (magnitude(d) <= meters(1e-7f)) {
				continue;
			}
			accumulate_geometric_stiffness(state, node.lever, normalize(d), abs(tension));
		}
	}
}

auto gse::vbd::solver::accumulate_muscle(const muscle_constraint& constraint, const std::uint32_t body_idx, const time_step dt) -> void {
	if (constraint.wrap_count != 0) {
		accumulate_wrapped_muscle(constraint, body_idx, dt);
		return;
	}
	const auto path = m_graph.muscle_path(constraint);
	const auto frame = muscle_frame_of(path, m_bodies, m_curve_words, m_solve_state[body_idx].newton);
	const auto [tension, path_stiffness] = muscle_tension(constraint, muscle_path_length(path, m_bodies, frame), constraint.path_length_start, dt);
	if (tension <= force{} && path_stiffness <= stiffness{}) {
		return;
	}

	const auto [g_lin, g_ang] = muscle_length_gradient(path, m_bodies, frame, body_idx);
	auto& state = m_solve_state[body_idx];
	state.gradient += g_lin * tension;
	state.hessian += outer_product(g_lin, g_lin) * path_stiffness;

	if (!m_bodies[body_idx].update_orientation) {
		return;
	}
	state.angular_gradient += g_ang * tension;
	state.angular_hessian += outer_product(g_ang, g_ang) * path_stiffness;
	state.hessian_xtheta += outer_product(g_lin, g_ang) * path_stiffness;

	const auto& body = m_bodies[body_idx];
	for (std::size_t k = 0; k < path.size(); ++k) {
		if (path[k].body != body_idx) {
			continue;
		}
		const vec3<lever_arm> r = path[k].frame_words == limits.no_curve_words
			? rotate_vector(body.predicted_orientation, path[k].local_point)
			: moving_point_in(*frame, path[k].local_point).offset;
		for (const auto neighbour : { k - 1, k + 1 }) {
			if (neighbour >= path.size()) {
				continue;
			}
			const vec3<displacement> d = muscle_point_of(path[k], m_bodies, frame) - muscle_point_of(path[neighbour], m_bodies, frame);
			if (magnitude(d) <= meters(1e-7f)) {
				continue;
			}
			accumulate_geometric_stiffness(state, r, normalize(d), abs(tension));
		}
	}
}
