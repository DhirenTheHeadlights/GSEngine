export module gse.physics:system;

import std;

import gse.utility;
import gse.log;

import gse.math;
import gse.platform;
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
		std::uint64_t debug_frame = 0;

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
		.iterations = 10,
		.alpha = 0.99f,
		.beta = 100000.f,
		.gamma = 0.99f,
		.post_stabilize = true,
		.penalty_min = 1.0f,
		.penalty_max = 1e9f,
		.collision_margin = 0.0005f,
		.stick_threshold = 0.01f,
		.friction_coefficient = 0.6f,
		.velocity_sleep_threshold = 0.05f,
		.speculative_margin = meters(0.02f)
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

	phase.schedule([steps, alpha, &s](chunk<motion_component> motion, chunk<collision_component> collision) {
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
				.lambda = { lambda, 0.f, 0.f },
				.penalty = {},
				.normal = normal,
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
			s.sleep_counters[mc.owner_id()] = 0;
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
			.initial_position = mc.current_position,
			.body_velocity = mc.current_velocity,
			.orientation = mc.orientation,
			.predicted_orientation = mc.orientation,
			.angular_inertia_target = mc.orientation,
			.initial_orientation = mc.orientation,
			.body_angular_velocity = mc.angular_velocity,
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

	const auto to_local = [](const quat& q, const vec3<length>& v) {
		const unitless::vec3 v_unitless = v.as<meters>();
		const unitless::vec3 rotated = mat3_cast(conjugate(q)) * v_unitless;
		return rotated;
	};

	for (int step = 0; step < steps; ++step) {
		const std::uint64_t frame = s.debug_frame++;
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
				.initial_position = mc.current_position,
				.body_velocity = mc.current_velocity,
				.orientation = mc.orientation,
				.predicted_orientation = mc.orientation,
				.angular_inertia_target = mc.orientation,
				.initial_orientation = mc.orientation,
				.body_angular_velocity = mc.angular_velocity,
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
		bool traced_player_wall_frame = false;
		bool traced_heavy_floor_frame = false;

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

				auto& sat = *sat_result;

				if (dot(sat.normal, collision_b->bounding_box.center() - collision_a->bounding_box.center()) < 0.f) {
					sat.normal = -sat.normal;
				}

				auto manifold = narrow_phase_collision::generate_manifold(
					collision_a->bounding_box,
					collision_b->bounding_box,
					sat.normal,
					sat.separation
				);

				if (manifold.point_count == 0) continue;

				const auto id_a = collision_a->owner_id();
				const auto id_b = collision_b->owner_id();

				const auto it_a = id_to_body_index.find(id_a);
				const auto it_b = id_to_body_index.find(id_b);
				if (it_a == id_to_body_index.end() || it_b == id_to_body_index.end()) continue;

				const std::uint32_t body_a = it_a->second;
				const std::uint32_t body_b = it_b->second;
				const bool player_a = motion_a && motion_a->self_controlled;
				const bool player_b = motion_b && motion_b->self_controlled;
				const bool player_wall_contact =
					(player_a || player_b) &&
					std::abs(sat.normal.y()) < 0.35f;
				const bool heavy_a =
					motion_a &&
					!motion_a->position_locked &&
					motion_a->mass.as<kilograms>() >= 50000.f;
				const bool heavy_b =
					motion_b &&
					!motion_b->position_locked &&
					motion_b->mass.as<kilograms>() >= 50000.f;
				const bool heavy_floor_contact =
					(heavy_a || heavy_b) &&
					std::abs(sat.normal.y()) > 0.7f &&
					((motion_a && motion_a->position_locked) || (motion_b && motion_b->position_locked));

				if (player_wall_contact) {
					const motion_component& player = player_a ? *motion_a : *motion_b;
					const motion_component& other_motion = player_a ? *motion_b : *motion_a;
					log::println(
						"[PhysTrace] frame={} pair={}<->{} player={} other={} other_mass={:.1f} manifold_points={} sat_n=({:.3f},{:.3f},{:.3f}) sep={:.5f} pos=({:.3f},{:.3f},{:.3f}) vel=({:.3f},{:.3f},{:.3f}) other_vel=({:.3f},{:.3f},{:.3f}) motor=({:.3f},{:.3f},{:.3f})",
						frame,
						id_a.number(),
						id_b.number(),
						player.owner_id().number(),
						other_motion.owner_id().number(),
						other_motion.mass.as<kilograms>(),
						manifold.point_count,
						sat.normal.x(),
						sat.normal.y(),
						sat.normal.z(),
						sat.separation.as<meters>(),
						player.current_position.x().as<meters>(),
						player.current_position.y().as<meters>(),
						player.current_position.z().as<meters>(),
						player.current_velocity.x().as<meters_per_second>(),
						player.current_velocity.y().as<meters_per_second>(),
						player.current_velocity.z().as<meters_per_second>(),
						other_motion.current_velocity.x().as<meters_per_second>(),
						other_motion.current_velocity.y().as<meters_per_second>(),
						other_motion.current_velocity.z().as<meters_per_second>(),
						player.motor.target_velocity.x().as<meters_per_second>(),
						player.motor.target_velocity.y().as<meters_per_second>(),
						player.motor.target_velocity.z().as<meters_per_second>()
					);
					traced_player_wall_frame = true;
				}

				if (heavy_floor_contact) {
					const motion_component& heavy = heavy_a ? *motion_a : *motion_b;
					log::println(
						"[HeavyTrace] frame={} pair={}<->{} heavy={} mass={:.1f} manifold_points={} sat_n=({:.3f},{:.3f},{:.3f}) sep={:.5f} pos=({:.3f},{:.3f},{:.3f}) vel=({:.3f},{:.3f},{:.3f})",
						frame,
						id_a.number(),
						id_b.number(),
						heavy.owner_id().number(),
						heavy.mass.as<kilograms>(),
						manifold.point_count,
						sat.normal.x(),
						sat.normal.y(),
						sat.normal.z(),
						sat.separation.as<meters>(),
						heavy.current_position.x().as<meters>(),
						heavy.current_position.y().as<meters>(),
						heavy.current_position.z().as<meters>(),
						heavy.current_velocity.x().as<meters_per_second>(),
						heavy.current_velocity.y().as<meters_per_second>(),
						heavy.current_velocity.z().as<meters_per_second>()
					);
					traced_heavy_floor_frame = true;
				}

				if (sat.normal.y() > 0.7f && motion_b) {
					motion_b->airborne = false;
				}
				if (sat.normal.y() < -0.7f && motion_a) {
					motion_a->airborne = false;
				}

				collision_a->collision_information.colliding = true;
				collision_a->collision_information.collision_normal = sat.normal;
				collision_a->collision_information.penetration = -sat.separation;

				collision_b->collision_information.colliding = true;
				collision_b->collision_information.collision_normal = -sat.normal;
				collision_b->collision_information.penetration = -sat.separation;

				const auto& cfg = s.vbd_solver.config();
				const unitless::vec3 constraint_normal = -sat.normal;

				const auto& bs_a = s.vbd_solver.body_states()[body_a];
				const auto& bs_b = s.vbd_solver.body_states()[body_b];
				const float penalty_floor = cfg.penalty_min;

				for (std::uint32_t p = 0; p < manifold.point_count; ++p) {
					const auto& [position_on_a, position_on_b, normal, separation, feature] = manifold.points[p];

					const vec3<length> world_r_a = position_on_a - bs_a.position;
					const vec3<length> world_r_b = position_on_b - bs_b.position;

					vec3<length> local_r_a = to_local(bs_a.orientation, world_r_a);
					vec3<length> local_r_b = to_local(bs_b.orientation, world_r_b);

					auto cached = s.contact_cache.lookup(body_a, body_b, feature);
					const unitless::vec3 current_d = (position_on_a - position_on_b).as<meters>();
					const float current_normal_gap = dot(constraint_normal, current_d) + cfg.collision_margin;
					const bool reuse_cached_normal =
						cached &&
						(cached->lambda[0] < -1e-3f || current_normal_gap < -1e-4f);
					const bool reuse_cached_sticking =
						reuse_cached_normal &&
						cached &&
						cached->sticking;

					float init_lambda[3] = {};
					float init_penalty[3] = { penalty_floor, penalty_floor, penalty_floor };
					bool reused_anchors = false;

					if (reuse_cached_normal) {
						init_penalty[0] = std::max(cached->penalty[0], penalty_floor);

						const unitless::vec3 cached_normal_force = cached->normal * cached->lambda[0];
						init_lambda[0] = std::min(dot(cached_normal_force, constraint_normal), 0.f);
					}

					if (reuse_cached_sticking) {
						init_penalty[1] = std::max(cached->penalty[1], penalty_floor);
						init_penalty[2] = std::max(cached->penalty[2], penalty_floor);

						const unitless::vec3 cached_tangent_force =
							cached->tangent_u * cached->lambda[1] +
							cached->tangent_v * cached->lambda[2];

						init_lambda[1] = dot(cached_tangent_force, manifold.tangent_u);
						init_lambda[2] = dot(cached_tangent_force, manifold.tangent_v);

						const float friction_bound = std::abs(init_lambda[0]) * cfg.friction_coefficient;
						init_lambda[1] = std::clamp(init_lambda[1], -friction_bound, friction_bound);
						init_lambda[2] = std::clamp(init_lambda[2], -friction_bound, friction_bound);

						local_r_a = cached->local_anchor_a;
						local_r_b = cached->local_anchor_b;
						reused_anchors = true;
					}

					if (player_wall_contact) {
						log::println(
							"[PhysTrace] frame={} contact {}<->{} feature=({},{},{},{},{},{},{},{}) p={} cached={} cached_stick={} reuse_n={} reuse_t={} reused_anchors={} gap={:.5f} sep={:.5f} init_lambda=({:.3f},{:.3f},{:.3f}) init_pen=({:.1f},{:.1f},{:.1f}) point_a=({:.3f},{:.3f},{:.3f}) point_b=({:.3f},{:.3f},{:.3f})",
							frame,
							id_a.number(),
							id_b.number(),
							static_cast<int>(feature.type_a),
							static_cast<int>(feature.type_b),
							feature.index_a,
							feature.index_b,
							feature.side_a0,
							feature.side_a1,
							feature.side_b0,
							feature.side_b1,
							p,
							cached.has_value(),
							cached ? cached->sticking : false,
							reuse_cached_normal,
							reuse_cached_sticking,
							reused_anchors,
							current_normal_gap,
							separation.as<meters>(),
							init_lambda[0],
							init_lambda[1],
							init_lambda[2],
							init_penalty[0],
							init_penalty[1],
							init_penalty[2],
							position_on_a.x().as<meters>(),
							position_on_a.y().as<meters>(),
							position_on_a.z().as<meters>(),
							position_on_b.x().as<meters>(),
							position_on_b.y().as<meters>(),
							position_on_b.z().as<meters>()
						);
					}

					if (heavy_floor_contact) {
						const motion_component& heavy = heavy_a ? *motion_a : *motion_b;
						log::println(
							"[HeavyTrace] frame={} contact {}<->{} feature=({},{},{},{},{},{},{},{}) p={} cached={} cached_stick={} reuse_n={} reuse_t={} reused_anchors={} gap={:.5f} sep={:.5f} init_lambda=({:.3f},{:.3f},{:.3f}) init_pen=({:.1f},{:.1f},{:.1f}) pos=({:.3f},{:.3f},{:.3f}) vel=({:.3f},{:.3f},{:.3f})",
							frame,
							id_a.number(),
							id_b.number(),
							static_cast<int>(feature.type_a),
							static_cast<int>(feature.type_b),
							feature.index_a,
							feature.index_b,
							feature.side_a0,
							feature.side_a1,
							feature.side_b0,
							feature.side_b1,
							p,
							cached.has_value(),
							cached ? cached->sticking : false,
							reuse_cached_normal,
							reuse_cached_sticking,
							reused_anchors,
							current_normal_gap,
							separation.as<meters>(),
							init_lambda[0],
							init_lambda[1],
							init_lambda[2],
							init_penalty[0],
							init_penalty[1],
							init_penalty[2],
							heavy.current_position.x().as<meters>(),
							heavy.current_position.y().as<meters>(),
							heavy.current_position.z().as<meters>(),
							heavy.current_velocity.x().as<meters_per_second>(),
							heavy.current_velocity.y().as<meters_per_second>(),
							heavy.current_velocity.z().as<meters_per_second>()
						);
					}

					s.vbd_solver.add_contact_constraint(vbd::contact_constraint{
						.body_a = body_a,
						.body_b = body_b,
						.normal = constraint_normal,
						.tangent_u = manifold.tangent_u,
						.tangent_v = manifold.tangent_v,
						.r_a = local_r_a,
						.r_b = local_r_b,
						.C0 = { separation.as<meters>(), 0.f, 0.f },
						.lambda = { init_lambda[0], init_lambda[1], init_lambda[2] },
						.penalty = { init_penalty[0], init_penalty[1], init_penalty[2] },
						.penalty_floor = penalty_floor,
						.friction_coeff = cfg.friction_coefficient,
						.sticking = cached ? cached->sticking : false,
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

		for (const auto& c : s.vbd_solver.graph().contact_constraints()) {
			const bool player_a = c.body_a < motion_ptrs.size() && motion_ptrs[c.body_a] && motion_ptrs[c.body_a]->self_controlled;
			const bool player_b = c.body_b < motion_ptrs.size() && motion_ptrs[c.body_b] && motion_ptrs[c.body_b]->self_controlled;
			if (!(player_a || player_b)) continue;
			if (std::abs(c.normal.y()) >= 0.35f) continue;

			const auto& body_a = s.vbd_solver.body_states()[c.body_a];
			const auto& body_b = s.vbd_solver.body_states()[c.body_b];
			const auto* player = motion_ptrs[player_a ? c.body_a : c.body_b];
			const auto& player_body = s.vbd_solver.body_states()[player_a ? c.body_a : c.body_b];
			const auto player_id = player->owner_id().number();
			const auto* other_motion = motion_ptrs[player_a ? c.body_b : c.body_a];
			const auto& other_body = s.vbd_solver.body_states()[player_a ? c.body_b : c.body_a];
			const auto other_id = other_motion->owner_id().number();

			const vec3<length> rAW = rotate_vector(body_a.orientation, c.r_a);
			const vec3<length> rBW = rotate_vector(body_b.orientation, c.r_b);
			const vec3<length> pA = body_a.position + rAW;
			const vec3<length> pB = body_b.position + rBW;
			const unitless::vec3 d = (pA - pB).as<meters>();

			const float cn = dot(c.normal, d) + s.vbd_solver.config().collision_margin;
			const float cu = dot(c.tangent_u, d);
			const float cv = dot(c.tangent_v, d);
			const float friction_bound = std::abs(c.lambda[0]) * c.friction_coeff;

			log::println(
				"[PhysTrace] frame={} solved {}<->{} feature=({},{},{},{},{},{},{},{}) C=({:.5f},{:.5f},{:.5f}) lambda=({:.3f},{:.3f},{:.3f}) pen=({:.1f},{:.1f},{:.1f}) fric_bound={:.3f} player_vel=({:.3f},{:.3f},{:.3f}) other_vel=({:.3f},{:.3f},{:.3f}) motor=({:.3f},{:.3f},{:.3f})",
				frame,
				player_id,
				other_id,
				static_cast<int>(c.feature.type_a),
				static_cast<int>(c.feature.type_b),
				c.feature.index_a,
				c.feature.index_b,
				c.feature.side_a0,
				c.feature.side_a1,
				c.feature.side_b0,
				c.feature.side_b1,
				cn,
				cu,
				cv,
				c.lambda[0],
				c.lambda[1],
				c.lambda[2],
				c.penalty[0],
				c.penalty[1],
				c.penalty[2],
				friction_bound,
				player_body.body_velocity.x().as<meters_per_second>(),
				player_body.body_velocity.y().as<meters_per_second>(),
				player_body.body_velocity.z().as<meters_per_second>(),
				other_body.body_velocity.x().as<meters_per_second>(),
				other_body.body_velocity.y().as<meters_per_second>(),
				other_body.body_velocity.z().as<meters_per_second>(),
				player->motor.target_velocity.x().as<meters_per_second>(),
				player->motor.target_velocity.y().as<meters_per_second>(),
				player->motor.target_velocity.z().as<meters_per_second>()
			);
			traced_player_wall_frame = true;
		}

		for (const auto& c : s.vbd_solver.graph().contact_constraints()) {
			const bool heavy_a =
				c.body_a < motion_ptrs.size() &&
				motion_ptrs[c.body_a] &&
				!motion_ptrs[c.body_a]->position_locked &&
				motion_ptrs[c.body_a]->mass.as<kilograms>() >= 50000.f;
			const bool heavy_b =
				c.body_b < motion_ptrs.size() &&
				motion_ptrs[c.body_b] &&
				!motion_ptrs[c.body_b]->position_locked &&
				motion_ptrs[c.body_b]->mass.as<kilograms>() >= 50000.f;
			if (!(heavy_a || heavy_b)) continue;

			const auto* other = motion_ptrs[heavy_a ? c.body_b : c.body_a];
			if (!other || !other->position_locked) continue;
			if (std::abs(c.normal.y()) <= 0.7f) continue;

			const auto& body_a = s.vbd_solver.body_states()[c.body_a];
			const auto& body_b = s.vbd_solver.body_states()[c.body_b];
			const auto* heavy = motion_ptrs[heavy_a ? c.body_a : c.body_b];
			const auto& heavy_body = s.vbd_solver.body_states()[heavy_a ? c.body_a : c.body_b];

			const vec3<length> rAW = rotate_vector(body_a.orientation, c.r_a);
			const vec3<length> rBW = rotate_vector(body_b.orientation, c.r_b);
			const vec3<length> pA = body_a.position + rAW;
			const vec3<length> pB = body_b.position + rBW;
			const unitless::vec3 d = (pA - pB).as<meters>();

			const float cn = dot(c.normal, d) + s.vbd_solver.config().collision_margin;
			const float cu = dot(c.tangent_u, d);
			const float cv = dot(c.tangent_v, d);
			const float friction_bound = std::abs(c.lambda[0]) * c.friction_coeff;

			log::println(
				"[HeavyTrace] frame={} solved {}<->{} feature=({},{},{},{},{},{},{},{}) C=({:.5f},{:.5f},{:.5f}) lambda=({:.3f},{:.3f},{:.3f}) pen=({:.1f},{:.1f},{:.1f}) fric_bound={:.3f} pos=({:.3f},{:.3f},{:.3f}) vel=({:.3f},{:.3f},{:.3f})",
				frame,
				heavy->owner_id().number(),
				other->owner_id().number(),
				static_cast<int>(c.feature.type_a),
				static_cast<int>(c.feature.type_b),
				c.feature.index_a,
				c.feature.index_b,
				c.feature.side_a0,
				c.feature.side_a1,
				c.feature.side_b0,
				c.feature.side_b1,
				cn,
				cu,
				cv,
				c.lambda[0],
				c.lambda[1],
				c.lambda[2],
				c.penalty[0],
				c.penalty[1],
				c.penalty[2],
				friction_bound,
				heavy_body.position.x().as<meters>(),
				heavy_body.position.y().as<meters>(),
				heavy_body.position.z().as<meters>(),
				heavy_body.body_velocity.x().as<meters_per_second>(),
				heavy_body.body_velocity.y().as<meters_per_second>(),
				heavy_body.body_velocity.z().as<meters_per_second>()
			);
			traced_heavy_floor_frame = true;
		}

		if (traced_player_wall_frame || traced_heavy_floor_frame) {
			log::flush();
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
				s.sleep_counters[mc.owner_id()] = 0;
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
