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
import :vbd_contact_cache;
import :vbd_solver;
import :vbd_gpu_solver;

export namespace gse::physics {
	struct joint_definition {
		id entity_a;
		id entity_b;
		vbd::joint_type type = vbd::joint_type::distance;
		vec3<length> local_anchor_a;
		vec3<length> local_anchor_b;
		unitless::vec3 local_axis_a = { 0.f, 1.f, 0.f };
		unitless::vec3 local_axis_b = { 0.f, 1.f, 0.f };
		length target_distance = {};
		angle limit_lower = radians(-std::numbers::pi_v<float>);
		angle limit_upper = radians(std::numbers::pi_v<float>);
		bool limits_enabled = false;
		quat rest_orientation;

		vec3<length> pos_lambda;
		unitless::vec3 pos_penalty;
		vec3<angle> ang_lambda;
		unitless::vec3 ang_penalty;
		angle limit_lambda = {};
		float limit_penalty = 0.f;
	};

	using joint_handle = std::uint32_t;

	struct state {
		time_t<float, seconds> accumulator{};
		bool update_phys = true;
		bool use_gpu_solver = false;
		gpu::context* gpu_ctx = nullptr;

		state() = default;
		explicit state(gpu::context& ctx) : gpu_ctx(&ctx) {}

		vbd::solver vbd_solver;
		mutable vbd::gpu_solver gpu_solver;
		vbd::contact_cache contact_cache;
		std::unordered_map<id, std::uint32_t> sleep_counters;
		std::vector<joint_definition> joints;

		struct gpu_prev_frame {
			std::vector<vbd::body_state> result_bodies;
			std::vector<id> result_entity_ids;
			std::vector<vbd::warm_start_entry> warm_start_contacts;
		} gpu_prev;

		mutable struct gpu_readback_state {
			struct snapshot {
				std::vector<id> entity_ids;
				std::vector<vbd::body_state> gpu_input_bodies;
				std::vector<vbd::body_state> gpu_result_bodies;
				std::vector<vbd::contact_readback_entry> gpu_contacts;
			};

			std::optional<snapshot> pending;
			std::optional<snapshot> in_flight;
			std::deque<snapshot> completed;
		} gpu_readback;
	};

	auto create_joint(state& s, const joint_definition& def) -> joint_handle;
	auto remove_joint(state& s, joint_handle handle) -> void;

	struct system {
		static auto initialize(const initialize_phase& phase, state& s) -> void;
		static auto update(update_phase& phase, state& s) -> void;
		static auto render(render_phase& phase, const state& s) -> void;
	};
}

auto gse::physics::create_joint(state& s, const joint_definition& def) -> joint_handle {
	const auto handle = static_cast<joint_handle>(s.joints.size());
	s.joints.push_back(def);
	return handle;
}

auto gse::physics::remove_joint(state& s, const joint_handle handle) -> void {
	if (handle < s.joints.size()) {
		s.joints.erase(s.joints.begin() + handle);
	}
}

namespace gse::physics {
	struct collision_pair {
		collision_component* collision;
		motion_component* motion;
	};

	auto refresh_airborne_from_collisions(state& s, chunk<motion_component>& motion, chunk<collision_component>& collision) -> void {
		std::vector<collision_pair> objects;
		objects.reserve(collision.size());

		for (collision_component& cc : collision) {
			if (!cc.resolve_collisions) continue;
			auto* mc = motion.find(cc.owner_id());
			if (!mc) continue;
			objects.push_back({
				.collision = std::addressof(cc),
				.motion = mc
			});
			if (!mc->position_locked) {
				mc->airborne = true;
			}
		}

		const auto speculative_margin = s.vbd_solver.config().speculative_margin;
		for (std::size_t i = 0; i < objects.size(); ++i) {
			for (std::size_t j = i + 1; j < objects.size(); ++j) {
				auto& [collision_a, motion_a] = objects[i];
				auto& [collision_b, motion_b] = objects[j];

				const auto& aabb_a = collision_a->bounding_box.aabb();
				const auto& aabb_b = collision_b->bounding_box.aabb();
				if (!aabb_a.overlaps(aabb_b, speculative_margin)) continue;

				auto sat_result = narrow_phase_collision::sat_speculative(
					collision_a->bounding_box,
					collision_b->bounding_box,
					speculative_margin
				);
				if (!sat_result) continue;

				auto sat = *sat_result;
				if (dot(sat.normal, collision_b->bounding_box.center() - collision_a->bounding_box.center()) < meters(0.f)) {
					sat.normal = -sat.normal;
				}

				if (sat.normal.y() > 0.7f && motion_b && !motion_b->position_locked) {
					motion_b->airborne = false;
				}
				if (sat.normal.y() < -0.7f && motion_a && !motion_a->position_locked) {
					motion_a->airborne = false;
				}
			}
		}
	}

