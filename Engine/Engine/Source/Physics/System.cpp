module gse.physics;

import std;

import :system;
import :narrow_phase_collision;
import :motion_component;
import :collision_component;
import :contact_manifold;
import :vbd_constraints;
import :vbd_contact_cache;
import :vbd_solver;
import :vbd_gpu_solver;

import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.time;
import gse.diag;
import gse.save;
import gse.log;
import gse.math;
import gse.meta;
import gse.gpu;
import gse.assets;
import gse.graphics;

auto gse::physics::system::create_joint(data& d, const joint_definition& def) -> joint_handle {
	const auto handle = static_cast<joint_handle>(d.joints.size());
	d.joints.push_back(def);
	return handle;
}

auto gse::physics::join(channel_writer& channels, const id a, const id b, const fixed_joint& config) -> void {
	channels.push<joint_request>({
		.def = {
			.entity_a = a,
			.entity_b = b,
			.type = vbd::joint_type::fixed,
			.local_anchor_a = config.anchor_a,
			.local_anchor_b = config.anchor_b,
		},
	});
}

auto gse::physics::join(channel_writer& channels, const id a, const id b, const distance_joint& config) -> void {
	channels.push<joint_request>({
		.def = {
			.entity_a = a,
			.entity_b = b,
			.type = vbd::joint_type::distance,
			.target_distance = config.target,
		},
	});
}

auto gse::physics::join(channel_writer& channels, const id a, const id b, const hinge_joint& config) -> void {
	channels.push<joint_request>({
		.def = {
			.entity_a = a,
			.entity_b = b,
			.type = vbd::joint_type::hinge,
			.local_anchor_a = config.anchor_a,
			.local_anchor_b = config.anchor_b,
			.local_axis_a = config.axis,
			.local_axis_b = config.axis,
			.limit_lower = config.limits ? config.limits->first : radians(-std::numbers::pi_v<float>),
			.limit_upper = config.limits ? config.limits->second : radians(std::numbers::pi_v<float>),
			.limits_enabled = config.limits.has_value(),
		},
	});
}

auto gse::physics::join(channel_writer& channels, const id a, const id b, const slider_joint& config) -> void {
	channels.push<joint_request>({
		.def = {
			.entity_a = a,
			.entity_b = b,
			.type = vbd::joint_type::slider,
			.local_axis_a = config.axis,
			.local_axis_b = config.axis,
		},
	});
}

auto gse::physics::join(channel_writer& channels, const id a, const id b, const spring_joint& config) -> void {
	channels.push<joint_request>({
		.def = {
			.entity_a = a,
			.entity_b = b,
			.type = vbd::joint_type::distance,
			.target_distance = config.target,
			.compliance = config.compliance,
			.damping = config.damping,
		},
	});
}

auto gse::physics::system::remove_joint(data& d, const joint_handle handle) -> void {
	if (handle < d.joints.size()) {
		d.joints.erase(d.joints.begin() + handle);
	}
}

auto gse::physics::system::contact_compare_key_hash::operator()(const contact_compare_key& key) const noexcept -> std::size_t {
	return gse::hash_combine(key);
}

auto gse::physics::system::collect_collision_objects(write<transform_component>& transform, write<collision_component>& collision) -> std::vector<collision_pair> {
	std::vector<collision_pair> objects;
	objects.reserve(collision.size());
	const auto collision_ids = collision.owner_ids();
	for (std::size_t i = 0; i < collision.size(); ++i) {
		if (!collision[i].resolve_collisions) {
			continue;
		}
		const auto eid = collision_ids[i];
		if (!transform.find(eid)) {
			continue;
		}
		objects.push_back({ .owner = eid });
	}
	return objects;
}

