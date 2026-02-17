export module gse.physics:system;

import std;

import gse.utility;
import gse.log;
import gse.physics.math;

import :narrow_phase_collision;
import :motion_component;
import :collision_component;
import :contact_manifold;
import :vbd_constraints;
import :vbd_constraint_graph;
import :vbd_contact_cache;
import :vbd_solver;
import :vbd_gpu_solver;

export namespace gse::physics {
	struct state {
		time_t<float, seconds> accumulator{};
		bool update_phys = true;
		bool use_gpu_solver = false;

		vbd::solver vbd_solver;
		vbd::gpu_solver gpu_solver;
		vbd::contact_cache contact_cache;
		std::unordered_map<id, std::uint32_t> sleep_counters;

		struct gpu_prev_frame {
			std::vector<vbd::body_state> bodies;
			std::vector<vbd::contact_constraint> contacts;
			std::vector<id> entity_ids;
		} gpu_prev;
	};

	struct system {
		static auto initialize(const initialize_phase& phase, state& s) -> void;
		static auto update(update_phase& phase, state& s) -> void;
	};
}

namespace gse::physics {
	struct collision_pair {
		collision_component* collision;
		motion_component* motion;
	};

	auto update_vbd(
		int steps,
		state& s,
		chunk<motion_component>& motion,
		chunk<collision_component>& collision
	) -> void;

	auto update_vbd_gpu(
		int steps,
		state& s,
		chunk<motion_component>& motion,
		chunk<collision_component>& collision,
		time_t<float, seconds> dt
	) -> void;
}

auto gse::physics::system::initialize(const initialize_phase& phase, state& s) -> void {
	phase.channels.push(save::register_property{
		.category = "Physics",
		.name = "Update Physics",
		.description = "Enable or disable the physics system update loop",
		.ref = &s.update_phys,
		.type = typeid(bool)
	});

	phase.channels.push(save::register_property{
		.category = "Physics",
		.name = "Use GPU Solver",
		.description = "Run VBD solver on GPU via compute shaders",
		.ref = &s.use_gpu_solver,
		.type = typeid(bool)
	});

	if (const auto* save_state = phase.try_state_of<save::state>()) {
		s.update_phys = save_state->read("Physics", "Update Physics", true);
		s.use_gpu_solver = save_state->read("Physics", "Use GPU Solver", false);
	}

	s.vbd_solver.configure(vbd::solver_config{
		.substeps = 10,
		.iterations = 20,
		.contact_compliance = 0.f,
		.contact_damping = 0.f,
		.friction_coefficient = 0.6f,
		.rho = 1.f,
		.linear_damping = 0.3f,
		.velocity_sleep_threshold = 0.05f,
		.speculative_margin = meters(0.02f)
	});
}

auto gse::physics::system::update(update_phase& phase, state& s) -> void {
	if (!s.update_phys) {
		return;
	}

	auto frame_time = system_clock::dt<time_t<float, seconds>>();
	constexpr time_t<float, seconds> max_time_step = seconds(0.25f);
	frame_time = std::min(frame_time, max_time_step);
	s.accumulator += frame_time;

	const time_t<float, seconds> const_update_time = system_clock::constant_update_time<time_t<float, seconds>>();

	int steps = 0;
	while (s.accumulator >= const_update_time) {
		s.accumulator -= const_update_time;
		steps++;
	}

	if (steps == 0) return;

	const float alpha = s.accumulator.as<seconds>() / const_update_time.as<seconds>();

	if (s.use_gpu_solver) {
		phase.schedule([steps, alpha, &s, &channels = phase.channels, const_update_time](chunk<motion_component> motion, chunk<collision_component> collision) {
			for (motion_component& mc : motion) {
				mc.render_position = mc.current_position;
				mc.render_orientation = mc.orientation;
			}

			update_vbd_gpu(steps, s, motion, collision, const_update_time);

			for (motion_component& mc : motion) {
				if (mc.position_locked) {
					mc.render_position = mc.current_position;
					mc.render_orientation = mc.orientation;
				}
				else {
					mc.render_position = lerp(mc.render_position, mc.current_position, alpha);
					mc.render_orientation = slerp(mc.render_orientation, mc.orientation, alpha);
				}
			}

			channels.push(vbd::gpu_dispatch_info{ &s.gpu_solver });
		});
		return;
	}

	phase.schedule([steps, alpha, &s, const_update_time](chunk<motion_component> motion, chunk<collision_component> collision) {
		for (motion_component& mc : motion) {
			mc.render_position = mc.current_position;
			mc.render_orientation = mc.orientation;
		}

		update_vbd(steps, s, motion, collision);

		for (motion_component& mc : motion) {
			if (mc.position_locked) {
				mc.render_position = mc.current_position;
				mc.render_orientation = mc.orientation;
			}
			else {
				mc.render_position = lerp(mc.render_position, mc.current_position, alpha);
				mc.render_orientation = slerp(mc.render_orientation, mc.orientation, alpha);
			}
		}
	});
}

