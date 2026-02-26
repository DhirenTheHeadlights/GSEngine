export module gse.physics:system;

import std;

import gse.utility;

import gse.math;
import gse.platform;
import gse.log;

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
		gpu::context* gpu_ctx = nullptr;

		vbd::solver vbd_solver;
		mutable vbd::gpu_solver gpu_solver;
		vbd::contact_cache contact_cache;
		std::unordered_map<id, std::uint32_t> sleep_counters;

		struct gpu_prev_frame {
			std::vector<vbd::body_state> bodies;
			std::vector<id> entity_ids;
			std::vector<std::vector<std::uint32_t>> prev_body_colors;
			std::vector<vbd::warm_start_entry> warm_start_contacts;
		} gpu_prev;
	};

	struct system {
		static auto initialize(const initialize_phase& phase, state& s) -> void;
		static auto update(update_phase& phase, state& s) -> void;
		static auto render(render_phase& phase, const state& s) -> void;
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
		.restitution = 0.3f,
		.linear_damping = 0.3f,
		.angular_damping = 0.3f,
		.velocity_sleep_threshold = 0.05f,
		.speculative_margin = meters(0.02f),
		.penalty_min_ratio = 0.1f,
		.penalty_max = 1e9f,
		.beta = 1e5f,
		.gamma = 0.99f
	});

	if (s.gpu_ctx) {
		s.gpu_solver.initialize_compute(*s.gpu_ctx);
	}
}