auto gse::physics::system::add_scene_contacts_to_solver(vbd::solver& solver, vbd::contact_cache& contact_cache, const std::vector<collision_pair>& objects, const flat_map<id, std::uint32_t>& id_to_body_index, const bool update_scene_state, write<transform_component>& transform, write<motion_component>& motion, write<collision_component>& collision, write<collision_result_component>* results, write<motion_status_component>* status) -> void {
	for (std::size_t i = 0; i < objects.size(); ++i) {
		for (std::size_t j = i + 1; j < objects.size(); ++j) {
			const auto owner_a = objects[i].owner;
			const auto owner_b = objects[j].owner;
			auto* transform_a = transform.find(owner_a);
			auto* transform_b = transform.find(owner_b);
			auto* collision_a = collision.find(owner_a);
			auto* collision_b = collision.find(owner_b);
			if (!transform_a || !transform_b || !collision_a || !collision_b) {
				continue;
			}

			const auto aabb_a = world_aabb_of(*transform_a, *collision_a);
			const auto aabb_b = world_aabb_of(*transform_b, *collision_b);

			if (!aabb_a.overlaps(aabb_b, solver.config().speculative_margin)) {
				continue;
			}

			const narrow_phase_collision::shape_data sd_a{
				.tc = transform_a,
				.shape = &collision_a->shape,
			};
			const narrow_phase_collision::shape_data sd_b{
				.tc = transform_b,
				.shape = &collision_b->shape,
			};

			auto sat_result = narrow_phase_collision::speculative_test(
				sd_a, sd_b, solver.config().speculative_margin
			);

			if (!sat_result) {
				continue;
			}

			auto& sat = *sat_result;
			if (dot(sat.normal, transform_b->position - transform_a->position) < meters(0.f)) {
				sat.normal = -sat.normal;
			}

			auto manifold = narrow_phase_collision::generate_shape_manifold(
				sd_a, sd_b, sat.normal, sat.separation
			);

			if (manifold.point_count == 0) {
				continue;
			}

			const auto it_a = id_to_body_index.find(owner_a);
			const auto it_b = id_to_body_index.find(owner_b);
			if (it_a == id_to_body_index.end() || it_b == id_to_body_index.end()) {
				continue;
			}

			const std::uint32_t body_a = it_a->second;
			const std::uint32_t body_b = it_b->second;

			if (update_scene_state) {
				if (status) {
					if (sat.normal.y() > 0.7f) {
						if (auto* st_b = status->find(owner_b)) {
							st_b->airborne = false;
						}
					}
					if (sat.normal.y() < -0.7f) {
						if (auto* st_a = status->find(owner_a)) {
							st_a->airborne = false;
						}
					}
				}

				if (results) {
					if (auto* res_a = results->find(owner_a)) {
						res_a->colliding = true;
						res_a->collision_normal = sat.normal;
						res_a->penetration = -sat.separation;
					}
					if (auto* res_b = results->find(owner_b)) {
						res_b->colliding = true;
						res_b->collision_normal = -sat.normal;
						res_b->penetration = -sat.separation;
					}
				}
			}

			const auto& cfg = solver.config();
			const vec3f constraint_normal = -sat.normal;

			const auto& bs_a = solver.body_states()[body_a];
			const auto& bs_b = solver.body_states()[body_b];
			const stiffness penalty_floor = cfg.penalty_min;

			for (std::uint32_t p = 0; p < manifold.point_count; ++p) {
				const auto& [position_on_a, position_on_b, normal, separation, feature] = manifold.points[p];

				const vec3<lever_arm> world_r_a = position_on_a - bs_a.position;
				const vec3<lever_arm> world_r_b = position_on_b - bs_b.position;

				vec3<lever_arm> local_r_a = inverse_rotate_vector(bs_a.orientation, world_r_a);
				vec3<lever_arm> local_r_b = inverse_rotate_vector(bs_b.orientation, world_r_b);

				auto cached = contact_cache.lookup(body_a, body_b, feature);
				const vec3<gap> current_d = position_on_a - position_on_b;
				const length current_normal_gap = dot(constraint_normal, current_d) + cfg.collision_margin;
				const bool reuse_cached_normal =
					cached &&
					(cached->lambda[0] < newtons(-1e-3f) || current_normal_gap < meters(-1e-4f));
				const bool reuse_cached_tangent =
					reuse_cached_normal &&
					cached.has_value();
				const bool reuse_cached_sticking =
					reuse_cached_tangent &&
					cached->sticking;

				vec3<force> init_lambda;
				vec3<stiffness> init_penalty = { penalty_floor, penalty_floor, penalty_floor };

				if (reuse_cached_normal) {
					init_penalty[0] = std::max(cached->penalty[0], penalty_floor);

					const vec3<force> cached_normal_force = cached->normal * cached->lambda[0];
					init_lambda[0] = std::min(dot(cached_normal_force, constraint_normal), force{});
				}

				if (reuse_cached_tangent) {
					init_penalty[1] = std::max(cached->penalty[1], penalty_floor);
					init_penalty[2] = std::max(cached->penalty[2], penalty_floor);

					const vec3<force> cached_tangent_force =
						cached->tangent_u * cached->lambda[1] +
						cached->tangent_v * cached->lambda[2];

					init_lambda[1] = dot(cached_tangent_force, manifold.tangent_u);
					init_lambda[2] = dot(cached_tangent_force, manifold.tangent_v);

					const force friction_bound = abs(init_lambda[0]) * cfg.friction_coefficient;
					init_lambda[1] = std::clamp(init_lambda[1], -friction_bound, friction_bound);
					init_lambda[2] = std::clamp(init_lambda[2], -friction_bound, friction_bound);
				}

				if (reuse_cached_sticking) {
					local_r_a = cached->local_anchor_a;
					local_r_b = cached->local_anchor_b;
				}

				const auto restitution_of = [&](const id eid) -> float {
					const auto* m = motion.find(eid);
					if (!m) return 0.f;
					const auto* d = std::get_if<dynamic_body>(&m->body);
					return d ? d->restitution : 0.f;
				};
				const float pair_restitution = std::max(restitution_of(owner_a), restitution_of(owner_b));

				solver.add_contact_constraint(vbd::contact_constraint{
					.body_a = body_a,
					.body_b = body_b,
					.feature_key = pack_feature(feature),
					.sticking = (cached && cached->sticking) ? 1u : 0u,
					.normal = constraint_normal,
					.tangent_u = manifold.tangent_u,
					.tangent_v = manifold.tangent_v,
					.local_anchor_a = local_r_a,
					.local_anchor_b = local_r_b,
					.c0 = { separation, 0.f, 0.f },
					.friction_coeff = cfg.friction_coefficient,
					.restitution = pair_restitution,
					.penalty_floor = penalty_floor,
					.lambda = init_lambda,
					.penalty = init_penalty,
				});

				if (update_scene_state && results) {
					if (auto* res_a = results->find(owner_a)) {
						res_a->collision_points.push_back(position_on_a);
					}
				}
			}
		}
	}
}

auto gse::physics::system::build_contact_cache_from_warm_start(const std::span<const vbd::contact_constraint> warm_start_contacts) -> vbd::contact_cache {
	vbd::contact_cache cache;
	for (const auto& c : warm_start_contacts) {
		cache.store(c.body_a, c.body_b, unpack_feature(c.feature_key), vbd::cached_lambda{
			.lambda = vec3<force>{ c.lambda },
			.penalty = vec3<stiffness>{ c.penalty },
			.normal = vec3f{ c.normal },
			.tangent_u = vec3f{ c.tangent_u },
			.tangent_v = vec3f{ c.tangent_v },
			.local_anchor_a = vec3<lever_arm>{ c.local_anchor_a },
			.local_anchor_b = vec3<lever_arm>{ c.local_anchor_b },
			.sticking = c.sticking != 0,
			.age = 0
		});
	}
	return cache;
}

auto gse::physics::system::invalidate_warm_start_entries(std::vector<vbd::contact_constraint>& warm_start_contacts, const std::span<const std::uint32_t> body_indices) -> void {
	if (body_indices.empty()) {
		return;
	}

	std::size_t i = 0;
	while (i < warm_start_contacts.size()) {
		if (const auto& entry = warm_start_contacts[i]; std::ranges::find(body_indices, entry.body_a) != body_indices.end() || std::ranges::find(body_indices, entry.body_b) != body_indices.end()) {
			warm_start_contacts[i] = std::move(warm_start_contacts[warm_start_contacts.size() - 1]);
			warm_start_contacts.pop_back();
		}
		else {
			++i;
		}
	}
}