	struct contact_compare_key {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;
		std::uint64_t feature_key = 0;

		auto operator==(const contact_compare_key&) const -> bool = default;
	};

	struct contact_compare_key_hash {
		auto operator()(const contact_compare_key& key) const noexcept -> std::size_t {
			std::size_t seed = std::hash<std::uint32_t>{}(key.body_a);
			seed ^= std::hash<std::uint32_t>{}(key.body_b) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
			seed ^= std::hash<std::uint64_t>{}(key.feature_key) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
			return seed;
		}
	};

	auto pack_feature(const feature_id& feature) -> std::uint64_t {
		return
			(static_cast<std::uint64_t>(static_cast<std::uint8_t>(feature.type_a)) << 56) |
			(static_cast<std::uint64_t>(feature.index_a) << 48) |
			(static_cast<std::uint64_t>(feature.side_a0) << 40) |
			(static_cast<std::uint64_t>(feature.side_a1) << 32) |
			(static_cast<std::uint64_t>(static_cast<std::uint8_t>(feature.type_b)) << 24) |
			(static_cast<std::uint64_t>(feature.index_b) << 16) |
			(static_cast<std::uint64_t>(feature.side_b0) << 8) |
			static_cast<std::uint64_t>(feature.side_b1);
	}

	auto unpack_feature(const std::uint64_t packed) -> feature_id {
		return {
			.type_a = static_cast<feature_type>((packed >> 56) & 0xFF),
			.type_b = static_cast<feature_type>((packed >> 24) & 0xFF),
			.index_a = static_cast<std::uint8_t>((packed >> 48) & 0xFF),
			.index_b = static_cast<std::uint8_t>((packed >> 16) & 0xFF),
			.side_a0 = static_cast<std::uint8_t>((packed >> 40) & 0xFF),
			.side_a1 = static_cast<std::uint8_t>((packed >> 32) & 0xFF),
			.side_b0 = static_cast<std::uint8_t>((packed >> 8) & 0xFF),
			.side_b1 = static_cast<std::uint8_t>(packed & 0xFF)
		};
	}

	auto build_contact_cache_from_warm_start(
		const std::span<const vbd::warm_start_entry> warm_start_contacts
	) -> vbd::contact_cache {
		vbd::contact_cache cache;
		for (const auto& c : warm_start_contacts) {
			cache.store(c.body_a, c.body_b, unpack_feature(c.feature_key), vbd::cached_lambda{
				.lambda = c.lambda,
				.penalty = c.penalty,
				.normal = c.normal,
				.tangent_u = c.tangent_u,
				.tangent_v = c.tangent_v,
				.local_anchor_a = c.local_anchor_a,
				.local_anchor_b = c.local_anchor_b,
				.sticking = c.sticking,
				.age = 0
			});
		}
		return cache;
	}

	auto invalidate_warm_start_entries(
		std::vector<vbd::warm_start_entry>& warm_start_contacts,
		const std::span<const std::uint32_t> body_indices
	) -> void {
		if (body_indices.empty()) return;

		std::erase_if(warm_start_contacts, [&](const vbd::warm_start_entry& entry) {
			return
				std::ranges::find(body_indices, entry.body_a) != body_indices.end() ||
				std::ranges::find(body_indices, entry.body_b) != body_indices.end();
		});
	}



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
		.collision_margin = meters(0.0005f),
		.stick_threshold = meters(0.01f),
		.friction_coefficient = 0.6f,
		.velocity_sleep_threshold = meters_per_second(0.05f),
		.angular_sleep_threshold = radians_per_second(0.05f),
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

	const float alpha = s.accumulator / const_update_time;

	if (s.use_gpu_solver) {
		phase.schedule([steps, frame_time, &s, const_update_time](chunk<motion_component> motion, chunk<collision_component> collision) {
			update_vbd_gpu(steps, s, motion, collision, const_update_time);

			const float blend = std::min(frame_time / (const_update_time * 2.f), 1.f);

			for (motion_component& mc : motion) {
				if (mc.position_locked) {
					mc.render_position = mc.current_position;
					mc.render_orientation = mc.orientation;
				}
				else {
					mc.render_position = lerp(mc.render_position, mc.current_position, blend);
					mc.render_orientation = slerp(mc.render_orientation, mc.orientation, blend);
				}
			}
		});
		return;
	}

