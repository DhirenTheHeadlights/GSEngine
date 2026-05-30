module gse.physics;

import std;

import :system;
import :joint_spec;
import :narrow_phase_collision;
import :motion_component;
import :muscle_component;
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

namespace gse::physics {
	auto make_joint_definition(
		id a,
		id b,
		const joint_config& config
	) -> joint_definition;

	auto clear_runtime_state(
		system::data& d
	) -> void;
}

auto gse::physics::make_joint_definition(const id a, const id b, const joint_config& config) -> joint_definition {
	joint_definition result;
	gse::match(config)
		.if_is([&](const fixed_joint& cfg) {
			result = {
				.entity_a = a,
				.entity_b = b,
				.type = vbd::joint_type::fixed,
				.local_anchor_a = cfg.anchor_a,
				.local_anchor_b = cfg.anchor_b,
			};
		})
		.else_if_is([&](const distance_joint& cfg) {
			result = {
				.entity_a = a,
				.entity_b = b,
				.type = vbd::joint_type::distance,
				.target_distance = cfg.target,
			};
		})
		.else_if_is([&](const hinge_joint& cfg) {
			result = {
				.entity_a = a,
				.entity_b = b,
				.type = vbd::joint_type::hinge,
				.local_anchor_a = cfg.anchor_a,
				.local_anchor_b = cfg.anchor_b,
				.local_axis_a = cfg.axis,
				.local_axis_b = cfg.axis,
				.limit_lower = cfg.limits ? cfg.limits->first : radians(-std::numbers::pi_v<float>),
				.limit_upper = cfg.limits ? cfg.limits->second : radians(std::numbers::pi_v<float>),
				.limits_enabled = cfg.limits.has_value(),
			};
		})
		.else_if_is([&](const slider_joint& cfg) {
			result = {
				.entity_a = a,
				.entity_b = b,
				.type = vbd::joint_type::slider,
				.local_axis_a = cfg.axis,
				.local_axis_b = cfg.axis,
			};
		})
		.else_if_is([&](const spring_joint& cfg) {
			result = {
				.entity_a = a,
				.entity_b = b,
				.type = vbd::joint_type::distance,
				.target_distance = cfg.target,
				.compliance = cfg.compliance,
				.damping = cfg.damping,
			};
		})
		.else_if_is([&](const muscle_joint& cfg) {
			result = {
				.entity_a = a,
				.entity_b = b,
				.type = vbd::joint_type::muscle,
				.local_anchor_a = cfg.anchor_a,
				.local_anchor_b = cfg.anchor_b,
				.target_distance = cfg.rest_length,
				.compliance = cfg.compliance,
				.damping = cfg.damping,
				.max_force = cfg.max_force,
			};
		})
		.else_if_is([&](const ball_joint& cfg) {
			result = {
				.entity_a = a,
				.entity_b = b,
				.type = vbd::joint_type::ball,
				.local_anchor_a = cfg.anchor_a,
				.local_anchor_b = cfg.anchor_b,
				.soft_ang_stiffness = cfg.rest_stiffness,
			};
		})
		.else_if_is([&](const universal_joint& cfg) {
			result = {
				.entity_a = a,
				.entity_b = b,
				.type = vbd::joint_type::universal,
				.local_anchor_a = cfg.anchor_a,
				.local_anchor_b = cfg.anchor_b,
				.local_axis_a = cfg.twist_axis,
				.local_axis_b = cfg.twist_axis,
			};
		});
	return result;
}

auto gse::physics::clear_runtime_state(system::data& d) -> void {
	d.joints.clear();
	d.contact_cache.clear();
	d.sleep_counters.clear();
	d.gpu_joints_dirty = true;
	d.gpu_uploaded_body_count = 0;
	d.gpu_uploaded_joint_count = 0;
	d.id_to_body_index.clear();
	d.joint_handles_by_entity.clear();
	d.gpu_pending_impulses.clear();
	d.body_airborne.clear();
	d.body_sleeping.clear();
	d.gpu_stats = {};
	d.vbd_solver.begin_frame(
		std::span<const vbd::body_state>{},
		d.contact_cache
	);
	d.vbd_solver.seed_previous_velocities(std::span<const vec3<velocity>>{});
}

auto gse::physics::system::create_joint(data& d, const joint_definition& def) -> joint_handle {
	const auto handle = static_cast<joint_handle>(d.joints.size());
	d.joints.push_back(def);
	d.gpu_joints_dirty = true;
	return handle;
}

auto gse::physics::system::remove_joint(data& d, const joint_handle handle) -> void {
	if (handle < d.joints.size()) {
		d.joints.erase(d.joints.begin() + handle);
		d.gpu_joints_dirty = true;
	}
}