auto gse::physics::update_vbd_gpu(
	const int steps,
	state& s,
	chunk<motion_component>& motion,
	chunk<collision_component>& collision,
	const time_t<float, seconds> dt
) -> void {
	if (!s.gpu_solver.buffers_created()) return;

	if (s.gpu_solver.has_readback_data() && !s.gpu_prev.entity_ids.empty()) {
		s.gpu_solver.readback(s.gpu_prev.bodies, s.gpu_prev.contacts);

		for (std::size_t i = 0; i < s.gpu_prev.entity_ids.size(); ++i) {
			const auto eid = s.gpu_prev.entity_ids[i];
			auto* mc = motion.find(eid);
			if (!mc) continue;

			if (!mc->position_locked) {
				const auto& bs = s.gpu_prev.bodies[i];

				mc->current_position = bs.position;
				mc->current_velocity = bs.body_velocity;
				if (mc->update_orientation) {
					mc->orientation = bs.orientation;
					mc->angular_velocity = bs.body_angular_velocity;
				}

				s.sleep_counters[eid] = bs.sleep_counter;
				mc->sleeping = bs.sleeping();
				mc->accumulators = {};
			}

			if (auto* cc = collision.find(eid)) {
				cc->bounding_box.update(mc->current_position, mc->orientation);
			}
		}

		for (const auto& c : s.gpu_prev.contacts) {
			s.contact_cache.store(c.body_a, c.body_b, c.feature, vbd::cached_lambda{
				.lambda = c.lambda,
				.lambda_tangent_u = c.lambda_tangent_u,
				.lambda_tangent_v = c.lambda_tangent_v,
				.age = 0
			});
		}
	}
	else {
		const auto extrap_dt = dt * static_cast<float>(steps);
		for (motion_component& mc : motion) {
			if (mc.position_locked || mc.sleeping) continue;
			const auto v = mc.current_velocity;
			mc.current_position += v * extrap_dt;
			if (auto* cc = collision.find(mc.owner_id())) {
				cc->bounding_box.update(mc.current_position, mc.orientation);
			}
		}
	}

	for (motion_component& mc : motion) {
		if (mc.position_locked) continue;
		if (mc.motor.jump_requested && !mc.airborne) {
			mc.current_velocity.y() = mc.motor.jump_speed;
			mc.airborne = true;
		}
		mc.motor.jump_requested = false;
	}

	std::unordered_map<id, std::uint32_t> id_to_body_index;
	std::vector<vbd::body_state> bodies;
	std::vector<id> entity_ids;

	bodies.reserve(motion.size());
	entity_ids.reserve(motion.size());

	std::uint32_t body_idx = 0;
	for (motion_component& mc : motion) {
		const auto eid = mc.owner_id();
		id_to_body_index[eid] = body_idx++;

		const auto sc_it = s.sleep_counters.find(eid);
		const auto sc = sc_it != s.sleep_counters.end() ? sc_it->second : 0u;

		bodies.push_back({
			.position = mc.current_position,
			.predicted_position = mc.current_position,
			.inertia_target = mc.current_position,
			.old_position = mc.current_position,
			.body_velocity = mc.current_velocity,
			.predicted_velocity = mc.current_velocity,
			.orientation = mc.orientation,
			.predicted_orientation = mc.orientation,
			.old_orientation = mc.orientation,
			.body_angular_velocity = mc.angular_velocity,
			.predicted_angular_velocity = mc.angular_velocity,
			.motor_target = mc.current_position,
			.mass_value = mc.mass,
			.inv_inertia = mc.inv_inertial_tensor(),
			.locked = mc.position_locked,
			.update_orientation = mc.update_orientation,
			.affected_by_gravity = mc.affected_by_gravity,
			.sleep_counter = sc
		});
		entity_ids.push_back(eid);
		mc.airborne = true;
	}

	for (collision_component& cc : collision) {
		if (!cc.resolve_collisions) continue;
		cc.collision_information = {
			.colliding = false,
			.collision_normal = {},
			.penetration = {},
			.collision_points = {}
		};
	}

	s.vbd_solver.begin_frame(bodies, s.contact_cache);

	std::vector<collision_pair> objects;
	objects.reserve(collision.size());

	for (collision_component& cc : collision) {
		if (!cc.resolve_collisions) continue;
		objects.push_back({
			.collision = std::addressof(cc),
			.motion = motion.find(cc.owner_id())
		});
	}

	for (std::size_t i = 0; i < objects.size(); ++i) {
		for (std::size_t j = i + 1; j < objects.size(); ++j) {
			auto& [collision_a, motion_a] = objects[i];
			auto& [collision_b, motion_b] = objects[j];

			const auto& [max_a, min_a] = collision_a->bounding_box.aabb();
			const auto& [max_b, min_b] = collision_b->bounding_box.aabb();

			const bool overlap =
				min_a.x() <= max_b.x() && max_a.x() >= min_b.x() &&
				min_a.y() <= max_b.y() && max_a.y() >= min_b.y() &&
				min_a.z() <= max_b.z() && max_a.z() >= min_b.z();

			if (!overlap) continue;

			auto sat_result = narrow_phase_collision::sat_speculative(
				collision_a->bounding_box,
				collision_b->bounding_box,
				s.vbd_solver.config().speculative_margin
			);

			if (!sat_result) continue;

			auto [normal, separation, is_speculative] = *sat_result;

			if (dot(normal, collision_b->bounding_box.center() - collision_a->bounding_box.center()) < 0.f) {
				normal = -normal;
			}

			auto manifold = narrow_phase_collision::generate_manifold(
				collision_a->bounding_box,
				collision_b->bounding_box,
				normal,
				s.vbd_solver.config().speculative_margin
			);

			if (manifold.point_count == 0) continue;

			const auto id_a = collision_a->owner_id();
			const auto id_b = collision_b->owner_id();

			if (!id_to_body_index.contains(id_a) || !id_to_body_index.contains(id_b)) continue;

			const std::uint32_t body_a = id_to_body_index[id_a];
			const std::uint32_t body_b = id_to_body_index[id_b];

			if (normal.y() > 0.7f && motion_b) motion_b->airborne = false;
			if (normal.y() < -0.7f && motion_a) motion_a->airborne = false;

			collision_a->collision_information.colliding = true;
			collision_a->collision_information.collision_normal = normal;
			collision_a->collision_information.penetration = -separation;

			collision_b->collision_information.colliding = true;
			collision_b->collision_information.collision_normal = -normal;
			collision_b->collision_information.penetration = -separation;

			for (std::uint32_t p = 0; p < manifold.point_count; ++p) {
				const auto& [position_on_a, position_on_b, normal, separation, feature] = manifold.points[p];
				const auto& body_state_a = s.vbd_solver.body_states()[body_a];
				const auto& body_state_b = s.vbd_solver.body_states()[body_b];

				const vec3<length> world_r_a = position_on_a - body_state_a.position;
				const vec3<length> world_r_b = position_on_b - body_state_b.position;

				const auto to_local = [](const quat& q, const vec3<length>& v) {
					const unitless::vec3 v_unitless = v.as<meters>();
					const unitless::vec3 rotated = mat3_cast(conjugate(q)) * v_unitless;
					return rotated * meters(1.f);
				};

				const vec3<length> local_r_a = to_local(body_state_a.orientation, world_r_a);
				const vec3<length> local_r_b = to_local(body_state_b.orientation, world_r_b);

				auto cached = s.contact_cache.lookup(body_a, body_b, feature);

				s.vbd_solver.add_contact_constraint(vbd::contact_constraint{
					.body_a = body_a,
					.body_b = body_b,
					.normal = normal,
					.tangent_u = manifold.tangent_u,
					.tangent_v = manifold.tangent_v,
					.r_a = local_r_a,
					.r_b = local_r_b,
					.initial_separation = separation,
					.compliance = s.vbd_solver.config().contact_compliance,
					.damping = s.vbd_solver.config().contact_damping,
					.lambda = cached ? cached->lambda * vbd::contact_cache::warm_start_factor : 0.f,
					.lambda_tangent_u = cached ? cached->lambda_tangent_u * vbd::contact_cache::warm_start_factor : 0.f,
					.lambda_tangent_v = cached ? cached->lambda_tangent_v * vbd::contact_cache::warm_start_factor : 0.f,
					.friction_coeff = s.vbd_solver.config().friction_coefficient,
					.feature = feature
				});

				collision_a->collision_information.collision_points.push_back(position_on_a);
			}
		}
	}

	for (motion_component& mc : motion) {
		if (!mc.motor.active) continue;
		if (!id_to_body_index.contains(mc.owner_id())) continue;

		const auto idx = id_to_body_index[mc.owner_id()];
		if (bodies[idx].sleeping() && magnitude(mc.motor.target_velocity) > 0.01f) {
			bodies[idx].sleep_counter = 0;
			s.vbd_solver.body_states()[idx].sleep_counter = 0;
		}

		s.vbd_solver.add_motor_constraint(vbd::velocity_motor_constraint{
			.body_index = idx,
			.target_velocity = mc.motor.target_velocity,
			.compliance = 0.5f,
			.max_force = newtons(mc.mass.as<kilograms>() * 50.f),
			.horizontal_only = true,
			.lambda = { 0.f, 0.f, 0.f }
		});
	}

	std::vector<bool> locked(bodies.size());
	for (std::uint32_t i = 0; i < bodies.size(); ++i) {
		locked[i] = bodies[i].locked;
	}
	s.vbd_solver.graph().compute_coloring(static_cast<std::uint32_t>(bodies.size()), locked);

	s.gpu_solver.upload(bodies, s.vbd_solver.graph(), s.vbd_solver.config(), dt * static_cast<float>(steps), steps);

	s.gpu_prev.bodies = std::move(bodies);
	s.gpu_prev.contacts = { s.vbd_solver.graph().contact_constraints().begin(), s.vbd_solver.graph().contact_constraints().end() };
	s.gpu_prev.entity_ids = std::move(entity_ids);
}