auto gse::physics::system::run(run_context& ctx, const gpu::context::data* gpu_s, const asset::data& assets_s, data& d) -> async::task<> {
	d.vbd_solver.configure(vbd::solver_config{
		.iterations = static_cast<std::uint32_t>(d.solver_iterations),
		.alpha = 0.99f,
		.beta = newtons_per_meter(100000.f),
		.gamma = 0.99f,
		.post_stabilize = true,
		.penalty_min = newtons_per_meter(1.0f),
		.penalty_max = newtons_per_meter(1e9f),
		.collision_margin = meters(0.0005f),
		.stick_threshold = meters(0.01f),
		.friction_coefficient = 0.6f,
		.velocity_sleep_threshold = meters_per_second(0.05f),
		.angular_sleep_threshold = radians_per_second(0.05f),
		.speculative_margin = meters(0.02f)
	});

	if (gpu_s) {
		co_await d.gpu_solver.initialize_compute(ctx, *gpu_s);
		d.gpu_buffers_created = d.gpu_solver.buffers_created();
	}

	while (true) {
		for (const auto& req : ctx.read_channel<joint_request>()) {
			system::create_joint(d, req.def);
		}

		for (const auto owner : ctx.drain_component_adds<collision_component>()) {
			ctx.add_component<collision_result_component>(owner);
		}

		for (const auto owner : ctx.drain_component_adds<motion_component>()) {
			ctx.add_component<motion_status_component>(owner);
		}

		if (const auto& readbacks = ctx.read_channel<gpu_readback_result>(); !readbacks.empty()) {
			auto completed = readbacks[0];
			d.completed_readback = std::move(completed);
		}
		if (const auto& stats_channel = ctx.read_channel<gpu_solver_stats>(); !stats_channel.empty()) {
			d.gpu_stats = stats_channel[0];
		}

		if (!d.update_phys) {
			co_await ctx.next_tick();
			continue;
		}

		if (auto solver_cfg = d.vbd_solver.config();
			solver_cfg.iterations != static_cast<std::uint32_t>(d.solver_iterations) ||
			solver_cfg.use_jacobi != d.use_jacobi ||
			solver_cfg.jacobi_omega != d.jacobi_omega) {
			solver_cfg.iterations = static_cast<std::uint32_t>(d.solver_iterations);
			solver_cfg.use_jacobi = d.use_jacobi;
			solver_cfg.jacobi_omega = d.jacobi_omega;
			d.vbd_solver.configure(solver_cfg);
		}

		if (!d.tick_clock_primed) {
			d.tick_clock.reset<float>();
			d.tick_clock_primed = true;
		}

		const time_t<float, seconds> elapsed = d.tick_clock.reset<float>();
		constexpr time_t<float, seconds> max_time_step = seconds(0.25f);
		const time_t<float, seconds> frame_time = std::min(elapsed, max_time_step);
		d.accumulator += frame_time;

		const auto const_update_time = system_clock::constant_update_time<time_t<float, seconds>>();

		int steps = 0;
		while (d.accumulator >= const_update_time) {
			d.accumulator -= const_update_time;
			steps++;
		}

		if (constexpr int max_physics_steps = 4; steps > max_physics_steps) {
			d.accumulator = {};
			steps = max_physics_steps;
		}

		{
			auto [transform, motion, status, motor, collision, results] = co_await ctx.acquire<
				write<transform_component>,
				write<motion_component>,
				write<motion_status_component>,
				read<motor_component>,
				write<collision_component>,
				write<collision_result_component>
			>();

			const auto impulses = ctx.read_channel<impulse_request>();

			if (d.use_gpu_solver) {
				update_vbd_gpu(steps, d, transform, motion, status, motor, collision, results, impulses, const_update_time, ctx.channels);
			}
			else {
				update_vbd(steps, d, transform, motion, status, motor, collision, results, impulses);
			}
		}

		co_await ctx.next_tick();
	}
}