auto gse::physics::system::query_transform(const data& d, const id entity_id) -> std::optional<transform_snapshot> {
	const auto it = d.id_to_body_index.find(entity_id);
	if (it == d.id_to_body_index.end()) {
		return std::nullopt;
	}
	const auto body = d.gpu_solver.query_body_snapshot(it->second);
	if (!body) {
		return std::nullopt;
	}
	return transform_snapshot{
		.position = body->position,
		.orientation = body->orientation,
	};
}

auto gse::physics::system::is_airborne(const data& d, const id entity_id) -> bool {
	const auto it = d.id_to_body_index.find(entity_id);
	if (it == d.id_to_body_index.end() || it->second >= d.body_airborne.size()) {
		return true;
	}
	return d.body_airborne[it->second] != 0;
}

auto gse::physics::system::is_sleeping(const data& d, const id entity_id) -> bool {
	const auto it = d.id_to_body_index.find(entity_id);
	if (it == d.id_to_body_index.end() || it->second >= d.body_sleeping.size()) {
		return false;
	}
	return d.body_sleeping[it->second] != 0;
}

auto gse::physics::system::collect_collision_objects(write<transform_component>& transform, write<collision_component>& collision) -> std::vector<collision_pair> {
	trace::scope_guard sg{ trace_id<"vbd_cpu::collect_objects">() };
	std::vector<collision_pair> objects;
	objects.reserve(collision.size());
	const auto collision_ids = collision.owner_ids();
	const bool order_matches =
		transform.size() == collision.size() && std::ranges::equal(collision_ids, transform.owner_ids());
	for (std::size_t i = 0; i < collision.size(); ++i) {
		auto& cc = collision[i];
		if (!cc.resolve_collisions) {
			continue;
		}
		const auto eid = collision_ids[i];
		const auto* tc = order_matches ? std::addressof(transform[i]) : transform.find(eid);
		if (!tc) {
			continue;
		}
		objects.push_back({
			.owner = eid,
			.box = world_aabb_of(*tc, cc),
		});
	}
	return objects;
}