auto gse::physics::system::update(update_phase& phase, state& s) -> void {
	if (!s.update_phys) {
		return;
	}

	auto frame_time = system_clock::dt<time_t<float, seconds>>();
	constexpr time_t<float, seconds> max_time_step = seconds(0.25f);
	frame_time = std::min(frame_time, max_time_step);
	s.accumulator += frame_time;

	const auto const_update_time = system_clock::constant_update_time<time_t<float, seconds>>();

	int steps = 0;
	while (s.accumulator >= const_update_time) {
		s.accumulator -= const_update_time;
		steps++;
	}

	if (steps == 0) return;

	const float alpha = s.accumulator / const_update_time;

	if (s.use_gpu_solver) {
		const int gpu_steps = std::min(steps, 1);
		phase.schedule([gpu_steps, alpha, &s, const_update_time](chunk<motion_component> motion, chunk<collision_component> collision) {
			for (motion_component& mc : motion) {
				mc.render_position = mc.current_position;
				mc.render_orientation = mc.orientation;
			}

			update_vbd_gpu(gpu_steps, s, motion, collision, const_update_time);

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

auto gse::physics::update_vbd_gpu(const int steps, state& s, chunk<motion_component>& motion, chunk<collision_component>& collision, const time_t<float, seconds> dt) -> void {
	if (!s.gpu_solver.buffers_created()) return;

	if (s.gpu_solver.has_readback_data() && !s.gpu_prev.entity_ids.empty()) {
		std::vector<vbd::contact_readback_entry> readback_contacts;
		s.gpu_solver.readback(s.gpu_prev.bodies, readback_contacts);

		for (std::size_t i = 0; i < s.gpu_prev.entity_ids.size(); ++i) {
			const auto eid = s.gpu_prev.entity_ids[i];
			auto* mc = motion.find(eid);
			if (!mc) continue;

			const auto& bs = s.gpu_prev.bodies[i];

			if (!mc->position_locked) {
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

			mc->airborne = true;

			if (auto* cc = collision.find(eid)) {
				cc->bounding_box.update(mc->current_position, mc->orientation);
				if (cc->resolve_collisions) {
					cc->collision_information = {
						.colliding = false,
						.collision_normal = {},
						.penetration = {},
						.collision_points = {}
					};
				}
			}
		}

		auto unpack_feature = [](const std::uint32_t packed) -> feature_id {
			const auto feat_a = (packed >> 16) & 0xFFFF;
			const auto feat_b = packed & 0xFFFF;
			return {
				.type_a = static_cast<feature_type>((feat_a >> 8) & 0xFF),
				.type_b = static_cast<feature_type>((feat_b >> 8) & 0xFF),
				.index_a = static_cast<std::uint8_t>(feat_a & 0xFF),
				.index_b = static_cast<std::uint8_t>(feat_b & 0xFF)
			};
		};

		for (const auto& [body_a, body_b, lambda, normal, separation, feature_packed] : readback_contacts) {
			if (body_a >= s.gpu_prev.entity_ids.size() || body_b >= s.gpu_prev.entity_ids.size()) continue;

			const auto eid_a = s.gpu_prev.entity_ids[body_a];
			const auto eid_b = s.gpu_prev.entity_ids[body_b];

			if (auto* mc_b = motion.find(eid_b)) {
				if (normal.y() > 0.7f) mc_b->airborne = false;
			}
			if (auto* mc_a = motion.find(eid_a)) {
				if (normal.y() < -0.7f) mc_a->airborne = false;
			}

			if (auto* cc_a = collision.find(eid_a)) {
				cc_a->collision_information.colliding = true;
				cc_a->collision_information.collision_normal = normal;
				cc_a->collision_information.penetration = meters(-separation);
			}
			if (auto* cc_b = collision.find(eid_b)) {
				cc_b->collision_information.colliding = true;
				cc_b->collision_information.collision_normal = -normal;
				cc_b->collision_information.penetration = meters(-separation);
			}

			s.contact_cache.store(body_a, body_b, unpack_feature(feature_packed), vbd::cached_lambda{
				.lambda = lambda,
				.lambda_tangent_u = 0.f,
				.lambda_tangent_v = 0.f,
				.age = 0
			});
		}

		vbd::constraint_graph color_graph;
		for (const auto& c : readback_contacts) {
			color_graph.add_contact(vbd::contact_constraint{
				.body_a = c.body_a,
				.body_b = c.body_b
			});
		}

		std::vector<bool> locked_prev(s.gpu_prev.bodies.size());
		for (std::size_t i = 0; i < s.gpu_prev.bodies.size(); ++i) {
			locked_prev[i] = s.gpu_prev.bodies[i].locked;
		}
		color_graph.compute_coloring(static_cast<std::uint32_t>(s.gpu_prev.bodies.size()), locked_prev);

		s.gpu_prev.prev_body_colors.clear();
		for (const auto& bc : color_graph.body_colors()) {
			s.gpu_prev.prev_body_colors.push_back(bc);
		}

		std::unordered_set<std::uint32_t> colored_bodies;
		for (const auto& bc : s.gpu_prev.prev_body_colors) {
			for (auto bi : bc) colored_bodies.insert(bi);
		}
		std::vector<std::uint32_t> uncolored;
		for (std::uint32_t i = 0; i < s.gpu_prev.bodies.size(); ++i) {
			if (!s.gpu_prev.bodies[i].locked && !colored_bodies.contains(i)) {
				uncolored.push_back(i);
			}
		}
		if (!uncolored.empty()) {
			s.gpu_prev.prev_body_colors.push_back(std::move(uncolored));
		}

		s.gpu_prev.warm_start_contacts.clear();
		s.gpu_prev.warm_start_contacts.reserve(readback_contacts.size());
		for (const auto& c : readback_contacts) {
			s.gpu_prev.warm_start_contacts.push_back(vbd::warm_start_entry{
				.body_a = c.body_a,
				.body_b = c.body_b,
				.feature_packed = c.feature_packed,
				.lambda = c.lambda
			});
		}
		std::ranges::sort(s.gpu_prev.warm_start_contacts, [](const vbd::warm_start_entry& a, const vbd::warm_start_entry& b) {
			if (a.body_a != b.body_a) return a.body_a < b.body_a;
			if (a.body_b != b.body_b) return a.body_b < b.body_b;
			return a.feature_packed < b.feature_packed;
		});
	}
	else {
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
	}

	std::vector<vbd::collision_body_data> collision_data(bodies.size());
	for (auto& cd : collision_data) {
		cd.aabb_min = unitless::vec3(1e30f, 1e30f, 1e30f);
		cd.aabb_max = unitless::vec3(-1e30f, -1e30f, -1e30f);
	}

	for (collision_component& cc : collision) {
		if (!cc.resolve_collisions) continue;
		const auto it = id_to_body_index.find(cc.owner_id());
		if (it == id_to_body_index.end()) continue;

		const auto he = cc.bounding_box.half_extents().as<meters>();
		const auto& [max, min] = cc.bounding_box.aabb();
		collision_data[it->second] = {
			.half_extents = he,
			.aabb_min = min.as<meters>(),
			.aabb_max = max.as<meters>()
		};
	}

	std::vector<vbd::velocity_motor_constraint> motors;
	for (motion_component& mc : motion) {
		if (!mc.motor.active) continue;
		if (mc.airborne) continue;
		if (!id_to_body_index.contains(mc.owner_id())) continue;

		const auto idx = id_to_body_index[mc.owner_id()];
		if (bodies[idx].sleeping() && magnitude(mc.motor.target_velocity) > 0.01f) {
			bodies[idx].sleep_counter = 0;
		}

		motors.push_back(vbd::velocity_motor_constraint{
			.body_index = idx,
			.target_velocity = mc.motor.target_velocity,
			.compliance = 0.5f,
			.max_force = newtons(mc.mass.as<kilograms>() * 50.f),
			.horizontal_only = true,
			.lambda = { 0.f, 0.f, 0.f }
		});
	}

	if (s.gpu_prev.prev_body_colors.empty()) {
		std::vector<std::uint32_t> all_dynamic;
		for (std::uint32_t i = 0; i < bodies.size(); ++i) {
			if (!bodies[i].locked) all_dynamic.push_back(i);
		}
		if (!all_dynamic.empty()) {
			s.gpu_prev.prev_body_colors.push_back(std::move(all_dynamic));
		}
	}

	s.gpu_solver.upload(
		bodies,
		collision_data,
		{},
		s.gpu_prev.prev_body_colors,
		motors,
		s.gpu_prev.warm_start_contacts,
		s.vbd_solver.config(),
		dt * static_cast<float>(steps),
		steps
	);

	s.gpu_prev.bodies = std::move(bodies);
	s.gpu_prev.entity_ids = std::move(entity_ids);
}

auto gse::physics::update_vbd(const int steps, state& s, chunk<motion_component>& motion, chunk<collision_component>& collision) -> void {
	const time_t<float, seconds> const_update_time = system_clock::constant_update_time<time_t<float, seconds>>();

	std::unordered_map<id, std::uint32_t> id_to_body_index;
	id_to_body_index.reserve(motion.size());
	std::vector<motion_component*> motion_ptrs;
	motion_ptrs.reserve(motion.size());

	{
		std::uint32_t body_idx = 0;
		for (motion_component& mc : motion) {
			id_to_body_index[mc.owner_id()] = body_idx++;
			motion_ptrs.push_back(std::addressof(mc));
		}
	}

	std::vector<collision_pair> objects;
	objects.reserve(collision.size());
	for (collision_component& cc : collision) {
		if (!cc.resolve_collisions) continue;
		objects.push_back({
			.collision = std::addressof(cc),
			.motion = motion.find(cc.owner_id())
		});
	}

	for (int step = 0; step < steps; ++step) {
		std::vector<vbd::body_state> bodies;
		bodies.reserve(motion.size());

		for (motion_component& mc : motion) {
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

		for (std::size_t i = 0; i < objects.size(); ++i) {
			for (std::size_t j = i + 1; j < objects.size(); ++j) {
				auto& [collision_a, motion_a] = objects[i];
				auto& [collision_b, motion_b] = objects[j];

				const auto& aabb_a = collision_a->bounding_box.aabb();
				const auto& aabb_b = collision_b->bounding_box.aabb();

				if (!aabb_a.overlaps(aabb_b, s.vbd_solver.config().speculative_margin)) continue;

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

				const auto it_a = id_to_body_index.find(id_a);
				const auto it_b = id_to_body_index.find(id_b);
				if (it_a == id_to_body_index.end() || it_b == id_to_body_index.end()) continue;

				const std::uint32_t body_a = it_a->second;
				const std::uint32_t body_b = it_b->second;

				static int s_contact_log = 0;
				++s_contact_log;
				const bool log_contacts = s_contact_log <= 500 && !bodies[body_a].locked && !bodies[body_b].locked;

				if (log_contacts) {
					log::println("[Contact-Gen] frame={} a={} b={} sat_n=({:.3f},{:.3f},{:.3f}) sat_sep={:.4f} spec={} pts={}",
						s_contact_log, body_a, body_b,
						normal.x(), normal.y(), normal.z(),
						separation.as<meters>(), is_speculative, manifold.point_count);
				}

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
						return rotated;
					};

					const vec3<length> local_r_a = to_local(body_state_a.orientation, world_r_a);
					const vec3<length> local_r_b = to_local(body_state_b.orientation, world_r_b);

					auto cached = s.contact_cache.lookup(body_a, body_b, feature);
					if (!cached) cached = s.contact_cache.lookup_body_pair(body_a, body_b);

					if (log_contacts) {
						log::println("[Contact-Pt] a={} b={} pt_n=({:.3f},{:.3f},{:.3f}) pt_sep={:.4f} wr_a=({:.3f},{:.3f},{:.3f}) wr_b=({:.3f},{:.3f},{:.3f}) warm={:.3f}",
							body_a, body_b,
							normal.x(), normal.y(), normal.z(), separation.as<meters>(),
							world_r_a.x().as<meters>(), world_r_a.y().as<meters>(), world_r_a.z().as<meters>(),
							world_r_b.x().as<meters>(), world_r_b.y().as<meters>(), world_r_b.z().as<meters>(),
							cached ? cached->lambda : 0.f);
						log::flush();
					}

					const auto& cfg = s.vbd_solver.config();
					const float sub_dt = const_update_time.as<seconds>() / static_cast<float>(cfg.substeps);
					const float h_sq = sub_dt * sub_dt;
					const float mass_a_val = body_state_a.locked ? 0.f : body_state_a.mass_value.as<kilograms>();
					const float mass_b_val = body_state_b.locked ? 0.f : body_state_b.mass_value.as<kilograms>();
					const float penalty_floor = cfg.penalty_min_ratio * std::max(mass_a_val, mass_b_val) / h_sq;
					const float init_penalty = cached ? std::max(cached->penalty * cfg.gamma, penalty_floor) : penalty_floor;

					s.vbd_solver.add_contact_constraint(vbd::contact_constraint{
						.body_a = body_a,
						.body_b = body_b,
						.normal = normal,
						.tangent_u = manifold.tangent_u,
						.tangent_v = manifold.tangent_v,
						.r_a = local_r_a,
						.r_b = local_r_b,
						.initial_separation = separation,
						.compliance = cfg.contact_compliance,
						.damping = cfg.contact_damping,
						.lambda = cached ? cached->lambda * vbd::contact_cache::warm_start_factor : 0.f,
						.lambda_tangent_u = cached ? cached->lambda_tangent_u * vbd::contact_cache::warm_start_factor : 0.f,
						.lambda_tangent_v = cached ? cached->lambda_tangent_v * vbd::contact_cache::warm_start_factor : 0.f,
						.penalty = init_penalty,
						.friction_coeff = cfg.friction_coefficient,
						.feature = feature
					});

					collision_a->collision_information.collision_points.push_back(position_on_a);
				}
			}
		}

		for (motion_component& mc : motion) {
			if (!mc.motor.active) continue;
			if (mc.airborne) continue;
			const auto it = id_to_body_index.find(mc.owner_id());
			if (it == id_to_body_index.end()) continue;

			s.vbd_solver.add_motor_constraint(vbd::velocity_motor_constraint{
				.body_index = it->second,
				.target_velocity = mc.motor.target_velocity,
				.compliance = 0.5f,
				.max_force = newtons(mc.mass.as<kilograms>() * 50.f),
				.horizontal_only = true,
				.lambda = { 0.f, 0.f, 0.f }
			});
		}

		s.vbd_solver.solve(const_update_time);

		{
			const auto& solver_bodies = s.vbd_solver.body_states();
			const auto& graph = s.vbd_solver.graph();
			const auto& solver_contacts = graph.contact_constraints();

			static int s_logged = 0;
			static int s_frame = 0;
			++s_frame;

			float max_v = 0.f;
			for (std::size_t bi = 0; bi < solver_bodies.size(); ++bi) {
				if (solver_bodies[bi].locked) continue;
				max_v = std::max(max_v, magnitude(solver_bodies[bi].body_velocity).as<meters_per_second>());
			}

			const bool should_log = !solver_contacts.empty() && s_logged < 500 && (s_frame % 10 == 0 || max_v > 3.f);
			if (should_log) {
				++s_logged;

				log::println("[SYS-Solve] log={} frame={} contacts={} max_v={:.2f}", s_logged, s_frame, solver_contacts.size(), max_v);

				for (std::size_t ci = 0; ci < solver_contacts.size(); ++ci) {
					const auto& c = solver_contacts[ci];
					const bool a_locked = solver_bodies[c.body_a].locked;
					const bool b_locked = solver_bodies[c.body_b].locked;
					if (a_locked || b_locked) continue;
					log::println("[SYS-C] ci={} a={} b={} lambda={:.1f} pre_vn={:.2f} n=({:.2f},{:.2f},{:.2f})",
						ci, c.body_a, c.body_b, c.lambda, c.pre_solve_v_n, c.normal.x(), c.normal.y(), c.normal.z());
				}

				for (std::size_t bi = 0; bi < solver_bodies.size(); ++bi) {
					const auto& b = solver_bodies[bi];
					if (b.locked) continue;
					const auto v = magnitude(b.body_velocity).as<meters_per_second>();
					const auto w = magnitude(b.body_angular_velocity).as<radians_per_second>();
					if (v > 0.5f || w > 0.5f || s_frame % 30 == 0) {
						log::println("[SYS-B] bi={} pos=({:.3f},{:.3f},{:.3f}) vel=({:.3f},{:.3f},{:.3f}) |v|={:.3f} |w|={:.3f}",
							bi,
							b.position.x().as<meters>(), b.position.y().as<meters>(), b.position.z().as<meters>(),
							b.body_velocity.x().as<meters_per_second>(), b.body_velocity.y().as<meters_per_second>(), b.body_velocity.z().as<meters_per_second>(),
							v, w);
					}
				}
				log::flush();
			}
		}

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

auto gse::physics::system::render(render_phase&, const state& s) -> void {
	if (!s.gpu_ctx || !s.use_gpu_solver) return;

	auto& config = s.gpu_ctx->config();
	if (!config.frame_in_progress()) return;

	const auto command = config.frame_context().command_buffer;
	const auto frame_index = config.current_frame();

	s.gpu_solver.stage_readback(frame_index);

	if (s.gpu_solver.pending_dispatch() && s.gpu_solver.ready_to_dispatch(frame_index)) {
		s.gpu_solver.commit_upload(frame_index);
		s.gpu_solver.dispatch_compute(command, config, frame_index);
	}
}