auto gse::physics::system::update_vbd_gpu(const int steps, data& d, write<transform_component>& transform, write<motion_component>& motion, write<motion_status_component>& status, read<motor_component>& motor, write<collision_component>& collision, write<collision_result_component>& results, std::span<const impulse_request> impulses, const time_t<float, seconds> dt, channel_writer& channels) -> void {
	if (!d.gpu_buffers_created) {
		return;
	}

	if (d.completed_readback) {
		{
			trace::scope_guard sg{trace_id<"vbd_gpu::readback">()};
			auto completed = std::move(*d.completed_readback);
			d.completed_readback.reset();

			{
				const auto body_count = completed.entity_ids.size();
				const std::span<const id> entity_ids{ completed.entity_ids };
				const bool transform_order_matches = body_count == transform.size() && std::ranges::equal(entity_ids, transform.owner_ids());
				const bool motion_order_matches = body_count == motion.size() && std::ranges::equal(entity_ids, motion.owner_ids());
				const bool status_order_matches = body_count == status.size() && std::ranges::equal(entity_ids, status.owner_ids());
				const bool collision_order_matches = body_count == collision.size() && std::ranges::equal(entity_ids, collision.owner_ids());

				for (std::size_t i = 0; i < body_count; ++i) {
					const auto eid = completed.entity_ids[i];
					auto* mc = motion_order_matches ? std::addressof(motion[i]) : motion.find(eid);
					auto* tc = transform_order_matches ? std::addressof(transform[i]) : transform.find(eid);
					auto* st = status_order_matches ? std::addressof(status[i]) : status.find(eid);
					if (!mc || !tc) {
						continue;
					}

					const auto& bs = completed.gpu_result_bodies[i];
					const auto* dyn = std::get_if<dynamic_body>(&mc->body);

					if (dyn) {
						tc->position = bs.position;
						mc->current_velocity = bs.velocity;
						if (dyn->update_orientation) {
							tc->orientation = bs.orientation;
							mc->angular_velocity = bs.angular_velocity;
						}

						d.sleep_counters[eid] = bs.sleep_counter;
						if (st) {
							st->sleeping = bs.sleeping();
						}
					}

					if (st) {
						st->airborne = true;
					}

					auto* cc = collision_order_matches ? std::addressof(collision[i]) : collision.find(eid);
					if (cc && cc->resolve_collisions) {
						if (auto* info = results.find(eid)) {
							info->colliding = false;
							info->collision_normal = {};
							info->penetration = {};
							info->collision_points.clear();
						}
					}
				}
			}

			{
				const auto& cfg = d.vbd_solver.config();
				d.gpu_prev.warm_start_contacts.clear();
				d.gpu_prev.warm_start_contacts.reserve(completed.gpu_contacts.size());

				for (const auto& c : completed.gpu_contacts) {
					const force friction_bound = abs(c.lambda[0]) * c.friction_coeff;
					const force tangential_lambda = hypot(c.lambda[1], c.lambda[2]);
					const length tangential_gap = hypot(c.c0[1], c.c0[2]);
					const bool sticking =
						c.lambda[0] < newtons(-1e-3f) &&
						tangential_gap < cfg.stick_threshold &&
						tangential_lambda < friction_bound;

					d.gpu_prev.warm_start_contacts.push_back({
						.body_a = c.body_a,
						.body_b = c.body_b,
						.feature_key = c.feature_key,
						.sticking = sticking ? 1u : 0u,
						.normal = c.normal,
						.tangent_u = c.tangent_u,
						.tangent_v = c.tangent_v,
						.local_anchor_a = c.local_anchor_a,
						.local_anchor_b = c.local_anchor_b,
						.lambda = c.lambda,
						.penalty = c.penalty,
					});

					const auto body_a = c.body_a;
					const auto body_b = c.body_b;
					if (body_a >= completed.entity_ids.size() || body_b >= completed.entity_ids.size()) {
						continue;
					}

					const auto eid_a = completed.entity_ids[body_a];
					const auto eid_b = completed.entity_ids[body_b];
					const vec3f normal{ c.normal };

					if (auto* st_b = status.find(eid_b)) {
						if (normal.y() < -0.7f) {
							st_b->airborne = false;
						}
					}
					if (auto* st_a = status.find(eid_a)) {
						if (normal.y() > 0.7f) {
							st_a->airborne = false;
						}
					}

					const auto& bs_a = completed.gpu_result_bodies[body_a];
					const auto& bs_b = completed.gpu_result_bodies[body_b];
					const auto world_r_a = rotate_vector(bs_a.orientation, vec3<lever_arm>{ c.local_anchor_a });
					const auto world_r_b = rotate_vector(bs_b.orientation, vec3<lever_arm>{ c.local_anchor_b });
					const auto contact_point_a = bs_a.position + world_r_a;
					const auto contact_point_b = bs_b.position + world_r_b;
					const auto midpoint = contact_point_a + (contact_point_b - contact_point_a) * 0.5f;

					if (auto* res_a = results.find(eid_a)) {
						res_a->colliding = true;
						res_a->collision_normal = normal;
						res_a->penetration = -c.c0[0];
						res_a->collision_points.push_back(midpoint);
					}
					if (auto* res_b = results.find(eid_b)) {
						res_b->colliding = true;
						res_b->collision_normal = -normal;
						res_b->penetration = -c.c0[0];
						res_b->collision_points.push_back(midpoint);
					}
				}
			}

			std::ranges::sort(
				d.gpu_prev.warm_start_contacts,
				[](const vbd::contact_constraint& a, const vbd::contact_constraint& b) {
					if (a.body_a != b.body_a) {
						return a.body_a < b.body_a;
					}
					if (a.body_b != b.body_b) {
						return a.body_b < b.body_b;
					}
					return a.feature_key < b.feature_key;
				}
			);

			{
				std::uint32_t ji = 0;
				for (auto& jd : d.joints) {
					if (ji >= completed.gpu_joint_readback.size()) {
						break;
					}
					const auto& sj = completed.gpu_joint_readback[ji];
					jd.pos_lambda = sj.pos_lambda;
					jd.pos_penalty = sj.pos_penalty;
					jd.ang_lambda = sj.ang_lambda;
					jd.ang_penalty = sj.ang_penalty;
					jd.limit_lambda = sj.limit_lambda;
					jd.limit_penalty = sj.limit_penalty;
					++ji;
				}
			}

			if (d.comparison_pending.has_value()) {
				const auto& cpu = d.comparison_pending->cpu_result;
				const auto& gpu = completed.gpu_result_bodies;
				const auto& cpu_contacts = d.comparison_pending->cpu_contacts;
				const auto& cpu_joints = d.comparison_pending->cpu_joints;
				const auto count = std::min(cpu.size(), gpu.size());

				length max_pos_err{};
				velocity max_vel_err{};
				angular_velocity max_ang_err{};
				std::uint32_t worst_pos_idx = 0;

				for (std::uint32_t i = 0; i < count; ++i) {
					if (cpu[i].locked) {
						continue;
					}
					const auto pe = magnitude(gpu[i].position - cpu[i].position);
					const auto ve = magnitude(gpu[i].velocity - cpu[i].velocity);
					const auto ae = magnitude(gpu[i].angular_velocity - cpu[i].angular_velocity);
					if (pe > max_pos_err) {
						max_pos_err = pe;
						worst_pos_idx = i;
					}
					if (ve > max_vel_err) {
						max_vel_err = ve;
					}
					if (ae > max_ang_err) {
						max_ang_err = ae;
					}
				}

				std::unordered_map<contact_compare_key, const vbd::contact_constraint*, contact_compare_key_hash> cpu_contact_map;
				cpu_contact_map.reserve(cpu_contacts.size());
				for (const auto& c : cpu_contacts) {
					cpu_contact_map[contact_compare_key{
						.body_a = c.body_a,
						.body_b = c.body_b,
						.feature_key = c.feature_key
					}] = std::addressof(c);
				}

				std::unordered_map<contact_compare_key, const vbd::contact_constraint*, contact_compare_key_hash> gpu_contact_map;
				gpu_contact_map.reserve(completed.gpu_contacts.size());
				for (const auto& c : completed.gpu_contacts) {
					gpu_contact_map[contact_compare_key{
						.body_a = c.body_a,
						.body_b = c.body_b,
						.feature_key = c.feature_key
					}] = std::addressof(c);
				}

				std::uint32_t matched_contacts = 0;
				std::uint32_t cpu_only_contacts = 0;
				std::uint32_t gpu_only_contacts = 0;
				force max_contact_lambda_err{};
				stiffness max_contact_penalty_err{};
				length max_contact_c0_err{};
				contact_compare_key worst_contact_key{};
				const vbd::contact_constraint* worst_cpu_contact = nullptr;
				const vbd::contact_constraint* worst_gpu_contact = nullptr;

				for (const auto& [key, cpu_contact] : cpu_contact_map) {
					const auto it = gpu_contact_map.find(key);
					if (it == gpu_contact_map.end()) {
						++cpu_only_contacts;
						continue;
					}

					++matched_contacts;
					const auto* gpu_contact = it->second;
					const force lambda_err = magnitude(cpu_contact->lambda - gpu_contact->lambda);
					const stiffness penalty_err = magnitude(cpu_contact->penalty - gpu_contact->penalty);
					const length c0_err = magnitude(cpu_contact->c0 - gpu_contact->c0);

					if (lambda_err > max_contact_lambda_err) {
						max_contact_lambda_err = lambda_err;
						max_contact_penalty_err = penalty_err;
						max_contact_c0_err = c0_err;
						worst_contact_key = key;
						worst_cpu_contact = cpu_contact;
						worst_gpu_contact = gpu_contact;
					}
					else if (penalty_err > max_contact_penalty_err) {
						max_contact_penalty_err = penalty_err;
						max_contact_c0_err = c0_err;
						worst_contact_key = key;
						worst_cpu_contact = cpu_contact;
						worst_gpu_contact = gpu_contact;
					}
				}

				for (const auto& key : gpu_contact_map | std::views::keys) {
					if (!cpu_contact_map.contains(key)) {
						++gpu_only_contacts;
					}
				}

				const auto joint_count = std::min(cpu_joints.size(), completed.gpu_joint_readback.size());
				force max_joint_pos_lambda_err{};
				stiffness max_joint_pos_penalty_err{};
				torque max_joint_ang_lambda_err{};
				angular_stiffness max_joint_ang_penalty_err{};
				torque max_joint_limit_lambda_err{};
				angular_stiffness max_joint_limit_penalty_err{};
				std::uint32_t worst_joint_pos_idx = 0;
				std::uint32_t worst_joint_ang_idx = 0;
				std::uint32_t worst_joint_limit_idx = 0;

				for (std::uint32_t i = 0; i < joint_count; ++i) {
					const auto pos_lambda_err = magnitude(cpu_joints[i].pos_lambda - completed.gpu_joint_readback[i].pos_lambda);
					const auto pos_penalty_err = magnitude(cpu_joints[i].pos_penalty - completed.gpu_joint_readback[i].pos_penalty);
					const auto ang_lambda_err = magnitude(cpu_joints[i].ang_lambda - completed.gpu_joint_readback[i].ang_lambda);
					const auto ang_penalty_err = magnitude(cpu_joints[i].ang_penalty - completed.gpu_joint_readback[i].ang_penalty);
					const auto limit_lambda_err = abs(cpu_joints[i].limit_lambda - completed.gpu_joint_readback[i].limit_lambda);
					const auto limit_penalty_err = abs(cpu_joints[i].limit_penalty - completed.gpu_joint_readback[i].limit_penalty);

					if (pos_lambda_err > max_joint_pos_lambda_err) {
						max_joint_pos_lambda_err = pos_lambda_err;
						worst_joint_pos_idx = i;
					}
					if (pos_penalty_err > max_joint_pos_penalty_err) {
						max_joint_pos_penalty_err = pos_penalty_err;
						worst_joint_pos_idx = i;
					}
					if (ang_lambda_err > max_joint_ang_lambda_err) {
						max_joint_ang_lambda_err = ang_lambda_err;
						worst_joint_ang_idx = i;
					}
					if (ang_penalty_err > max_joint_ang_penalty_err) {
						max_joint_ang_penalty_err = ang_penalty_err;
						worst_joint_ang_idx = i;
					}
					if (limit_lambda_err > max_joint_limit_lambda_err) {
						max_joint_limit_lambda_err = limit_lambda_err;
						worst_joint_limit_idx = i;
					}
					if (limit_penalty_err > max_joint_limit_penalty_err) {
						max_joint_limit_penalty_err = limit_penalty_err;
						worst_joint_limit_idx = i;
					}
				}

				log::println(
					"SOLVER COMPARE: bodies={} max_pos_err={} (body {}), max_vel_err={}, max_ang_err={}, contacts cpu={} gpu={} matched={} cpu_only={} gpu_only={}, joints cpu={} gpu={}",
					count,
					max_pos_err,
					worst_pos_idx,
					max_vel_err,
					max_ang_err,
					cpu_contacts.size(),
					completed.gpu_contacts.size(),
					matched_contacts,
					cpu_only_contacts,
					gpu_only_contacts,
					cpu_joints.size(),
					completed.gpu_joint_readback.size()
				);

				log::println(
					"  contact deltas: max_lambda_err={}, max_penalty_err={}, max_c0_err={}",
					max_contact_lambda_err,
					max_contact_penalty_err,
					max_contact_c0_err
				);

				log::println(
					"  joint deltas: max_pos_lambda_err={} (joint {}), max_pos_penalty_err={}, max_ang_lambda_err={} (joint {}), max_ang_penalty_err={}, max_limit_lambda_err={} (joint {}), max_limit_penalty_err={}",
					max_joint_pos_lambda_err,
					worst_joint_pos_idx,
					max_joint_pos_penalty_err,
					max_joint_ang_lambda_err,
					worst_joint_ang_idx,
					max_joint_ang_penalty_err,
					max_joint_limit_lambda_err,
					worst_joint_limit_idx,
					max_joint_limit_penalty_err
				);

	#if 0
				if (max_pos_err > meters(1e-4f)) {
					const auto& cb = cpu[worst_pos_idx];
					const auto& gb = gpu[worst_pos_idx];
					log::println(
						"  worst body[{}] entity={}: cpu_pos={} gpu_pos={} cpu_vel={} gpu_vel={} cpu_ang={} gpu_ang={}",
						worst_pos_idx,
						worst_pos_idx < completed.entity_ids.size() ? completed.entity_ids[worst_pos_idx] : id{},
						cb.position,
						gb.position,
						cb.velocity,
						gb.velocity,
						cb.angular_velocity,
						gb.angular_velocity
					);
				}
	#endif

				if (worst_cpu_contact && worst_gpu_contact) {
					log::println(
						"  worst contact: bodies=({}, {}), feature={}, cpu_lambda={} gpu_lambda={} cpu_penalty={} gpu_penalty={} cpu_c0={} gpu_c0={}",
						worst_contact_key.body_a,
						worst_contact_key.body_b,
						worst_contact_key.feature_key,
						worst_cpu_contact->lambda,
						worst_gpu_contact->lambda,
						worst_cpu_contact->penalty,
						worst_gpu_contact->penalty,
						worst_cpu_contact->c0,
						worst_gpu_contact->c0
					);
				}

				d.comparison_pending.reset();
			}

			d.gpu_prev.result_bodies.assign(
				completed.gpu_result_bodies.begin(),
				completed.gpu_result_bodies.end()
			);
			d.gpu_prev.result_entity_ids.assign(
				completed.entity_ids.begin(),
				completed.entity_ids.end()
			);
		}
	}

	if (steps <= 0) {
		return;
	}

	std::vector<id> launched_entities;
	{
		trace::scope_guard sg{trace_id<"vbd_gpu::impulses">()};
		launched_entities.reserve(impulses.size());

		for (const auto& req : impulses) {
			auto* mc = motion.find(req.target);
			if (!mc || !is_dynamic(*mc)) {
				continue;
			}
			mc->current_velocity += req.impulse / mass_of(*mc);
			d.sleep_counters[req.target] = 0;
			launched_entities.push_back(req.target);
			if (auto* st = status.find(req.target)) {
				st->sleeping = false;
			}
		}
	}

	flat_map<id, std::uint32_t> id_to_body_index;
	std::vector<vbd::body_state> bodies;
	std::vector<id> entity_ids;
	std::optional<flat_map<id, vec3<velocity>>> prev_gpu_velocity;
	const auto& prev_result_entity_ids = d.gpu_prev.result_entity_ids;
	const auto& prev_result_bodies = d.gpu_prev.result_bodies;
	bool prev_order_matches = false;

	std::vector<std::pair<id, std::uint32_t>> id_to_body_index_staging;

	{
		trace::scope_guard sg{trace_id<"vbd_gpu::build_bodies">()};
		const auto body_count = motion.size();
		bodies.resize(body_count);
		entity_ids.resize(body_count);
		id_to_body_index_staging.resize(body_count);

		const auto prev_count = std::min(prev_result_entity_ids.size(), prev_result_bodies.size());
		const std::span<const id> prev_ids{ prev_result_entity_ids };
		prev_order_matches = prev_count == body_count && std::ranges::equal(prev_ids, motion.owner_ids());
		if (!prev_order_matches) {
			std::vector<std::pair<id, vec3<velocity>>> prev_staging;
			prev_staging.reserve(prev_count);
			for (std::size_t i = 0; i < prev_count; ++i) {
				prev_staging.emplace_back(prev_result_entity_ids[i], prev_result_bodies[i].velocity);
			}
			prev_gpu_velocity.emplace();
			prev_gpu_velocity->assign_unsorted(std::move(prev_staging));
		}

		constexpr acceleration gravity_mag = meters_per_second_squared(9.8f);
		const auto& sleep_counters_ref = d.sleep_counters;
		const auto& prev_gpu_velocity_ref = prev_gpu_velocity;

		const auto build_motion_ids = motion.owner_ids();
		const bool build_transform_order_matches = transform.size() == body_count && std::ranges::equal(build_motion_ids, transform.owner_ids());
		task::parallel_invoke_range(0, body_count, [&, dt](std::size_t i) {
			motion_component& mc = motion[i];
			const auto eid = build_motion_ids[i];
			const auto* tc = build_transform_order_matches ? std::addressof(transform[i]) : transform.find(eid);
			if (!tc) {
				return;
			}
			id_to_body_index_staging[i] = { eid, static_cast<std::uint32_t>(i) };

			const auto sc_it = sleep_counters_ref.find(eid);
			const auto sc = sc_it != sleep_counters_ref.end() ? sc_it->second : 0u;

			const auto* dyn = std::get_if<dynamic_body>(&mc.body);
			const bool locked = dyn == nullptr;

			float accel_weight = 0.f;
			if (!locked && sc < 60u && dt > time_t<float, seconds>(seconds(1e-6f))) {
				if (prev_order_matches) {
					const velocity delta_vy = mc.current_velocity.y() - prev_result_bodies[i].velocity.y();
					const acceleration accel_y = delta_vy / dt;
					accel_weight = std::clamp(-accel_y / gravity_mag, 0.f, 1.f);
					if (!std::isfinite(accel_weight)) {
						accel_weight = 0.f;
					}
				}
				else if (prev_gpu_velocity_ref) {
					if (const auto prev_it = prev_gpu_velocity_ref->find(eid); prev_it != prev_gpu_velocity_ref->end()) {
						const velocity delta_vy = mc.current_velocity.y() - prev_it->second.y();
						const acceleration accel_y = delta_vy / dt;
						accel_weight = std::clamp(-accel_y / gravity_mag, 0.f, 1.f);
						if (!std::isfinite(accel_weight)) {
							accel_weight = 0.f;
						}
					}
				}
			}
			bodies[i] = {
				.position = tc->position,
				.predicted_position = tc->position,
				.inertia_target = tc->position,
				.old_position = tc->position,
				.velocity = mc.current_velocity,
				.orientation = tc->orientation,
				.predicted_orientation = tc->orientation,
				.angular_inertia_target = tc->orientation,
				.old_orientation = tc->orientation,
				.angular_velocity = mc.angular_velocity,
				.mass = dyn ? dyn->mass : kilograms(0.f),
				.locked = locked ? 1u : 0u,
				.update_orientation = (dyn && dyn->update_orientation) ? 1u : 0u,
				.affected_by_gravity = (dyn && dyn->affected_by_gravity) ? 1u : 0u,
				.sleep_counter = sc,
				.accel_weight = accel_weight,
				.restitution = dyn ? dyn->restitution : 0.f,
				.inv_inertia = inv_inertial_tensor(mc, tc->orientation),
			};
			entity_ids[i] = eid;
		});

		id_to_body_index.assign_unsorted(std::move(id_to_body_index_staging));
	}

	std::vector<std::uint32_t> jumped_body_indices;

	{
		trace::scope_guard sg{trace_id<"vbd_gpu::build_collision">()};
			jumped_body_indices.reserve(launched_entities.size());
			for (const auto eid : launched_entities) {
				if (const auto it = id_to_body_index.find(eid); it != id_to_body_index.end()) {
					jumped_body_indices.push_back(it->second);
				}
			}
			invalidate_warm_start_entries(d.gpu_prev.warm_start_contacts, jumped_body_indices);

			for (auto& b : bodies) {
				b.aabb_min = vec3<position>(position(1e30f));
				b.aabb_max = vec3<position>(position(-1e30f));
			}

			const auto& id_to_body_index_ref = id_to_body_index;
			const auto collision_ids = collision.owner_ids();
			const bool coll_transform_order_matches = transform.size() == collision.size() && std::ranges::equal(collision_ids, transform.owner_ids());
			task::parallel_invoke_range(
				0,
				collision.size(),
				[&](std::size_t i) {
					collision_component& cc = collision[i];
					if (!cc.resolve_collisions) {
						return;
					}
					const auto eid = collision_ids[i];
					const auto it = id_to_body_index_ref.find(eid);
					if (it == id_to_body_index_ref.end()) {
						return;
					}
					const auto* tc = coll_transform_order_matches ? std::addressof(transform[i]) : transform.find(eid);
					if (!tc) {
						return;
					}

					const transform_component body_tc{
						.position = bodies[it->second].position,
						.orientation = bodies[it->second].orientation,
					};
					const auto bb = std::visit([&](const auto& shape) { return gse::bounding_box(body_tc, shape); }, cc.shape);
					const auto [max, min] = bb.aabb();
					auto& b = bodies[it->second];
					b.half_extents = bb.half_extents();
					b.aabb_min = min;
					b.aabb_max = max;
				}
			);
	}

	std::vector<vbd::velocity_motor_constraint> motors;
	{
		trace::scope_guard sg{trace_id<"vbd_gpu::build_motors">()};
		const auto motor_ids = motor.owner_ids();
		motors.reserve(motor.size());
		for (std::size_t i = 0; i < motor.size(); ++i) {
			const auto eid = motor_ids[i];
			const auto& mt = motor[i];
			if (auto* st = status.find(eid); st && st->airborne) {
				continue;
			}
			const auto it = id_to_body_index.find(eid);
			if (it == id_to_body_index.end()) {
				continue;
			}
			const auto* mc = motion.find(eid);
			if (!mc || !is_dynamic(*mc)) {
				continue;
			}

			const auto idx = it->second;
			if (bodies[idx].sleeping() && magnitude(mt.velocity_drive_target) > meters_per_second(.01f)) {
				bodies[idx].sleep_counter = 0;
			}

			motors.push_back(vbd::velocity_motor_constraint{
				.body_index = idx,
				.horizontal_only = mt.horizontal_only ? 1u : 0u,
				.target_velocity = mt.velocity_drive_target,
				.compliance = 0.5f,
				.max_force = mass_of(*mc) * meters_per_second_squared(50.f),
			});
		}
	}

	std::vector<vbd::joint_constraint> gpu_joints;
	{
		trace::scope_guard sg{trace_id<"vbd_gpu::build_joints">()};
		gpu_joints.reserve(d.joints.size());
		for (auto& jd : d.joints) {
			const auto it_a = id_to_body_index.find(jd.entity_a);
			const auto it_b = id_to_body_index.find(jd.entity_b);
			if (it_a == id_to_body_index.end() || it_b == id_to_body_index.end()) {
				continue;
			}

			if (!jd.rest_orientation_initialized && jd.type != vbd::joint_type::distance) {
				jd.rest_orientation = bodies[it_b->second].orientation * conjugate(bodies[it_a->second].orientation);
				jd.rest_orientation_initialized = true;
			}

			gpu_joints.push_back(vbd::joint_constraint{
				.body_a = it_a->second,
				.body_b = it_b->second,
				.type = jd.type,
				.limits_enabled = jd.limits_enabled ? 1u : 0u,
				.local_anchor_a = jd.local_anchor_a,
				.local_anchor_b = jd.local_anchor_b,
				.local_axis_a = jd.local_axis_a,
				.local_axis_b = jd.local_axis_b,
				.target_distance = jd.target_distance,
				.compliance = jd.compliance,
				.damping = jd.damping,
				.limit_lower = jd.limit_lower,
				.limit_upper = jd.limit_upper,
				.rest_orientation = jd.rest_orientation,
				.pos_lambda = jd.pos_lambda,
				.pos_penalty = jd.pos_penalty,
				.ang_lambda = jd.ang_lambda,
				.ang_penalty = jd.ang_penalty,
				.limit_lambda = jd.limit_lambda,
				.limit_penalty = jd.limit_penalty,
			});
		}
	}

	{
		trace::scope_guard sg{trace_id<"vbd_gpu::upload">()};
		channels.push<gpu_upload_payload>({
			.bodies = bodies,
			.motors = motors,
			.joints = gpu_joints,
			.warm_starts = d.gpu_prev.warm_start_contacts,
			.solver_cfg = d.vbd_solver.config(),
			.dt = dt * static_cast<float>(steps),
			.steps = steps * std::max(d.physics_substeps, 1),
			.entity_ids = entity_ids,
			.joint_count = static_cast<std::uint32_t>(gpu_joints.size())
		});

		gpu_body_index_map body_map;
		body_map.entries.reserve(id_to_body_index.size());
		for (const auto& [eid, idx] : id_to_body_index) {
			body_map.entries.emplace_back(eid, idx);
		}
		channels.push(std::move(body_map));
	}

	if (d.compare_solvers && d.comparison_timer.tick()) {
		{
			trace::scope_guard sg{trace_id<"vbd_gpu::compare">()};
		if (steps != 1) {
			log::println("SOLVER COMPARE: skipped for {} fixed steps; single-step captures are the only apples-to-apples parity check right now", steps);
		}
		else {
			vbd::solver cpu_ref;
			cpu_ref.configure(d.vbd_solver.config());
			auto ref_cache = build_contact_cache_from_warm_start(d.gpu_prev.warm_start_contacts);
			cpu_ref.begin_frame(bodies, ref_cache);

			std::vector<vec3<velocity>> ref_prev_velocities;
			ref_prev_velocities.reserve(entity_ids.size());
			for (std::size_t i = 0; i < entity_ids.size(); ++i) {
				if (prev_order_matches && i < prev_result_bodies.size()) {
					ref_prev_velocities.push_back(prev_result_bodies[i].velocity);
				}
				else if (prev_gpu_velocity) {
					if (const auto it = prev_gpu_velocity->find(entity_ids[i]); it != prev_gpu_velocity->end()) {
						ref_prev_velocities.push_back(it->second);
					}
					else {
						ref_prev_velocities.push_back(bodies[i].velocity);
					}
				}
				else {
					ref_prev_velocities.push_back(bodies[i].velocity);
				}
			}
			cpu_ref.seed_previous_velocities(ref_prev_velocities);

			const auto objects = collect_collision_objects(transform, collision);
			add_scene_contacts_to_solver(cpu_ref, ref_cache, objects, id_to_body_index, false, transform, motion, collision, nullptr, nullptr);

			for (const auto& m : motors) {
				cpu_ref.add_motor_constraint(m);
			}
			for (const auto& j : gpu_joints) {
				cpu_ref.add_joint_constraint(j);
			}

			cpu_ref.solve(dt);

			std::vector<vbd::body_state> cpu_result;
			cpu_ref.end_frame(cpu_result, ref_cache);

			d.comparison_pending = system::data::solver_comparison_snapshot{
				.cpu_result = std::move(cpu_result),
				.cpu_contacts = cpu_ref.graph().contact_constraints() | std::views::all | std::ranges::to<std::vector>(),
				.cpu_joints = cpu_ref.graph().joint_constraints() | std::views::all | std::ranges::to<std::vector>(),
			};
		}
		}
	}
}