auto gse::physics::system::add_scene_contacts_to_solver(vbd::solver& solver, vbd::contact_cache& contact_cache, std::vector<collision_pair>& objects, const flat_map<id, std::uint32_t>& id_to_body_index, const bool update_scene_state, write<transform_component>& transform, write<motion_component>& motion, write<collision_component>& collision, write<collision_result_component>* results, std::span<std::uint8_t> body_airborne) -> void {
	trace::scope_guard sg{ trace_id<"vbd_cpu::broad_phase">() };

	{
		trace::scope_guard sg_sort{ trace_id<"vbd_cpu::broad_phase::sort">() };
		std::ranges::sort(
			objects,
			[](const collision_pair& a, const collision_pair& b) {
				return a.box.min.x() < b.box.min.x();
			}
		);
	}

	const auto margin = solver.config().speculative_margin;
	const auto& cfg = solver.config();
	const stiffness penalty_floor = cfg.penalty_min;

	struct pending_pair_meta {
		id owner_a;
		id owner_b;
		vec3f sat_normal;
		gap separation;
	};

	struct pending_point {
		vbd::contact_constraint constraint;
		vec3<position> position_on_a;
		id owner_a;
	};

	const auto worker_count = std::max<std::size_t>(1, task::thread_count());
	std::vector<std::vector<pending_pair_meta>> per_worker_pairs(worker_count);
	std::vector<std::vector<pending_point>> per_worker_points(worker_count);

	{
		task::parallel_invoke_range(
			0,
			objects.size(),
			[&](std::size_t i) {
				const auto worker_idx = task::current_worker().value_or(0);
				auto& pairs_bucket = per_worker_pairs[worker_idx];
				auto& points_bucket = per_worker_points[worker_idx];

				const auto& obj_a = objects[i];
				const auto x_max_a = obj_a.box.max.x() + margin;

				for (std::size_t j = i + 1; j < objects.size(); ++j) {
					const auto& obj_b = objects[j];

					if (obj_b.box.min.x() - margin > x_max_a) {
						break;
					}

					if (!obj_a.box.overlaps(obj_b.box, margin)) {
						continue;
					}

					const auto owner_a = obj_a.owner;
					const auto owner_b = obj_b.owner;
					auto* transform_a = transform.find(owner_a);
					auto* transform_b = transform.find(owner_b);
					auto* collision_a = collision.find(owner_a);
					auto* collision_b = collision.find(owner_b);
					if (!transform_a || !transform_b || !collision_a || !collision_b) {
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

					auto sat_result = narrow_phase_collision::speculative_test(sd_a, sd_b, margin);
					if (!sat_result) {
						continue;
					}

					auto& sat = *sat_result;
					if (dot(sat.normal, transform_b->position - transform_a->position) < meters(0.f)) {
						sat.normal = -sat.normal;
					}

					auto manifold = narrow_phase_collision::generate_shape_manifold(sd_a, sd_b, sat.normal, sat.separation);
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
					const vec3f constraint_normal = -sat.normal;

					pairs_bucket.push_back(
						pending_pair_meta{
							.owner_a = owner_a,
							.owner_b = owner_b,
							.sat_normal = sat.normal,
							.separation = sat.separation,
						}
					);

					const auto& bs_a = solver.body_states()[body_a];
					const auto& bs_b = solver.body_states()[body_b];

					const auto restitution_of = [&](const id eid) -> float {
						const auto* m = motion.find(eid);
						if (!m) {
							return 0.f;
						}
						const auto* d = std::get_if<dynamic_body>(&m->body);
						return d ? d->restitution : 0.f;
					};
					const float pair_restitution = std::max(restitution_of(owner_a), restitution_of(owner_b));

					for (std::uint32_t pi = 0; pi < manifold.point_count; ++pi) {
						const auto& [position_on_a, position_on_b, normal, separation, feature] = manifold.points[pi];

						const vec3<lever_arm> world_r_a = position_on_a - bs_a.position;
						const vec3<lever_arm> world_r_b = position_on_b - bs_b.position;

						vec3<lever_arm> local_r_a = inverse_rotate_vector(bs_a.orientation, world_r_a);
						vec3<lever_arm> local_r_b = inverse_rotate_vector(bs_b.orientation, world_r_b);

						auto cached = contact_cache.lookup(body_a, body_b, feature);
						const vec3<gap> current_d = position_on_a - position_on_b;
						const length current_normal_gap = dot(constraint_normal, current_d) + cfg.collision_margin;
						const bool reuse_cached_normal =
							cached && (cached->lambda[0] < newtons(-1e-3f) || current_normal_gap < meters(-1e-4f));
						const bool reuse_cached_tangent = reuse_cached_normal && cached.has_value();
						const bool reuse_cached_sticking = reuse_cached_tangent && cached->sticking;

						vec3<force> init_lambda;
						vec3<stiffness> init_penalty = { penalty_floor, penalty_floor, penalty_floor };

						if (reuse_cached_normal) {
							init_penalty[0] = std::max(cached->penalty[0], penalty_floor);

							const vec3<force> cached_normal_force = cached->normal * cached->lambda[0];
							init_lambda[0] = std::min(
								dot(cached_normal_force, constraint_normal),
								force{}
							);
						}

						if (reuse_cached_tangent) {
							init_penalty[1] = std::max(cached->penalty[1], penalty_floor);
							init_penalty[2] = std::max(cached->penalty[2], penalty_floor);

							const vec3<force> cached_tangent_force =
								cached->tangent_u * cached->lambda[1] + cached->tangent_v * cached->lambda[2];

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

						points_bucket.push_back(
							pending_point{
								.constraint =
									vbd::contact_constraint{
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
									},
								.position_on_a = position_on_a,
								.owner_a = owner_a,
							}
						);
					}
				}
			},
			trace_id<"vbd_cpu::broad_phase::pair_loop">()
		);
	}

	trace::scope_guard sg_merge{ trace_id<"vbd_cpu::broad_phase::merge">() };

	if (update_scene_state) {
		auto* motion_base = motion.data();
		const auto airborne_capacity = body_airborne.size();
		for (const auto& bucket : per_worker_pairs) {
			for (const auto& m : bucket) {
				if (m.sat_normal.y() > 0.7f) {
					if (auto* mc_b = motion.find(m.owner_b)) {
						const auto idx = static_cast<std::size_t>(mc_b - motion_base);
						if (idx < airborne_capacity) {
							body_airborne[idx] = 0;
						}
					}
				}
				if (m.sat_normal.y() < -0.7f) {
					if (auto* mc_a = motion.find(m.owner_a)) {
						const auto idx = static_cast<std::size_t>(mc_a - motion_base);
						if (idx < airborne_capacity) {
							body_airborne[idx] = 0;
						}
					}
				}

				if (results) {
					if (auto* res_a = results->find(m.owner_a)) {
						res_a->colliding = true;
						res_a->collision_normal = m.sat_normal;
						res_a->penetration = -m.separation;
					}
					if (auto* res_b = results->find(m.owner_b)) {
						res_b->colliding = true;
						res_b->collision_normal = -m.sat_normal;
						res_b->penetration = -m.separation;
					}
				}
			}
		}
	}

	for (auto& bucket : per_worker_points) {
		for (auto& pp : bucket) {
			solver.add_contact_constraint(pp.constraint);

			if (update_scene_state && results) {
				if (auto* res_a = results->find(pp.owner_a)) {
					res_a->collision_points.push_back(pp.position_on_a);
				}
			}
		}
	}
}

auto gse::physics::system::run(run_context& ctx, const gpu::context::data* gpu_s, const asset::data& assets_s, data& d) -> async::task<> {
	d.vbd_solver.configure(
		vbd::solver_config{
			.iterations = static_cast<std::uint32_t>(d.solver_iterations),
			.alpha = 0.99f,
			.beta = newtons_per_meter_squared(100000.f),
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
		}
	);

	if (gpu_s) {
		co_await d.gpu_solver.initialize_compute(ctx, *gpu_s);
		d.gpu_buffers_created = d.gpu_solver.buffers_created();
	}

	while (true) {
		{
			auto [specs, muscles] = co_await ctx.acquire<write<joint_spec>, read<muscle_component>>();
			const auto spec_owners = specs.owner_ids();
			for (std::size_t i = 0; i < specs.size(); ++i) {
				auto& spec = specs[i];
				if (spec.resolved) {
					continue;
				}
				const auto handle =
					system::create_joint(
						d,
						make_joint_definition(spec.entity_a, spec.entity_b, spec.config)
					);
				d.joint_handles_by_entity[spec_owners[i]] = handle;
				spec.resolved = true;
			}

			const auto muscle_owners = muscles.owner_ids();
			for (std::size_t i = 0; i < muscles.size(); ++i) {
				const auto handle_it = d.joint_handles_by_entity.find(muscle_owners[i]);
				if (handle_it == d.joint_handles_by_entity.end()) {
					continue;
				}
				if (handle_it->second >= d.joints.size()) {
					continue;
				}
				d.joints[handle_it->second].activation = muscles[i].activation;
			}
		}

		for (const auto owner : ctx.drain_component_adds<collision_component>()) {
			ctx.add_component<collision_result_component>(owner);
		}

		if (const auto& stats_channel = ctx.read_channel<gpu_solver_stats>(); !stats_channel.empty()) {
			d.gpu_stats = stats_channel[0];
		}

		if (!d.update_phys) {
			co_await ctx.next_tick();
			continue;
		}

		if (auto solver_cfg = d.vbd_solver.config(); solver_cfg.iterations != static_cast<std::uint32_t>(d.solver_iterations) || solver_cfg.use_jacobi != d.use_jacobi || solver_cfg.jacobi_omega != d.jacobi_omega) {
			solver_cfg.iterations = static_cast<std::uint32_t>(d.solver_iterations);
			solver_cfg.use_jacobi = d.use_jacobi;
			solver_cfg.jacobi_omega = d.jacobi_omega;
			d.vbd_solver.configure(solver_cfg);
		}

		const auto const_update_time = system_clock::fixed_dt<time_t<float, seconds>>();
		const int steps = system_clock::fixed_steps_this_frame();

		{
			auto [transform, motion, motor, collision, results] = co_await ctx.acquire_with(
				write_v<transform_component>,
				write_v<motion_component>,
				read_v<motor_component>,
				write_v<collision_component>,
				write_v<collision_result_component>
			);

			const auto impulses = ctx.read_channel<impulse_request>();

			if (motion.empty() && collision.empty() && motor.empty()) {
				clear_runtime_state(d);
			}
			else if (d.use_gpu_solver) {
				trace::scope_guard sg{ trace_id<"physics::tick_gpu">() };
				update_vbd_gpu(
					steps,
					d,
					transform,
					motion,
					motor,
					collision,
					results,
					impulses,
					const_update_time,
					ctx.channels
				);
			}
			else {
				trace::scope_guard sg{ trace_id<"physics::tick_cpu">() };
				update_vbd(steps, d, transform, motion, motor, collision, results, impulses);
			}
		}

		co_await ctx.next_tick();
	}
}

auto gse::physics::system::update_vbd_gpu(const int steps, data& d, write<transform_component>& transform, write<motion_component>& motion, read<motor_component>& motor, write<collision_component>& collision, write<collision_result_component>& results, std::span<const impulse_request> impulses, const time_t<float, seconds> dt, channel_writer& channels) -> void {
	if (!d.gpu_buffers_created) {
		return;
	}

	if (steps <= 0) {
		d.gpu_pending_impulses.insert(d.gpu_pending_impulses.end(), impulses.begin(), impulses.end());
		if (d.gpu_solver.body_count() > 0) {
			channels.push<gpu_solver_frame_info>({
				.snapshot = &d.gpu_solver.snapshot_buffer(d.gpu_solver.latest_snapshot_slot()),
				.body_count = d.gpu_solver.body_count(),
				.body_stride = sizeof(vbd::body_state),
				.position_offset = std::meta::offset_of_v<^^vbd::body_state::position>
			});
		}
		return;
	}

	std::vector<impulse_request> merged_impulses;
	std::span<const impulse_request> step_impulses = impulses;
	if (!d.gpu_pending_impulses.empty()) {
		merged_impulses.reserve(d.gpu_pending_impulses.size() + impulses.size());
		merged_impulses.insert(merged_impulses.end(), d.gpu_pending_impulses.begin(), d.gpu_pending_impulses.end());
		merged_impulses.insert(merged_impulses.end(), impulses.begin(), impulses.end());
		d.gpu_pending_impulses.clear();
		step_impulses = std::span<const impulse_request>(merged_impulses.data(), merged_impulses.size());
	}

	{
		trace::scope_guard sg{ trace_id<"vbd_gpu::airborne_update">() };
		const auto grounded_bits_raw = d.gpu_solver.read_grounded();
		const auto motion_count = motion.size();
		d.body_airborne.resize(motion_count, 1);
		d.body_sleeping.resize(motion_count, 0);
		auto* airborne_data = d.body_airborne.data();
		if (grounded_bits_raw.empty()) {
			task::coarse_parallel(
				motion_count,
				512,
				[airborne_data](std::size_t i) {
					airborne_data[i] = 1;
				},
				trace_id<"vbd_gpu::airborne_update::all_airborne">()
			);
		}
		else {
			std::array<std::uint32_t, vbd::limits.max_grounded_uints> grounded_local{};
			const auto copy_count = std::min(grounded_bits_raw.size(), grounded_local.size());
			std::ranges::copy_n(
				grounded_bits_raw.begin(),
				static_cast<std::ptrdiff_t>(copy_count),
				grounded_local.begin()
			);

			const auto bit_count = std::min<std::size_t>(motion_count, vbd::limits.max_bodies);
			task::coarse_parallel(
				bit_count,
				512,
				[airborne_data, &grounded_local](std::size_t i) {
					const auto bit = (grounded_local[i / 32u] >> (i % 32u)) & 1u;
					airborne_data[i] = (bit == 0) ? std::uint8_t{ 1 } : std::uint8_t{ 0 };
				},
				trace_id<"vbd_gpu::airborne_update::bits">()
			);
			task::coarse_parallel(
				motion_count - bit_count,
				512,
				[airborne_data, bit_count](std::size_t i) {
					airborne_data[bit_count + i] = 1;
				},
				trace_id<"vbd_gpu::airborne_update::overflow">()
			);
		}
	}

	{
		trace::scope_guard sg{ trace_id<"vbd_gpu::clear_collision_results">() };
		auto* results_data = results.data();
		task::coarse_parallel(
			results.size(),
			512,
			[results_data](std::size_t i) {
				auto& res = results_data[i];
				res.colliding = false;
				res.collision_normal = {};
				res.penetration = {};
				res.collision_points.clear();
			},
			trace_id<"vbd_gpu::clear_collision_results::loop">()
		);
	}

	{
		trace::scope_guard sg{ trace_id<"vbd_gpu::impulses_cpu">() };

		for (const auto& req : step_impulses) {
			auto* mc = motion.find(req.target);
			if (!mc || !is_dynamic(*mc)) {
				continue;
			}
			d.sleep_counters[req.target] = 0;
			const auto idx = static_cast<std::size_t>(mc - motion.data());
			if (idx < d.body_airborne.size()) {
				d.body_airborne[idx] = 1;
				d.body_sleeping[idx] = 0;
			}
		}
	}

	std::vector<vbd::body_state> bodies;
	std::vector<id> entity_ids;

	std::vector<std::pair<id, std::uint32_t>> id_to_body_index_staging;

	{
		trace::scope_guard sg{ trace_id<"vbd_gpu::build_bodies">() };
		const auto body_count = motion.size();
		assert(
			body_count <= vbd::limits.max_bodies,
			"scene has {} bodies, exceeds vbd::limits.max_bodies = {}",
			body_count,
			vbd::limits.max_bodies
		);
		bodies.resize(body_count);
		entity_ids.resize(body_count);
		id_to_body_index_staging.resize(body_count);

		const auto& sleep_counters_ref = d.sleep_counters;

		const auto build_motion_ids = motion.owner_ids();
		const bool build_transform_order_matches =
			transform.size() == body_count && std::ranges::equal(build_motion_ids, transform.owner_ids());
		task::parallel_invoke_range(
			0,
			body_count,
			[&](std::size_t i) {
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
				const bool is_static_body = std::holds_alternative<static_body>(mc.body);
				const bool locked = dyn == nullptr;

				bodies[i] = {
					.position = tc->position,
					.predicted_position = tc->position,
					.inertia_target = tc->position,
					.old_position = tc->position,
					.velocity = is_static_body ? vec3<gse::velocity>{} : mc.current_velocity,
					.orientation = tc->orientation,
					.predicted_orientation = tc->orientation,
					.angular_inertia_target = tc->orientation,
					.old_orientation = tc->orientation,
					.angular_velocity = is_static_body ? vec3<gse::angular_velocity>{} : mc.angular_velocity,
					.mass = dyn ? dyn->mass : kilograms(0.f),
					.locked = locked ? 1u : 0u,
					.update_orientation = (dyn && dyn->update_orientation) ? 1u : 0u,
					.affected_by_gravity = (dyn && dyn->affected_by_gravity) ? 1u : 0u,
					.sleep_counter = sc,
					.accel_weight = 0.f,
					.restitution = dyn ? dyn->restitution : 0.f,
					.inv_inertia = inv_inertial_tensor(mc, tc->orientation),
				};
				entity_ids[i] = eid;
			},
			trace::untraced
		);

		d.id_to_body_index.assign_unsorted(std::move(id_to_body_index_staging));
	}

	{
		trace::scope_guard sg{ trace_id<"vbd_gpu::build_collision">() };
		for (auto& b : bodies) {
			b.aabb_min = vec3<position>(position(1e30f));
			b.aabb_max = vec3<position>(position(-1e30f));
		}

		const auto& id_to_body_index_ref = d.id_to_body_index;
		const auto collision_ids = collision.owner_ids();
		const bool coll_transform_order_matches =
			transform.size() == collision.size() && std::ranges::equal(collision_ids, transform.owner_ids());
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
				gse::bounding_box bb;
				gse::match(cc.shape)
					.if_is([&](const box_shape& s) {
						bb = gse::bounding_box(body_tc, s);
					})
					.else_if_is([&](const sphere_shape& s) {
						bb = gse::bounding_box(body_tc, s);
					})
					.else_if_is([&](const capsule_shape& s) {
						bb = gse::bounding_box(body_tc, s);
					});
				const auto [max, min] = bb.aabb();
				auto& b = bodies[it->second];
				b.half_extents = bb.half_extents();
				b.aabb_min = min;
				b.aabb_max = max;
			},
			trace::untraced
		);
	}

	std::vector<vbd::impulse_constraint> gpu_impulses;
	{
		trace::scope_guard sg{ trace_id<"vbd_gpu::build_impulses">() };
		assert(
			step_impulses.size() <= vbd::limits.max_impulses,
			"impulse count {} exceeds vbd::limits.max_impulses = {}",
			step_impulses.size(),
			vbd::limits.max_impulses
		);
		gpu_impulses.reserve(step_impulses.size());
		for (const auto& req : step_impulses) {
			const auto it = d.id_to_body_index.find(req.target);
			if (it == d.id_to_body_index.end()) {
				continue;
			}
			const auto* mc = motion.find(req.target);
			if (!mc || !is_dynamic(*mc)) {
				continue;
			}
			gpu_impulses.push_back(
				vbd::impulse_constraint{
					.body_index = it->second,
					.delta_velocity = req.impulse / mass_of(*mc),
				}
			);
		}
	}

	std::vector<vbd::velocity_motor_constraint> motors;
	{
		trace::scope_guard sg{ trace_id<"vbd_gpu::build_motors">() };
		assert(
			motor.size() <= vbd::limits.max_motors,
			"motor count {} exceeds vbd::limits.max_motors = {}",
			motor.size(),
			vbd::limits.max_motors
		);
		const auto motor_ids = motor.owner_ids();
		motors.reserve(motor.size());
		for (std::size_t i = 0; i < motor.size(); ++i) {
			const auto eid = motor_ids[i];
			const auto& mt = motor[i];
			const auto* mc = motion.find(eid);
			if (!mc || !is_dynamic(*mc)) {
				continue;
			}
			const auto motion_idx = static_cast<std::size_t>(mc - motion.data());
			if (mt.requires_ground_contact && motion_idx < d.body_airborne.size() && d.body_airborne[motion_idx] != 0) {
				continue;
			}
			const auto it = d.id_to_body_index.find(eid);
			if (it == d.id_to_body_index.end()) {
				continue;
			}

			const auto idx = it->second;
			if (bodies[idx].sleeping() && magnitude(mt.velocity_drive_target) > meters_per_second(.01f)) {
				bodies[idx].sleep_counter = 0;
			}

			motors.push_back(
				vbd::velocity_motor_constraint{
					.body_index = idx,
					.horizontal_only = mt.horizontal_only ? 1u : 0u,
					.target_velocity = mt.velocity_drive_target,
					.compliance = 0.5f,
					.max_force = mt.max_force,
				}
			);
		}
	}

	std::vector<vbd::joint_constraint> gpu_joints;
	{
		trace::scope_guard sg{ trace_id<"vbd_gpu::build_joints">() };
		assert(
			d.joints.size() <= vbd::limits.max_joints,
			"joint count {} exceeds vbd::limits.max_joints = {}",
			d.joints.size(),
			vbd::limits.max_joints
		);
		gpu_joints.reserve(d.joints.size());
		for (auto& jd : d.joints) {
			const auto it_a = d.id_to_body_index.find(jd.entity_a);
			const auto it_b = d.id_to_body_index.find(jd.entity_b);
			if (it_a == d.id_to_body_index.end() || it_b == d.id_to_body_index.end()) {
				continue;
			}

			if (!jd.rest_orientation_initialized && jd.type != vbd::joint_type::distance) {
				jd.rest_orientation = bodies[it_b->second].orientation * conjugate(bodies[it_a->second].orientation);
				jd.rest_orientation_initialized = true;
			}

			gpu_joints.push_back(
				vbd::joint_constraint{
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
					.soft_ang_stiffness = jd.soft_ang_stiffness,
					.activation = jd.activation,
					.max_force = jd.max_force,
				}
			);
		}
	}

	{
		trace::scope_guard sg{ trace_id<"vbd_gpu::upload">() };
		const bool refresh_joints = d.gpu_joints_dirty || d.gpu_uploaded_body_count != entity_ids.size() ||
			d.gpu_uploaded_joint_count != gpu_joints.size();

		channels.push<gpu_upload_payload>({
			.bodies = bodies,
			.motors = motors,
			.joints = gpu_joints,
			.impulses = std::move(gpu_impulses),
			.solver_cfg = d.vbd_solver.config(),
			.dt = dt * static_cast<float>(steps),
			.steps = steps * std::max(d.physics_substeps, 1),
			.refresh_joints = refresh_joints
		});

		d.gpu_joints_dirty = false;
		d.gpu_uploaded_body_count = static_cast<std::uint32_t>(entity_ids.size());
		d.gpu_uploaded_joint_count = static_cast<std::uint32_t>(gpu_joints.size());

		gpu_body_index_map body_map;
		body_map.entries.reserve(d.id_to_body_index.size());
		for (const auto& [eid, idx] : d.id_to_body_index) {
			body_map.entries.emplace_back(eid, idx);
		}
		channels.push(std::move(body_map));
	}
}