auto gse::physics::update_vbd(
	const int steps,
	state& s,
	chunk<motion_component>& motion,
	chunk<collision_component>& collision
) -> void {
	const time_t<float, seconds> const_update_time = system_clock::constant_update_time<time_t<float, seconds>>();

	for (int step = 0; step < steps; ++step) {
		std::unordered_map<id, std::uint32_t> id_to_body_index;
		std::vector<vbd::body_state> bodies;
		std::vector<motion_component*> motion_ptrs;

		bodies.reserve(motion.size());
		motion_ptrs.reserve(motion.size());

		std::uint32_t body_idx = 0;
		for (motion_component& mc : motion) {
			id_to_body_index[mc.owner_id()] = body_idx++;

			const auto eid = mc.owner_id();
			const auto sc_it = s.sleep_counters.find(eid);
			const auto sc = sc_it != s.sleep_counters.end() ? sc_it->second : 0u;

			bodies.push_back({
				.position = mc.current_position,
				.predicted_position = mc.current_position,
				.inertia_target = mc.current_position,
				.old_position = mc.current_position,
				.body_velocity = mc.current_velocity,
				.predicted_velocity = mc.current_velocity,
				.orientation = mc.orientation,
				.predicted_orientation = mc.orientation,
				.old_orientation = mc.orientation,
				.body_angular_velocity = mc.angular_velocity,
				.predicted_angular_velocity = mc.angular_velocity,
				.motor_target = mc.current_position,
				.mass_value = mc.mass,
				.inv_inertia = mc.inv_inertial_tensor(),
				.locked = mc.position_locked,
				.update_orientation = mc.update_orientation,
				.affected_by_gravity = mc.affected_by_gravity,
				.sleep_counter = sc
			});
			motion_ptrs.push_back(std::addressof(mc));

			mc.airborne = true;
		}

		for (collision_component& cc : collision) {
			if (!cc.resolve_collisions) continue;

			cc.collision_information = {
				.colliding = false,
				.collision_normal = {},
				.penetration = {},
				.collision_points = {}
			};
		}

		s.vbd_solver.begin_frame(bodies, s.contact_cache);

		std::vector<collision_pair> objects;
		objects.reserve(collision.size());

		for (collision_component& cc : collision) {
			if (!cc.resolve_collisions) continue;

			objects.push_back({
				.collision = std::addressof(cc),
				.motion = motion.find(cc.owner_id())
			});
		}

		for (std::size_t i = 0; i < objects.size(); ++i) {
			for (std::size_t j = i + 1; j < objects.size(); ++j) {
				auto& [collision_a, motion_a] = objects[i];
				auto& [collision_b, motion_b] = objects[j];

				const auto& [max_a, min_a] = collision_a->bounding_box.aabb();
				const auto& [max_b, min_b] = collision_b->bounding_box.aabb();

				const bool overlap =
					min_a.x() <= max_b.x() && max_a.x() >= min_b.x() &&
					min_a.y() <= max_b.y() && max_a.y() >= min_b.y() &&
					min_a.z() <= max_b.z() && max_a.z() >= min_b.z();

				if (!overlap) continue;

				auto sat_result = narrow_phase_collision::sat_speculative(
					collision_a->bounding_box,
					collision_b->bounding_box,
					s.vbd_solver.config().speculative_margin
				);

				if (!sat_result) continue;

				auto [normal, separation, is_speculative] = *sat_result;

				if (dot(normal, collision_b->bounding_box.center() - collision_a->bounding_box.center()) < 0.f) {
					normal = -normal;
				}

				auto manifold = narrow_phase_collision::generate_manifold(
					collision_a->bounding_box,
					collision_b->bounding_box,
					normal,
					s.vbd_solver.config().speculative_margin
				);

				if (manifold.point_count == 0) continue;

				const auto id_a = collision_a->owner_id();
				const auto id_b = collision_b->owner_id();

				if (!id_to_body_index.contains(id_a) || !id_to_body_index.contains(id_b)) continue;

				const std::uint32_t body_a = id_to_body_index[id_a];
				const std::uint32_t body_b = id_to_body_index[id_b];

				if (normal.y() > 0.7f && motion_b) {
					motion_b->airborne = false;
				}
				if (normal.y() < -0.7f && motion_a) {
					motion_a->airborne = false;
				}

				collision_a->collision_information.colliding = true;
				collision_a->collision_information.collision_normal = normal;
				collision_a->collision_information.penetration = -separation;

				collision_b->collision_information.colliding = true;
				collision_b->collision_information.collision_normal = -normal;
				collision_b->collision_information.penetration = -separation;

				for (std::uint32_t p = 0; p < manifold.point_count; ++p) {
					const auto& [position_on_a, position_on_b, normal, separation, feature] = manifold.points[p];
					const auto& body_state_a = s.vbd_solver.body_states()[body_a];
					const auto& body_state_b = s.vbd_solver.body_states()[body_b];

					const vec3<length> world_r_a = position_on_a - body_state_a.position;
					const vec3<length> world_r_b = position_on_b - body_state_b.position;

					const auto to_local = [](const quat& q, const vec3<length>& v) {
						const unitless::vec3 v_unitless = v.as<meters>();
						const unitless::vec3 rotated = mat3_cast(conjugate(q)) * v_unitless;
						return rotated * meters(1.f);
					};

					const vec3<length> local_r_a = to_local(body_state_a.orientation, world_r_a);
					const vec3<length> local_r_b = to_local(body_state_b.orientation, world_r_b);

					auto cached = s.contact_cache.lookup(body_a, body_b, feature);

					s.vbd_solver.add_contact_constraint(vbd::contact_constraint{
						.body_a = body_a,
						.body_b = body_b,
						.normal = normal,
						.tangent_u = manifold.tangent_u,
						.tangent_v = manifold.tangent_v,
						.r_a = local_r_a,
						.r_b = local_r_b,
						.initial_separation = separation,
						.compliance = s.vbd_solver.config().contact_compliance,
						.damping = s.vbd_solver.config().contact_damping,
						.lambda = cached ? cached->lambda * vbd::contact_cache::warm_start_factor : 0.f,
						.lambda_tangent_u = cached ? cached->lambda_tangent_u * vbd::contact_cache::warm_start_factor : 0.f,
						.lambda_tangent_v = cached ? cached->lambda_tangent_v * vbd::contact_cache::warm_start_factor : 0.f,
						.friction_coeff = s.vbd_solver.config().friction_coefficient,
						.feature = feature
					});

					collision_a->collision_information.collision_points.push_back(position_on_a);
				}
			}
		}

		for (motion_component& mc : motion) {
			if (!mc.motor.active) continue;
			if (!id_to_body_index.contains(mc.owner_id())) continue;

			s.vbd_solver.add_motor_constraint(vbd::velocity_motor_constraint{
				.body_index = id_to_body_index[mc.owner_id()],
				.target_velocity = mc.motor.target_velocity,
				.compliance = 0.5f,
				.max_force = newtons(mc.mass.as<kilograms>() * 50.f),
				.horizontal_only = true,
				.lambda = { 0.f, 0.f, 0.f }
			});
		}

		s.vbd_solver.solve(const_update_time);

		std::vector<vbd::body_state> result_bodies;
		s.vbd_solver.end_frame(result_bodies, s.contact_cache);

		for (std::size_t i = 0; i < motion_ptrs.size(); ++i) {
			auto* mc = motion_ptrs[i];
			const auto& bs = result_bodies[i];

			mc->current_position = bs.position;
			mc->current_velocity = bs.body_velocity;
			if (mc->update_orientation) {
				mc->orientation = bs.orientation;
				mc->angular_velocity = bs.body_angular_velocity;
			}

			s.sleep_counters[mc->owner_id()] = bs.sleep_counter;
			mc->sleeping = bs.sleeping();

			mc->accumulators = {};

			if (auto* cc = collision.find(mc->owner_id())) {
				cc->bounding_box.update(mc->current_position, mc->orientation);
			}
		}

		for (motion_component& mc : motion) {
			if (mc.position_locked) continue;

			if (mc.motor.jump_requested && !mc.airborne) {
				mc.current_velocity.y() = mc.motor.jump_speed;
				mc.airborne = true;
			}
			mc.motor.jump_requested = false;
		}
	}
}