auto gse::physics::system::update_vbd(const int steps, data& d, write<transform_component>& transform, write<motion_component>& motion, write<motion_status_component>& status, read<motor_component>& motor, write<collision_component>& collision, write<collision_result_component>& results, std::span<const impulse_request> impulses) -> void {
	const auto const_update_time = system_clock::constant_update_time<time_t<float, seconds>>();
	const int substeps = std::max(d.physics_substeps, 1);
	const auto sub_dt = const_update_time / static_cast<float>(substeps);

	for (const auto& req : impulses) {
		auto* mc = motion.find(req.target);
		if (!mc || !is_dynamic(*mc)) {
			continue;
		}
		mc->current_velocity += req.impulse / mass_of(*mc);
		d.sleep_counters[req.target] = 0;
		if (auto* st = status.find(req.target)) {
			st->sleeping = false;
		}
	}

	flat_map<id, std::uint32_t> id_to_body_index;
	{
		std::vector<std::pair<id, std::uint32_t>> id_staging;
		id_staging.reserve(motion.size());
		const auto motion_ids = motion.owner_ids();
		for (std::size_t i = 0; i < motion.size(); ++i) {
			id_staging.emplace_back(motion_ids[i], static_cast<std::uint32_t>(i));
		}
		id_to_body_index.assign_unsorted(std::move(id_staging));
	}

	const auto objects = collect_collision_objects(transform, collision);

	const int total_substeps = steps * substeps;
	for (int step = 0; step < total_substeps; ++step) {
		std::vector<vbd::body_state> bodies;
		bodies.reserve(motion.size());

		const auto step_motion_ids = motion.owner_ids();
		const bool step_transform_order_matches = transform.size() == motion.size() && std::ranges::equal(step_motion_ids, transform.owner_ids());
		for (std::size_t i = 0; i < motion.size(); ++i) {
			motion_component& mc = motion[i];
			const auto eid = step_motion_ids[i];
			const auto* tc = step_transform_order_matches ? std::addressof(transform[i]) : transform.find(eid);
			if (!tc) {
				continue;
			}
			const auto sc_it = d.sleep_counters.find(eid);
			const auto sc = sc_it != d.sleep_counters.end() ? sc_it->second : 0u;

			const auto* dyn = std::get_if<dynamic_body>(&mc.body);
			const bool locked = dyn == nullptr;

			bodies.push_back({
				.position = tc->position,
				.predicted_position = tc->position,
				.inertia_target = tc->position,
				.old_position = tc->position,
				.velocity = mc.current_velocity,
				.orientation = tc->orientation,
				.predicted_orientation = tc->orientation,
				.angular_inertia_target = tc->orientation,
				.old_orientation = tc->orientation,
				.angular_velocity = mc.angular_velocity,
				.mass = dyn ? dyn->mass : kilograms(0.f),
				.locked = locked ? 1u : 0u,
				.update_orientation = (dyn && dyn->update_orientation) ? 1u : 0u,
				.affected_by_gravity = (dyn && dyn->affected_by_gravity) ? 1u : 0u,
				.sleep_counter = sc,
				.restitution = dyn ? dyn->restitution : 0.f,
				.inv_inertia = inv_inertial_tensor(mc, tc->orientation),
			});

			if (auto* st = status.find(eid)) {
				st->airborne = true;
			}
		}

		for (collision_result_component& res : results) {
			res.colliding = false;
			res.collision_normal = {};
			res.penetration = {};
			res.collision_points.clear();
		}

		d.vbd_solver.begin_frame(bodies, d.contact_cache);
		add_scene_contacts_to_solver(d.vbd_solver, d.contact_cache, objects, id_to_body_index, true, transform, motion, collision, &results, &status);

		const auto motor_step_ids = motor.owner_ids();
		for (std::size_t i = 0; i < motor.size(); ++i) {
			const auto eid = motor_step_ids[i];
			const auto& mt = motor[i];
			if (auto* st = status.find(eid); st && st->airborne) {
				continue;
			}
			const auto it = id_to_body_index.find(eid);
			if (it == id_to_body_index.end()) {
				continue;
			}
			const auto* mc = motion.find(eid);
			if (!mc || !is_dynamic(*mc)) {
				continue;
			}

			auto& solver_body = d.vbd_solver.body_states()[it->second];
			if (solver_body.sleeping() && magnitude(mt.velocity_drive_target) > meters_per_second(.01f)) {
				solver_body.sleep_counter = 0;
			}

			d.vbd_solver.add_motor_constraint(vbd::velocity_motor_constraint{
				.body_index = it->second,
				.horizontal_only = mt.horizontal_only ? 1u : 0u,
				.target_velocity = mt.velocity_drive_target,
				.compliance = 0.5f,
				.max_force = mass_of(*mc) * meters_per_second_squared(50.f),
			});
		}

		for (auto& jd : d.joints) {
			const auto it_a = id_to_body_index.find(jd.entity_a);
			const auto it_b = id_to_body_index.find(jd.entity_b);
			if (it_a == id_to_body_index.end() || it_b == id_to_body_index.end()) {
				continue;
			}

			if (!jd.rest_orientation_initialized && jd.type != vbd::joint_type::distance) {
				jd.rest_orientation = bodies[it_b->second].orientation * conjugate(bodies[it_a->second].orientation);
				jd.rest_orientation_initialized = true;
			}

			d.vbd_solver.add_joint_constraint(vbd::joint_constraint{
				.body_a = it_a->second,
				.body_b = it_b->second,
				.type = jd.type,
				.limits_enabled = jd.limits_enabled ? 1u : 0u,
				.local_anchor_a = jd.local_anchor_a,
				.local_anchor_b = jd.local_anchor_b,
				.local_axis_a = jd.local_axis_a,
				.local_axis_b = jd.local_axis_b,
				.target_distance = jd.target_distance,
				.compliance = jd.compliance,
				.damping = jd.damping,
				.limit_lower = jd.limit_lower,
				.limit_upper = jd.limit_upper,
				.rest_orientation = jd.rest_orientation,
				.pos_lambda = jd.pos_lambda,
				.pos_penalty = jd.pos_penalty,
				.ang_lambda = jd.ang_lambda,
				.ang_penalty = jd.ang_penalty,
				.limit_lambda = jd.limit_lambda,
				.limit_penalty = jd.limit_penalty,
			});
		}

		d.vbd_solver.solve(sub_dt);

		{
			std::uint32_t ji = 0;
			const auto& solved_joints = d.vbd_solver.graph().joint_constraints();
			for (auto& jd : d.joints) {
				const auto it_a = id_to_body_index.find(jd.entity_a);
				const auto it_b = id_to_body_index.find(jd.entity_b);
				if (it_a == id_to_body_index.end() || it_b == id_to_body_index.end()) {
					continue;
				}
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
		d.vbd_solver.end_frame(result_bodies, d.contact_cache);

		const auto result_motion_ids = motion.owner_ids();
		const bool result_transform_order_matches = transform.size() == motion.size() && std::ranges::equal(result_motion_ids, transform.owner_ids());
		for (std::size_t i = 0; i < motion.size(); ++i) {
			auto& mc = motion[i];
			const auto eid = result_motion_ids[i];
			auto* tc = result_transform_order_matches ? std::addressof(transform[i]) : transform.find(eid);
			if (!tc) {
				continue;
			}
			const auto& bs = result_bodies[i];
			const auto* dyn = std::get_if<dynamic_body>(&mc.body);
			if (!dyn) {
				continue;
			}

			tc->position = bs.position;
			mc.current_velocity = bs.velocity;
			if (dyn->update_orientation) {
				tc->orientation = bs.orientation;
				mc.angular_velocity = bs.angular_velocity;
			}

			d.sleep_counters[eid] = bs.sleep_counter;
			if (auto* st = status.find(eid)) {
				st->sleeping = bs.sleeping();
			}
		}
	}
}

auto gse::physics::system::frame(frame_context& ctx, const gpu::context::data* gpu_s, data& d) -> async::task<> {
	if (!gpu_s || !d.use_gpu_solver) {
		co_return;
	}
	if (!d.gpu_solver.compute_initialized()) {
		co_return;
	}

	d.gpu_solver.stage_readback();

	if (d.gpu_solver.has_readback_data()) {
		if (d.in_flight.has_value()) {
			gpu_readback_result result;
			result.entity_ids = std::move(d.in_flight->entity_ids);
			result.gpu_input_bodies = std::move(d.in_flight->gpu_input_bodies);
			result.gpu_result_bodies = result.gpu_input_bodies;
			result.gpu_joint_count = d.in_flight->gpu_joint_count;
			std::vector<vbd::joint_constraint> joint_readback(result.gpu_joint_count);
			d.gpu_solver.readback(result.gpu_result_bodies, result.gpu_contacts, joint_readback);
			result.gpu_joint_readback = std::move(joint_readback);
			d.in_flight.reset();
			ctx.channels.push<gpu_readback_result>(std::move(result));
		}
		else {
			thread_local std::vector<vbd::body_state> discard_bodies;
			thread_local std::vector<vbd::contact_constraint> discard_contacts;
			thread_local std::vector<vbd::joint_constraint> discard_joints;
			discard_bodies.clear();
			discard_contacts.clear();
			discard_joints.clear();
			d.gpu_solver.readback(discard_bodies, discard_contacts, discard_joints);
		}
	}

	if (const auto& uploads = ctx.read_channel<gpu_upload_payload>(); !uploads.empty()) {
		const auto& [bodies, motors, joints, warm_starts, solver_cfg, dt, steps, entity_ids, joint_count] = uploads[0];

		d.gpu_solver.upload(
			bodies,
			motors,
			joints,
			warm_starts,
			solver_cfg,
			dt,
			steps
		);

		d.in_flight = data::readback_frame{
			.entity_ids = entity_ids,
			.gpu_input_bodies = bodies,
			.gpu_joint_count = joint_count
		};
	}

	if (d.gpu_solver.pending_dispatch() && d.gpu_solver.ready_to_dispatch()) {
		d.gpu_solver.commit_upload();
		d.gpu_solver.dispatch_compute();
	}

	ctx.channels.push<gpu_solver_stats>({
		.active = true,
		.contact_count = d.gpu_solver.contact_count(),
		.motor_count = d.gpu_solver.motor_count(),
		.solve_time = d.gpu_solver.solve_time(),
	});

	const auto snap_slot = d.gpu_solver.latest_snapshot_slot();

	ctx.channels.push<gpu_solver_frame_info>({
		.snapshot = &d.gpu_solver.snapshot_buffer(snap_slot),
		.semaphore = d.gpu_solver.compute_semaphore(),
		.body_count = d.gpu_solver.body_count(),
		.body_stride = sizeof(vbd::body_state),
		.position_offset = std::meta::offset_of_v<^^vbd::body_state::position>
	});
}