auto gse::physics::system::update_vbd(const int steps, data& d, write<transform_component>& transform, write<motion_component>& motion, read<motor_component>& motor, write<collision_component>& collision, write<collision_result_component>& results, std::span<const impulse_request> impulses) -> void {
	trace::scope_guard sg_update{ trace_id<"vbd_cpu::update">() };

	const auto const_update_time = system_clock::constant_update_time<time_t<float, seconds>>();
	const int substeps = std::max(d.physics_substeps, 1);
	const auto sub_dt = const_update_time / static_cast<float>(substeps);

	d.body_airborne.resize(motion.size(), 1);
	d.body_sleeping.resize(motion.size(), 0);

	for (const auto& req : impulses) {
		auto* mc = motion.find(req.target);
		if (!mc || !is_dynamic(*mc)) {
			continue;
		}
		mc->current_velocity += req.impulse / mass_of(*mc);
		d.sleep_counters[req.target] = 0;
		const auto idx = static_cast<std::size_t>(mc - motion.data());
		if (idx < d.body_sleeping.size()) {
			d.body_sleeping[idx] = 0;
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

	const int total_substeps = steps * substeps;
	for (int step = 0; step < total_substeps; ++step) {
		trace::scope_guard sg_step{ trace_id<"vbd_cpu::substep">() };

		std::vector<vbd::body_state> bodies;
		bodies.resize(motion.size());

		const auto step_motion_ids = motion.owner_ids();
		const bool step_transform_order_matches =
			transform.size() == motion.size() && std::ranges::equal(step_motion_ids, transform.owner_ids());

		{
			task::parallel_invoke_range(
				0,
				motion.size(),
				[&](std::size_t i) {
					motion_component& mc = motion[i];
					const auto eid = step_motion_ids[i];
					const auto* tc = step_transform_order_matches ? std::addressof(transform[i]) : transform.find(eid);
					if (!tc) {
						bodies[i] = vbd::body_state{
							.locked = 1u,
							.update_orientation = 0u,
							.affected_by_gravity = 0u,
						};
						return;
					}
					const auto sc_it = d.sleep_counters.find(eid);
					const auto sc = sc_it != d.sleep_counters.end() ? sc_it->second : 0u;

					const auto* dyn = std::get_if<dynamic_body>(&mc.body);
					const bool is_static_body = std::holds_alternative<static_body>(mc.body);
					const bool locked = dyn == nullptr;

					bodies[i] = {
						.position = tc->position,
						.predicted_position = tc->position,
						.inertia_target = tc->position,
						.old_position = tc->position,
						.velocity = is_static_body ? vec3<gse::velocity>{} : mc.current_velocity,
						.orientation = tc->orientation,
						.predicted_orientation = tc->orientation,
						.angular_inertia_target = tc->orientation,
						.old_orientation = tc->orientation,
						.angular_velocity = is_static_body ? vec3<gse::angular_velocity>{} : mc.angular_velocity,
						.mass = dyn ? dyn->mass : kilograms(0.f),
						.locked = locked ? 1u : 0u,
						.update_orientation = (dyn && dyn->update_orientation) ? 1u : 0u,
						.affected_by_gravity = (dyn && dyn->affected_by_gravity) ? 1u : 0u,
						.sleep_counter = sc,
						.restitution = dyn ? dyn->restitution : 0.f,
						.inv_inertia = inv_inertial_tensor(mc, tc->orientation),
					};
				},
				trace::untraced
			);
		}

		std::ranges::fill(
			d.body_airborne,
			std::uint8_t{ 1 }
		);

		for (collision_result_component& res : results) {
			res.colliding = false;
			res.collision_normal = {};
			res.penetration = {};
			res.collision_points.clear();
		}

		{
			trace::scope_guard sg{ trace_id<"vbd_cpu::begin_frame">() };
			d.vbd_solver.begin_frame(bodies, d.contact_cache);
		}

		auto objects = collect_collision_objects(transform, collision);
		add_scene_contacts_to_solver(
			d.vbd_solver,
			d.contact_cache,
			objects,
			id_to_body_index,
			true,
			transform,
			motion,
			collision,
			&results,
			d.body_airborne
		);

		const auto motor_step_ids = motor.owner_ids();
		for (std::size_t i = 0; i < motor.size(); ++i) {
			const auto eid = motor_step_ids[i];
			const auto& mt = motor[i];
			const auto* mc = motion.find(eid);
			if (!mc || !is_dynamic(*mc)) {
				continue;
			}
			const auto motion_idx = static_cast<std::size_t>(mc - motion.data());
			if (mt.requires_ground_contact && motion_idx < d.body_airborne.size() && d.body_airborne[motion_idx] != 0) {
				continue;
			}
			const auto it = id_to_body_index.find(eid);
			if (it == id_to_body_index.end()) {
				continue;
			}

			auto& solver_body = d.vbd_solver.body_states()[it->second];
			if (solver_body.sleeping() && magnitude(mt.velocity_drive_target) > meters_per_second(.01f)) {
				solver_body.sleep_counter = 0;
			}

			d.vbd_solver.add_motor_constraint(
				vbd::velocity_motor_constraint{
					.body_index = it->second,
					.horizontal_only = mt.horizontal_only ? 1u : 0u,
					.target_velocity = mt.velocity_drive_target,
					.compliance = 0.5f,
					.max_force = mt.max_force,
				}
			);
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

			d.vbd_solver.add_joint_constraint(
				vbd::joint_constraint{
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
					.soft_ang_stiffness = jd.soft_ang_stiffness,
					.activation = jd.activation,
					.max_force = jd.max_force,
				}
			);
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
		{
			trace::scope_guard sg{ trace_id<"vbd_cpu::end_frame">() };
			d.vbd_solver.end_frame(result_bodies, d.contact_cache);
		}

		{
			trace::scope_guard sg{ trace_id<"vbd_cpu::writeback">() };
			const auto result_motion_ids = motion.owner_ids();
			const bool result_transform_order_matches =
				transform.size() == motion.size() && std::ranges::equal(result_motion_ids, transform.owner_ids());
			for (std::size_t i = 0; i < motion.size(); ++i) {
				auto& mc = motion[i];
				const auto eid = result_motion_ids[i];
				auto* tc = result_transform_order_matches ? std::addressof(transform[i]) : transform.find(eid);
				if (!tc) {
					continue;
				}
				const auto& bs = result_bodies[i];
				const auto* dyn = std::get_if<dynamic_body>(&mc.body);
				if (is_static(mc)) {
					continue;
				}
				if (!dyn) {
					tc->position = bs.position;
					tc->orientation = bs.orientation;
					continue;
				}

				tc->position = bs.position;
				mc.current_velocity = bs.velocity;
				if (dyn->update_orientation) {
					tc->orientation = bs.orientation;
					mc.angular_velocity = bs.angular_velocity;
				}

				d.sleep_counters[eid] = bs.sleep_counter;
				if (i < d.body_sleeping.size()) {
					d.body_sleeping[i] = bs.sleeping() ? 1 : 0;
				}
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

	if (const auto& uploads = ctx.read_channel<gpu_upload_payload>(); !uploads.empty()) {
		trace::scope_guard sg{ trace_id<"physics::frame::upload">() };
		const auto& upload = uploads[0];

		d.gpu_solver.upload(
			upload.bodies,
			upload.motors,
			upload.joints,
			upload.impulses,
			upload.solver_cfg,
			upload.dt,
			upload.steps,
			upload.refresh_joints
		);
	}

	if (d.gpu_solver.pending_dispatch()) {
		{
			trace::scope_guard sg{ trace_id<"physics::frame::commit_upload">() };
			d.gpu_solver.commit_upload();
		}
		co_await d.gpu_solver.dispatch_compute(ctx);
	}

	ctx.channels.push<gpu_solver_stats>({
		.active = true,
		.motor_count = d.gpu_solver.motor_count(),
	});

	ctx.channels.push<gpu_solver_frame_info>({
		.snapshot = &d.gpu_solver.snapshot_buffer(d.gpu_solver.latest_snapshot_slot()),
		.body_count = d.gpu_solver.body_count(),
		.body_stride = sizeof(vbd::body_state),
		.position_offset = std::meta::offset_of_v<^^vbd::body_state::position>
	});
}