	phase.schedule([steps, alpha, &s](chunk<motion_component> motion, chunk<collision_component> collision) {
		update_vbd(steps, s, motion, collision);

		for (motion_component& mc : motion) {
			if (mc.position_locked) {
				mc.render_position = mc.current_position;
				mc.render_orientation = mc.orientation;
			}
			else {
				mc.render_position = lerp(mc.previous_position, mc.current_position, alpha);
				mc.render_orientation = slerp(mc.previous_orientation, mc.orientation, alpha);
			}
		}
	});
}

auto gse::physics::update_vbd_gpu(const int steps, state& s, chunk<motion_component>& motion, chunk<collision_component>& collision, const time_t<float, seconds> dt) -> void {
	if (!s.gpu_solver.buffers_created()) return;

	if (!s.gpu_readback.completed.empty()) {
		auto completed = std::move(s.gpu_readback.completed.front());
		s.gpu_readback.completed.pop_front();

		for (std::size_t i = 0; i < completed.entity_ids.size(); ++i) {
			const auto eid = completed.entity_ids[i];
			auto* mc = motion.find(eid);
			if (!mc) continue;

			const auto& bs = completed.gpu_result_bodies[i];

			if (!mc->position_locked) {
				const auto diff = bs.position - mc->current_position;
				const bool body_moved = magnitude(diff).as<meters>() > 1e-6f;

				if (body_moved) {
					mc->previous_position = mc->current_position;
					mc->previous_orientation = mc->orientation;
					mc->current_position = bs.position;
				}

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

		const auto& cfg = s.vbd_solver.config();
		for (const auto& c : completed.gpu_contacts) {
			const auto body_a = c.body_a;
			const auto body_b = c.body_b;
			if (body_a >= completed.entity_ids.size() || body_b >= completed.entity_ids.size()) continue;

			const auto eid_a = completed.entity_ids[body_a];
			const auto eid_b = completed.entity_ids[body_b];
			const auto& normal = c.normal;

			if (auto* mc_b = motion.find(eid_b)) {
				if (normal.y() < -0.7f) mc_b->airborne = false;
			}
			if (auto* mc_a = motion.find(eid_a)) {
				if (normal.y() > 0.7f) mc_a->airborne = false;
			}

			const auto& bs_a = completed.gpu_result_bodies[body_a];
			const auto& bs_b = completed.gpu_result_bodies[body_b];
			const auto world_r_a = rotate_vector(bs_a.orientation, c.local_anchor_a);
			const auto world_r_b = rotate_vector(bs_b.orientation, c.local_anchor_b);
			const auto contact_point_a = bs_a.position + world_r_a;
			const auto contact_point_b = bs_b.position + world_r_b;
			const auto midpoint = (contact_point_a + contact_point_b) * 0.5f;

			if (auto* cc_a = collision.find(eid_a)) {
				cc_a->collision_information.colliding = true;
				cc_a->collision_information.collision_normal = normal;
				cc_a->collision_information.penetration = -c.c0[0];
				cc_a->collision_information.collision_points.push_back(midpoint);
			}
			if (auto* cc_b = collision.find(eid_b)) {
				cc_b->collision_information.colliding = true;
				cc_b->collision_information.collision_normal = -normal;
				cc_b->collision_information.penetration = -c.c0[0];
				cc_b->collision_information.collision_points.push_back(midpoint);
			}

			const length friction_bound = abs(c.lambda[0]) * c.friction_coeff;
			const length tangential_lambda = meters(std::hypot(c.lambda[1].as<meters>(), c.lambda[2].as<meters>()));
			const length tangential_gap = meters(std::hypot(c.c0[1].as<meters>(), c.c0[2].as<meters>()));
			const bool sticking =
				c.lambda[0] < meters(-1e-3f) &&
				tangential_gap < cfg.stick_threshold &&
				tangential_lambda < friction_bound;

		}

		s.gpu_prev.warm_start_contacts.clear();
		s.gpu_prev.warm_start_contacts.reserve(completed.gpu_contacts.size());
		for (const auto& c : completed.gpu_contacts) {
			const length friction_bound = abs(c.lambda[0]) * c.friction_coeff;
			const length tangential_lambda = meters(std::hypot(c.lambda[1].as<meters>(), c.lambda[2].as<meters>()));
			const length tangential_gap = meters(std::hypot(c.c0[1].as<meters>(), c.c0[2].as<meters>()));
			const bool sticking =
				c.lambda[0] < meters(-1e-3f) &&
				tangential_gap < cfg.stick_threshold &&
				tangential_lambda < friction_bound;

			s.gpu_prev.warm_start_contacts.push_back(vbd::warm_start_entry{
				.body_a = c.body_a,
				.body_b = c.body_b,
				.feature_key = c.feature_key,
				.sticking = sticking,
				.normal = c.normal,
				.tangent_u = c.tangent_u,
				.tangent_v = c.tangent_v,
				.local_anchor_a = c.local_anchor_a,
				.local_anchor_b = c.local_anchor_b,
				.lambda = c.lambda,
				.penalty = c.penalty,
			});
		}
		std::ranges::sort(s.gpu_prev.warm_start_contacts, [](const vbd::warm_start_entry& a, const vbd::warm_start_entry& b) {
			if (a.body_a != b.body_a) return a.body_a < b.body_a;
			if (a.body_b != b.body_b) return a.body_b < b.body_b;
			return a.feature_key < b.feature_key;
		});

		s.gpu_prev.result_bodies = completed.gpu_result_bodies;
		s.gpu_prev.result_entity_ids = completed.entity_ids;
	}

	if (steps <= 0) {
		return;
	}

	std::vector<id> launched_entities;
	launched_entities.reserve(motion.size());

	for (motion_component& mc : motion) {
		if (mc.position_locked) continue;
		if (magnitude(mc.pending_impulse).as<meters_per_second>() > 1e-6f) {
			mc.current_velocity += mc.pending_impulse;
			s.sleep_counters[mc.owner_id()] = 0;
			launched_entities.push_back(mc.owner_id());
			mc.pending_impulse = {};
		}
	}

	std::unordered_map<id, std::uint32_t> id_to_body_index;
	std::vector<vbd::body_state> bodies;
	std::vector<id> entity_ids;
	std::vector<float> accel_weights;

	bodies.reserve(motion.size());
	entity_ids.reserve(motion.size());
	accel_weights.reserve(motion.size());

	std::unordered_map<id, vec3<velocity>> prev_gpu_velocity;
	prev_gpu_velocity.reserve(s.gpu_prev.result_entity_ids.size());
	for (std::size_t i = 0; i < s.gpu_prev.result_entity_ids.size() && i < s.gpu_prev.result_bodies.size(); ++i) {
		prev_gpu_velocity.emplace(s.gpu_prev.result_entity_ids[i], s.gpu_prev.result_bodies[i].body_velocity);
	}

	const float dt_s = dt.as<seconds>();
	constexpr float gravity_mag = 9.8f;

	std::uint32_t body_idx = 0;
	for (motion_component& mc : motion) {
		const auto eid = mc.owner_id();
		id_to_body_index[eid] = body_idx++;

		const auto sc_it = s.sleep_counters.find(eid);
		const auto sc = sc_it != s.sleep_counters.end() ? sc_it->second : 0u;

		float accel_weight = 0.f;
		if (!mc.position_locked && sc < 60u && dt_s > 1e-6f) {
			if (const auto prev_it = prev_gpu_velocity.find(eid); prev_it != prev_gpu_velocity.end()) {
				const float delta_vy = (mc.current_velocity.y() - prev_it->second.y()).as<meters_per_second>();
				const float accel_y = delta_vy / dt_s;
				accel_weight = std::clamp(-accel_y / gravity_mag, 0.f, 1.f);
				if (!std::isfinite(accel_weight)) {
					accel_weight = 0.f;
				}
			}
		}
		accel_weights.push_back(accel_weight);

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

	std::vector<std::uint32_t> jumped_body_indices;
	jumped_body_indices.reserve(launched_entities.size());
	for (const auto eid : launched_entities) {
		if (const auto it = id_to_body_index.find(eid); it != id_to_body_index.end()) {
			jumped_body_indices.push_back(it->second);
		}
	}
	invalidate_warm_start_entries(s.gpu_prev.warm_start_contacts, jumped_body_indices);

	std::vector<vbd::collision_body_data> collision_data(bodies.size());
	for (auto& cd : collision_data) {
		cd.aabb_min = vec3<length>(meters(1e30f));
		cd.aabb_max = vec3<length>(meters(-1e30f));
	}

	for (collision_component& cc : collision) {
		if (!cc.resolve_collisions) continue;
		const auto it = id_to_body_index.find(cc.owner_id());
		if (it == id_to_body_index.end()) continue;

		cc.bounding_box.update(bodies[it->second].position, bodies[it->second].orientation);
		const auto& [max, min] = cc.bounding_box.aabb();
		collision_data[it->second] = {
			.half_extents = cc.bounding_box.half_extents(),
			.aabb_min = min,
			.aabb_max = max
		};
	}

	std::vector<vbd::velocity_motor_constraint> motors;
	for (motion_component& mc : motion) {
		if (!mc.velocity_drive_active) continue;
		if (mc.airborne) continue;
		if (!id_to_body_index.contains(mc.owner_id())) continue;

		const auto idx = id_to_body_index[mc.owner_id()];
		if (bodies[idx].sleeping() && magnitude(mc.velocity_drive_target) > meters_per_second(.01f)) {
			bodies[idx].sleep_counter = 0;
		}

		motors.push_back(vbd::velocity_motor_constraint{
			.body_index = idx,
			.target_velocity = mc.velocity_drive_target,
			.compliance = 0.5f,
			.max_force = newtons(mc.mass.as<kilograms>() * 50.f),
			.horizontal_only = true,
		});
	}

	s.gpu_solver.upload(
		bodies,
		collision_data,
		accel_weights,
		motors,
		s.gpu_prev.warm_start_contacts,
		jumped_body_indices,
		s.vbd_solver.config(),
		dt * static_cast<float>(steps),
		steps
	);

	s.gpu_readback.pending = state::gpu_readback_state::snapshot{
		.entity_ids = entity_ids,
		.gpu_input_bodies = bodies
	};
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
		std::vector<vbd::body_state> bodies;
		bodies.reserve(motion.size());

		for (motion_component& mc : motion) {
			mc.previous_position = mc.current_position;
			mc.previous_orientation = mc.orientation;

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

				if (dot(sat.normal, collision_b->bounding_box.center() - collision_a->bounding_box.center()) < meters(0.f)) {
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
					const vec3<length> current_d = position_on_a - position_on_b;
					const length current_normal_gap = dot(constraint_normal, current_d) + cfg.collision_margin;
					const bool reuse_cached_normal =
						cached &&
						(cached->lambda[0] < meters(-1e-3f) || current_normal_gap < meters(-1e-4f));
					const bool reuse_cached_tangent =
						reuse_cached_normal &&
						cached.has_value();
					const bool reuse_cached_sticking =
						reuse_cached_tangent &&
						cached->sticking;

					vec3<length> init_lambda;
					unitless::vec3 init_penalty = { penalty_floor, penalty_floor, penalty_floor };

					if (reuse_cached_normal) {
						init_penalty[0] = std::max(cached->penalty[0], penalty_floor);

						const vec3<length> cached_normal_force = cached->normal * cached->lambda[0];
						init_lambda[0] = std::min(dot(cached_normal_force, constraint_normal), length{});
					}

					if (reuse_cached_tangent) {
						init_penalty[1] = std::max(cached->penalty[1], penalty_floor);
						init_penalty[2] = std::max(cached->penalty[2], penalty_floor);

						const vec3<length> cached_tangent_force =
							cached->tangent_u * cached->lambda[1] +
							cached->tangent_v * cached->lambda[2];

						init_lambda[1] = dot(cached_tangent_force, manifold.tangent_u);
						init_lambda[2] = dot(cached_tangent_force, manifold.tangent_v);

						const length friction_bound = abs(init_lambda[0]) * cfg.friction_coefficient;
						init_lambda[1] = std::clamp(init_lambda[1], -friction_bound, friction_bound);
						init_lambda[2] = std::clamp(init_lambda[2], -friction_bound, friction_bound);
					}

					if (reuse_cached_sticking) {
						local_r_a = cached->local_anchor_a;
						local_r_b = cached->local_anchor_b;
					}

					s.vbd_solver.add_contact_constraint(vbd::contact_constraint{
						.body_a = body_a,
						.body_b = body_b,
						.normal = constraint_normal,
						.tangent_u = manifold.tangent_u,
						.tangent_v = manifold.tangent_v,
						.r_a = local_r_a,
						.r_b = local_r_b,
						.c0 = { separation, 0.f, 0.f },
						.lambda = init_lambda,
						.penalty = init_penalty,
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
			if (!mc.velocity_drive_active) continue;
			if (mc.airborne) continue;
			const auto it = id_to_body_index.find(mc.owner_id());
			if (it == id_to_body_index.end()) continue;

			s.vbd_solver.add_motor_constraint(vbd::velocity_motor_constraint{
				.body_index = it->second,
				.target_velocity = mc.velocity_drive_target,
				.compliance = 0.5f,
				.max_force = newtons(mc.mass.as<kilograms>() * 50.f),
				.horizontal_only = true,
			});
		}

		for (auto& jd : s.joints) {
			const auto it_a = id_to_body_index.find(jd.entity_a);
			const auto it_b = id_to_body_index.find(jd.entity_b);
			if (it_a == id_to_body_index.end() || it_b == id_to_body_index.end()) continue;

			s.vbd_solver.add_joint_constraint(vbd::joint_constraint{
				.body_a = it_a->second,
				.body_b = it_b->second,
				.type = jd.type,
				.local_anchor_a = jd.local_anchor_a,
				.local_anchor_b = jd.local_anchor_b,
				.local_axis_a = jd.local_axis_a,
				.local_axis_b = jd.local_axis_b,
				.target_distance = jd.target_distance,
				.limit_lower = jd.limit_lower,
				.limit_upper = jd.limit_upper,
				.limits_enabled = jd.limits_enabled,
				.rest_orientation = jd.rest_orientation,
				.pos_lambda = jd.pos_lambda,
				.pos_penalty = jd.pos_penalty,
				.ang_lambda = jd.ang_lambda,
				.ang_penalty = jd.ang_penalty,
				.limit_lambda = jd.limit_lambda,
				.limit_penalty = jd.limit_penalty,
			});
		}

		s.vbd_solver.solve(const_update_time);

		{
			std::uint32_t ji = 0;
			const auto& solved_joints = s.vbd_solver.graph().joint_constraints();
			for (auto& jd : s.joints) {
				const auto it_a = id_to_body_index.find(jd.entity_a);
				const auto it_b = id_to_body_index.find(jd.entity_b);
				if (it_a == id_to_body_index.end() || it_b == id_to_body_index.end()) continue;
				if (ji < solved_joints.size()) {
					const auto& sj = solved_joints[ji];
					jd.pos_lambda = sj.pos_lambda;
					jd.pos_penalty = sj.pos_penalty;
					jd.ang_lambda = sj.ang_lambda;
					jd.ang_penalty = sj.ang_penalty;
					jd.limit_lambda = sj.limit_lambda;
					jd.limit_penalty = sj.limit_penalty;
				}
				++ji;
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
			if (magnitude(mc.pending_impulse).as<meters_per_second>() > 1e-6f) {
				mc.current_velocity += mc.pending_impulse;
				s.sleep_counters[mc.owner_id()] = 0;
				if (const auto it = id_to_body_index.find(mc.owner_id()); it != id_to_body_index.end()) {
					s.contact_cache.remove_body(it->second);
				}
				mc.pending_impulse = {};
			}
		}
	}
}

auto gse::physics::system::render(render_phase&, const state& s) -> void {
	if (!s.gpu_ctx || !s.use_gpu_solver) return;
	if (!s.gpu_solver.compute_initialized()) return;

	auto& config = s.gpu_ctx->config();

	s.gpu_solver.stage_readback();
	const bool had_readback = s.gpu_solver.has_readback_data();
	if (had_readback) {
		auto& in_flight = s.gpu_readback.in_flight;
		if (in_flight.has_value()) {
			auto completed = std::move(*in_flight);
			in_flight.reset();
			completed.gpu_result_bodies = completed.gpu_input_bodies;
			s.gpu_solver.readback(completed.gpu_result_bodies, completed.gpu_contacts);
			s.gpu_readback.completed.push_back(std::move(completed));
		}
		else {
			static thread_local std::vector<vbd::body_state> discard_bodies;
			static thread_local std::vector<vbd::contact_readback_entry> discard_contacts;
			discard_bodies.clear();
			discard_contacts.clear();
			s.gpu_solver.readback(discard_bodies, discard_contacts);
		}
	}

	if (s.gpu_solver.pending_dispatch() && s.gpu_solver.ready_to_dispatch()) {
		if (s.gpu_readback.pending.has_value()) {
			s.gpu_readback.in_flight = std::move(s.gpu_readback.pending);
			s.gpu_readback.pending.reset();
		}
		else {
			s.gpu_readback.in_flight.reset();
		}

		s.gpu_solver.commit_upload();
		s.gpu_solver.dispatch_compute(config);
	}
}
