module gse.physics:system_impl;

import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.gpu;
import gse.log;
import gse.math;
import gse.meta;
import gse.save;
import gse.time;
import std;

import :collision_component;
import :contact_manifold;
import :joint_drive_component;
import :joint_spec;
import :motion_component;
import :muscle_component;
import :narrow_phase_collision;
import :system;
import :vbd_constraints;
import :vbd_contact_cache;
import :vbd_gpu_solver;
import :vbd_solver;

namespace gse::physics {
	auto make_joint_definition(
		id a,
		id b,
		const joint_config& config
	) -> joint_definition;

	auto clear_runtime_state(
		data& d
	) -> void;

	auto absorb_upload_report(
		data& d,
		const gpu_upload_report& report
	) -> void;

	auto record_consumed_resets(
		data& d,
		write<transform_component>& transform,
		write<motion_component>& motion
	) -> void;

	auto trace_body_index(
		const data& d
	) -> std::uint32_t;

	auto initialize_joint_rest_orientations(
		data& d,
		write<transform_component>& transform
	) -> void;
}

auto gse::physics::make_joint_definition(const id a, const id b, const joint_config& config) -> joint_definition {
	joint_definition result;
	match(config)
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

auto gse::physics::clear_runtime_state(data& d) -> void {
	d.joints.clear();
	d.contact_cache.clear();
	d.sleep_counters.clear();
	++d.joints_generation;
	d.id_to_body_index.clear();
	d.kinematic_step_start.clear();
	d.gpu_pending_impulses.clear();
	d.gpu_reset_ticks.clear();
	d.gpu_plan.active = false;
	d.body_airborne.clear();
	d.body_sleeping.clear();
	d.vbd_solver.begin_frame(
		std::span<const vbd::body_state>{},
		std::span<const id>{},
		d.contact_cache
	);
	d.vbd_solver.seed_previous_velocities(std::span<const vec3<velocity>>{});
}

auto gse::physics::absorb_upload_report(data& d, const gpu_upload_report& report) -> void {
	d.id_to_body_index = report.body_index;
	for (const auto& [owner, rest_orientation] : report.rest_orientations) {
		if (auto* jd = d.joints.try_get(owner)) {
			jd->rest_orientation = rest_orientation;
			jd->rest_orientation_initialized = true;
		}
	}
	const auto history = d.rollback_ring.size();
	if (history == 0) {
		return;
	}
	for (const auto& recorded : report.recorded) {
		auto& entry = d.rollback_ring[recorded.tick % history];
		if (entry.step == recorded.tick) {
			entry.inputs = recorded.inputs;
		}
	}
}

auto gse::physics::record_consumed_resets(data& d, write<transform_component>& transform, write<motion_component>& motion) -> void {
	const auto consumed_tick = d.gpu_plan.first_tick + static_cast<std::uint64_t>(d.gpu_plan.total_ticks);
	const auto owners = motion.owner_ids();
	for (std::size_t i = 0; i < motion.size(); ++i) {
		if (motion[i].reset_pending != 0 && transform.find(owners[i])) {
			d.gpu_reset_ticks.try_emplace(owners[i], consumed_tick);
		}
	}
}

auto gse::physics::initialize_joint_rest_orientations(data& d, write<transform_component>& transform) -> void {
	for (auto& jd : d.joints.items()) {
		if (jd.rest_orientation_initialized || jd.type == vbd::joint_type::distance) {
			continue;
		}
		const auto* tc_a = transform.find(jd.entity_a);
		const auto* tc_b = transform.find(jd.entity_b);
		if (!tc_a || !tc_b) {
			continue;
		}
		jd.rest_orientation = tc_b->orientation * conjugate(tc_a->orientation);
		jd.rest_orientation_initialized = true;
	}
}

auto gse::physics::create_joint(data& d, const id owner, const joint_definition& def) -> void {
	d.joints.add(owner, def);
	++d.joints_generation;
}

auto gse::physics::remove_joint(data& d, const id owner) -> void {
	d.joints.remove(owner);
	++d.joints_generation;
}

auto gse::physics::query_transform(const shared_view<data> d, const id entity_id) -> std::optional<transform_snapshot> {
	const auto it = d.id_to_body_index.find(entity_id);
	if (it == d.id_to_body_index.end()) {
		return std::nullopt;
	}
	if (!gpu_solver_active(d)) {
		const auto cpu_bodies = d.vbd_solver.body_states();
		if (it->second >= cpu_bodies.size()) {
			return std::nullopt;
		}
		const auto& cpu_body = cpu_bodies[it->second];
		return transform_snapshot{
			.position = origin_from_com(cpu_body.position, cpu_body.orientation, cpu_body.com_local),
			.orientation = cpu_body.orientation,
		};
	}

	const auto body = d.gpu_solver.query_body_snapshot(it->second);
	if (!body) {
		return std::nullopt;
	}
	return transform_snapshot{
		.position = origin_from_com(body->position, body->orientation, body->com_local),
		.orientation = body->orientation,
	};
}

auto gse::physics::sim_transform(const transform_component& tc, const motion_component* mc, const interpolation_state& interpolation) -> transform_component {
	if (!mc || interpolation.readback_age_steps <= 0 || !is_dynamic(*mc)) {
		return tc;
	}

	const auto age = system_clock::fixed_dt() * static_cast<float>(interpolation.readback_age_steps);
	return interpolated_transform(tc, mc, vec3<displacement>{}, -age);
}

auto gse::physics::render_lag(const interpolation_state& interpolation) -> time_t<float, seconds> {
	if (!interpolation.advancing) {
		return {};
	}
	return system_clock::fixed_lag();
}

auto gse::physics::render_transform(const transform_component& tc, const motion_component* mc, const interpolation_state& interpolation) -> transform_component {
	return interpolated_transform(sim_transform(tc, mc, interpolation), mc, vec3<displacement>{}, render_lag(interpolation));
}

auto gse::physics::is_airborne(const shared_view<data> d, const id entity_id) -> bool {
	const auto it = d.id_to_body_index.find(entity_id);
	if (it == d.id_to_body_index.end() || it->second >= d.body_airborne.size()) {
		return true;
	}
	return d.body_airborne[it->second] != 0;
}

auto gse::physics::is_sleeping(const shared_view<data> d, const id entity_id) -> bool {
	const auto it = d.id_to_body_index.find(entity_id);
	if (it == d.id_to_body_index.end() || it->second >= d.body_sleeping.size()) {
		return false;
	}
	return d.body_sleeping[it->second] != 0;
}

auto gse::physics::capacities_from_settings(const data& d) -> vbd::vbd_capacities {
	return {
		.max_bodies = static_cast<std::uint32_t>(d.gpu_max_bodies),
		.max_contacts = static_cast<std::uint32_t>(d.gpu_max_contacts),
		.max_collision_pairs = static_cast<std::uint32_t>(d.gpu_max_collision_pairs),
		.max_joints = static_cast<std::uint32_t>(d.gpu_max_joints),
		.max_islands = static_cast<std::uint32_t>(d.gpu_max_islands),
		.max_impulses = static_cast<std::uint32_t>(d.gpu_max_impulses),
		.max_motors = static_cast<std::uint32_t>(d.gpu_max_motors),
		.grid_table_size = static_cast<std::uint32_t>(d.gpu_grid_table_size),
		.ring_max_bodies = static_cast<std::uint32_t>(d.gpu_ring_max_bodies),
		.ring_max_contacts = static_cast<std::uint32_t>(d.gpu_ring_max_contacts),
	};
}

auto gse::physics::solver_config_from_settings(const data& d) -> vbd::solver_config {
	return {
		.iterations = static_cast<std::uint32_t>(d.solver_iterations),
		.alpha = d.solver_alpha,
		.beta = d.solver_beta,
		.gamma = d.solver_gamma,
		.penalty_min = d.penalty_min,
		.penalty_max = d.penalty_max,
		.collision_margin = d.collision_margin,
		.stick_threshold = d.stick_threshold,
		.velocity_sleep_threshold = d.velocity_sleep_threshold,
		.angular_sleep_threshold = d.angular_sleep_threshold,
		.speculative_margin = d.speculative_margin,
		.speculative_margin_floor = d.speculative_margin_floor,
		.convergence_threshold_linear = d.convergence_threshold_linear,
		.convergence_threshold_angular = d.convergence_threshold_angular,
		.convergence_speed_scale = d.convergence_speed_scale,
		.max_iterations = static_cast<std::uint32_t>(d.max_solver_iterations),
		.use_jacobi = d.use_jacobi,
		.jacobi_omega = d.jacobi_omega,
		.trace_hashes = d.trace_hashes,
		.use_solve_fold = (d.gpu_solve_fold && !d.gpu_sweep_fold_bailed) ? 1u : 0u,
		.color_cap = static_cast<std::uint32_t>(d.gpu_color_cap < 0 ? d.gpu_color_cap_resolved : d.gpu_color_cap),
		.sweep_workgroups = static_cast<std::uint32_t>(d.gpu_sweep_workgroups),
		.trace_body = trace_body_index(d),
	};
}

auto gse::physics::trace_body_index(const data& d) -> std::uint32_t {
	return d.trace_body < 0 ? 0xFFFFFFFFu : static_cast<std::uint32_t>(d.trace_body);
}

auto gse::physics::gpu_solver_active(const data& d) -> bool {
	return d.use_gpu_solver && d.gpu_solver.buffers_created();
}

auto gse::physics::gpu_solver_active(const shared_view<data> d) -> bool {
	return d.use_gpu_solver && d.gpu_solver.buffers_created();
}

auto gse::physics::gpu_solver_frame_info_of(const data& d) -> gpu_solver_frame_info {
	return {
		.snapshot = &d.gpu_solver.render_body_buffer(),
		.body_count = d.gpu_solver.body_count(),
		.body_stride = sizeof(vbd::body_state),
		.position_offset = static_cast<std::uint32_t>(std::meta::offset_of(^^vbd::body_state::position).bytes),
	};
}

auto gse::physics::resolve_hull(const collision_shape& shape, const std::span<const convex_hull> hulls) -> const convex_hull* {
	const convex_hull* result = nullptr;
	match(shape).if_is([&](const hull_shape& s) {
		if (s.index < hulls.size()) {
			result = std::addressof(hulls[s.index]);
		}
	});
	return result;
}

auto gse::physics::build_mass_properties(const body_build_view& view, std::vector<mass_properties>& out) -> void {
	const auto body_count = view.motions.size();
	out.assign(body_count, mass_properties{});

	const bool collision_order_matches =
		view.collisions.size() == body_count && std::ranges::equal(view.motion_owners, view.collision_owners);

	if (collision_order_matches) {
		task::parallel_invoke_range(
			0,
			body_count,
			[&](std::size_t i) {
				out[i] = mass_properties_of(
					view.collisions[i].shape,
					mass_of(view.motions[i]),
					resolve_hull(view.collisions[i].shape, view.hulls)
				);
			},
			trace::untraced
		);
		return;
	}

	std::flat_map<id, std::uint32_t> body_by_owner;
	std::vector<std::pair<id, std::uint32_t>> staging;
	staging.reserve(body_count);
	for (std::size_t i = 0; i < body_count; ++i) {
		staging.emplace_back(view.motion_owners[i], static_cast<std::uint32_t>(i));
	}
	body_by_owner.insert(staging.begin(), staging.end());

	for (std::size_t j = 0; j < view.collisions.size(); ++j) {
		const auto it = body_by_owner.find(view.collision_owners[j]);
		if (it == body_by_owner.end()) {
			continue;
		}
		out[it->second] = mass_properties_of(
			view.collisions[j].shape,
			mass_of(view.motions[it->second]),
			resolve_hull(view.collisions[j].shape, view.hulls)
		);
	}
}

auto gse::physics::build_body_states(const body_build_view& view, const std::unordered_map<id, std::uint32_t>& sleep_counters, std::vector<vbd::body_state>& bodies, std::flat_map<id, std::uint32_t>& id_to_body_index, std::vector<std::uint8_t>& has_transform) -> void {
	const auto body_count = view.motions.size();
	assert(
		view.mass_props.size() == body_count,
		"body_build_view carries {} mass properties for {} bodies; call build_mass_properties first",
		view.mass_props.size(),
		body_count
	);

	bodies.assign(body_count, vbd::body_state{});
	has_transform.assign(body_count, 0);

	std::vector<std::pair<id, std::uint32_t>> id_to_body_index_staging(body_count);

	const bool transform_order_matches =
		view.transforms.size() == body_count && std::ranges::equal(view.motion_owners, view.transform_owners);

	std::flat_map<id, std::uint32_t> transform_by_owner;
	if (!transform_order_matches) {
		std::vector<std::pair<id, std::uint32_t>> staging;
		staging.reserve(view.transform_owners.size());
		for (std::size_t i = 0; i < view.transform_owners.size(); ++i) {
			staging.emplace_back(view.transform_owners[i], static_cast<std::uint32_t>(i));
		}
		transform_by_owner.insert(staging.begin(), staging.end());
	}

	task::parallel_invoke_range(
		0,
		body_count,
		[&](std::size_t i) {
			const auto& mc = view.motions[i];
			const auto eid = view.motion_owners[i];
			const transform_component* tc = nullptr;
			if (transform_order_matches) {
				tc = std::addressof(view.transforms[i]);
			}
			else if (const auto ti = transform_by_owner.find(eid); ti != transform_by_owner.end()) {
				tc = std::addressof(view.transforms[ti->second]);
			}
			if (!tc) {
				bodies[i] = {
					.locked = 1u,
					.update_orientation = 0u,
					.affected_by_gravity = 0u,
				};
				return;
			}
			id_to_body_index_staging[i] = { eid, static_cast<std::uint32_t>(i) };
			has_transform[i] = 1;

			const auto sc_it = sleep_counters.find(eid);
			const auto sc = sc_it != sleep_counters.end() ? sc_it->second : 0u;

			const auto* dyn = std::get_if<dynamic_body>(&mc.body);
			const bool is_static_body = std::holds_alternative<static_body>(mc.body);
			const bool locked = dyn == nullptr;
			const auto& props = view.mass_props[i];
			const auto com = com_from_origin(tc->position, tc->orientation, props.centroid);
			const auto inv_inertia = inv_inertial_tensor(props.inv_inertia_body, tc->orientation);

			bodies[i] = {
				.position = com,
				.predicted_position = com,
				.inertia_target = com,
				.old_position = com,
				.velocity = is_static_body ? vec3<velocity>{} : mc.current_velocity,
				.orientation = tc->orientation,
				.predicted_orientation = tc->orientation,
				.angular_inertia_target = tc->orientation,
				.old_orientation = tc->orientation,
				.angular_velocity = is_static_body ? vec3<angular_velocity>{} : mc.angular_velocity,
				.mass = dyn ? dyn->mass : kilograms(0.f),
				.locked = locked ? 1u : 0u,
				.update_orientation = (dyn && dyn->update_orientation && is_rotatable(inv_inertia)) ? 1u : 0u,
				.affected_by_gravity = (dyn && dyn->affected_by_gravity) ? 1u : 0u,
				.sleep_counter = sc,
				.accel_weight = 0.f,
				.restitution = dyn ? dyn->restitution : 0.f,
				.inv_inertia = inv_inertia,
				.com_local = props.centroid,
				.reset_pending = mc.reset_pending,
			};
		},
		trace::untraced
	);

	std::vector<std::pair<id, std::uint32_t>> id_to_body_index_entries;
	id_to_body_index_entries.reserve(body_count);
	for (std::size_t i = 0; i < body_count; ++i) {
		if (has_transform[i] != 0) {
			id_to_body_index_entries.push_back(id_to_body_index_staging[i]);
		}
	}

	id_to_body_index.clear();
	id_to_body_index.insert(id_to_body_index_entries.begin(), id_to_body_index_entries.end());
}

auto gse::physics::build_body_bounds(const body_build_view& view, const std::flat_map<id, std::uint32_t>& id_to_body_index, const std::span<const std::uint8_t> has_transform, const std::span<vbd::body_state> bodies) -> void {
	for (auto& b : bodies) {
		b.aabb_min = vec3<position>(position(1e30f));
		b.aabb_max = vec3<position>(position(-1e30f));
	}

	task::parallel_invoke_range(
		0,
		view.collisions.size(),
		[&](std::size_t i) {
			const auto& cc = view.collisions[i];
			if (!cc.resolve_collisions) {
				return;
			}
			const auto it = id_to_body_index.find(view.collision_owners[i]);
			if (it == id_to_body_index.end() || has_transform[it->second] == 0) {
				return;
			}

			auto& b = bodies[it->second];
			const transform_component body_tc{
				.position = origin_from_com(b.position, b.orientation, b.com_local),
				.orientation = b.orientation,
			};
			bounding_box bb;
			vec3<displacement> shape_params;
			match(cc.shape)
				.if_is([&](const box_shape& s) {
					bb = bounding_box(body_tc, s);
				})
				.else_if_is([&](const sphere_shape& s) {
					bb = bounding_box(body_tc, s);
					shape_params = vec3<displacement>(s.radius, displacement{}, displacement{});
				})
				.else_if_is([&](const capsule_shape& s) {
					bb = bounding_box(body_tc, s);
					shape_params = vec3<displacement>(s.radius, s.half_height, displacement{});
				})
				.else_if_is([&](const hull_shape&) {
					if (const auto* hull = resolve_hull(cc.shape, view.hulls)) {
						bb = bounding_box(body_tc, *hull);
					}
				});
			const auto [max, min] = bb.aabb();
			b.shape_kind = static_cast<std::uint32_t>(cc.shape.index());
			b.shape_params = shape_params;
			b.half_extents = bb.half_extents();
			b.aabb_min = min;
			b.aabb_max = max;
		},
		trace::untraced
	);
}

auto gse::physics::build_joint_constraints(const std::span<joint_definition> definitions, const std::flat_map<id, std::uint32_t>& id_to_body_index, const std::span<const vbd::body_state> bodies, std::vector<vbd::joint_constraint>& out) -> void {
	constexpr auto unresolved = std::numeric_limits<std::uint32_t>::max();
	const auto count = definitions.size();
	auto* definition_data = definitions.data();

	std::vector<std::pair<std::uint32_t, std::uint32_t>> resolved(count, { unresolved, unresolved });
	auto* resolved_data = resolved.data();

	task::coarse_parallel(
		count,
		64,
		[definition_data, resolved_data, &id_to_body_index, bodies](const std::size_t i) {
			auto& jd = definition_data[i];
			const auto it_a = id_to_body_index.find(jd.entity_a);
			const auto it_b = id_to_body_index.find(jd.entity_b);
			if (it_a == id_to_body_index.end() || it_b == id_to_body_index.end()) {
				return;
			}

			if (!jd.rest_orientation_initialized && jd.type != vbd::joint_type::distance) {
				jd.rest_orientation = bodies[it_b->second].orientation * conjugate(bodies[it_a->second].orientation);
				jd.rest_orientation_initialized = true;
			}

			resolved_data[i] = { it_a->second, it_b->second };
		},
		trace_id<"physics::resolve_joint_bodies">()
	);

	std::vector<std::uint32_t> slots(count);
	auto* slot_data = slots.data();
	std::uint32_t kept = 0;
	for (std::size_t i = 0; i < count; ++i) {
		slot_data[i] = kept;
		if (resolved_data[i].first != unresolved) {
			++kept;
		}
	}

	out.resize(kept);
	auto* out_data = out.data();

	task::coarse_parallel(
		count,
		64,
		[definition_data, resolved_data, slot_data, out_data, unresolved](const std::size_t i) {
			const auto [body_index_a, body_index_b] = resolved_data[i];
			if (body_index_a == unresolved) {
				return;
			}

			const auto& jd = definition_data[i];
			out_data[slot_data[i]] = {
				.body_a = body_index_a,
				.body_b = body_index_b,
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
				.drive_target = jd.drive_target,
				.drive_stiffness = jd.drive_stiffness,
				.drive_damping = jd.drive_damping,
				.drive_max_torque = jd.drive_max_torque,
			};
		},
		trace_id<"physics::write_joint_constraints">()
	);
}

auto gse::physics::apply_joint_drive(joint_definition& jd, const joint_drive_component& drive) -> void {
	if (!drive.enabled) {
		jd.drive_stiffness = {};
		return;
	}
	jd.drive_target = drive.target;
	jd.drive_stiffness = drive.stiffness;
	jd.drive_damping = drive.damping;
	jd.drive_max_torque = drive.max_torque;
}

auto gse::physics::apply_muscle_activation(joint_definition& jd, const muscle_component& muscle) -> bool {
	if (jd.activation == muscle.activation) {
		return false;
	}
	jd.activation = muscle.activation;
	return true;
}

auto gse::physics::build_motor_constraints(const std::span<const motor_input> motors, const std::flat_map<id, std::uint32_t>& id_to_body_index, const std::span<const std::uint8_t> body_airborne, const std::span<vbd::body_state> bodies, std::vector<vbd::velocity_motor_constraint>& out) -> void {
	out.clear();
	out.reserve(motors.size());

	for (const auto& [eid, mt] : motors) {
		const auto it = id_to_body_index.find(eid);
		if (it == id_to_body_index.end()) {
			continue;
		}

		const auto idx = it->second;
		if (bodies[idx].locked != 0u) {
			continue;
		}
		if (mt.requires_ground_contact && idx < body_airborne.size() && body_airborne[idx] != 0) {
			continue;
		}

		if (bodies[idx].sleeping() && magnitude(mt.velocity_drive_target) > meters_per_second(.01f)) {
			bodies[idx].sleep_counter = 0;
		}

		out.push_back({
			.body_index = idx,
			.horizontal_only = mt.horizontal_only ? 1u : 0u,
			.target_velocity = mt.velocity_drive_target,
			.compliance = mt.compliance,
			.max_force = mt.max_force,
		});
	}
}

auto gse::physics::collect_collision_objects(write<transform_component>& transform, write<collision_component>& collision, write<motion_component>& motion, const std::flat_map<id, std::uint32_t>& id_to_body_index, const std::span<const convex_hull> hulls) -> std::vector<collision_pair> {
	trace::scope_guard _{ trace_id<"vbd_cpu::collect_objects">() };
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
		const auto body_it = id_to_body_index.find(eid);
		if (body_it == id_to_body_index.end()) {
			continue;
		}
		const auto* mc = motion.find(eid);
		const auto* dyn = mc ? std::get_if<dynamic_body>(&mc->body) : nullptr;
		const auto* hull = resolve_hull(cc.shape, hulls);
		objects.push_back({
			.owner = eid,
			.box = world_aabb_of(*tc, cc, hull),
			.tc = tc,
			.cc = &cc,
			.hull = hull,
			.body_index = body_it->second,
			.restitution = dyn ? dyn->restitution : 0.f,
		});
	}
	return objects;
}

auto gse::physics::build_pair_set(std::vector<collision_pair>& objects, const std::flat_set<std::pair<std::uint64_t, std::uint64_t>>& jointed_pairs, const gap sweep_margin, const std::size_t chunks_per_worker) -> std::vector<candidate_pair> {
	trace::scope_guard _{ trace_id<"vbd_cpu::broad_phase::enumerate">() };

	{
		trace::scope_guard _{ trace_id<"vbd_cpu::broad_phase::sort">() };
		std::ranges::sort(
			objects,
			[](const collision_pair& a, const collision_pair& b) {
				if (a.box.min.x() != b.box.min.x()) {
					return a.box.min.x() < b.box.min.x();
				}
				return a.owner.number() < b.owner.number();
			}
		);
	}

	const auto worker_count = std::max<std::size_t>(1, task::thread_count());
	const std::size_t target_chunks = std::max<std::size_t>(1, worker_count * chunks_per_worker);
	const std::size_t chunk_size = std::max<std::size_t>(1, (objects.size() + target_chunks - 1) / target_chunks);
	const std::size_t chunk_count = (objects.size() + chunk_size - 1) / chunk_size;
	std::vector<std::vector<candidate_pair>> per_chunk_candidates(chunk_count);

	task::parallel_invoke_range(
		0,
		chunk_count,
		[&](std::size_t c) {
			auto& bucket = per_chunk_candidates[c];

			const std::size_t chunk_begin = c * chunk_size;
			const std::size_t chunk_end = std::min(chunk_begin + chunk_size, objects.size());
			for (std::size_t i = chunk_begin; i < chunk_end; ++i) {
				const auto& obj_a = objects[i];
				const auto x_max_a = obj_a.box.max.x() + sweep_margin;

				for (std::size_t j = i + 1; j < objects.size(); ++j) {
					const auto& obj_b = objects[j];

					if (obj_b.box.min.x() - sweep_margin > x_max_a) {
						break;
					}

					if (!obj_a.box.overlaps(obj_b.box, sweep_margin)) {
						continue;
					}

					const auto a_number = obj_a.owner.number();
					const auto b_number = obj_b.owner.number();
					const auto pair_key = a_number < b_number
						? std::pair(a_number, b_number)
						: std::pair(b_number, a_number);
					if (jointed_pairs.contains(pair_key)) {
						continue;
					}

					bucket.push_back({
						.a = static_cast<std::uint32_t>(i),
						.b = static_cast<std::uint32_t>(j),
					});
				}
			}
		},
		trace_id<"vbd_cpu::broad_phase::pair_scan">(),
		chunks_per_worker
	);

	std::size_t candidate_count = 0;
	for (const auto& bucket : per_chunk_candidates) {
		candidate_count += bucket.size();
	}
	std::vector<candidate_pair> candidates;
	candidates.reserve(candidate_count);
	for (const auto& bucket : per_chunk_candidates) {
		candidates.insert(candidates.end(), bucket.begin(), bucket.end());
	}
	return candidates;
}

auto gse::physics::pair_set_valid(std::span<const collision_pair> objects, const length max_travel) -> bool {
	trace::scope_guard _{ trace_id<"vbd_cpu::broad_phase::validate">() };
	for (const auto& obj : objects) {
		const auto current = world_aabb_of(*obj.tc, *obj.cc, obj.hull);
		if (current.min.x() < obj.box.min.x() - max_travel || current.min.y() < obj.box.min.y() - max_travel || current.min.z() < obj.box.min.z() - max_travel || current.max.x() > obj.box.max.x() + max_travel || current.max.y() > obj.box.max.y() + max_travel || current.max.z() > obj.box.max.z() + max_travel) {
			return false;
		}
	}
	return true;
}

auto gse::physics::add_scene_contacts_to_solver(vbd::solver& solver, vbd::contact_cache& contact_cache, std::span<const collision_pair> objects, std::span<const candidate_pair> candidates, const vbd::time_step sub_dt, write<collision_result_component>& results, std::span<std::uint8_t> body_airborne, const std::size_t chunks_per_worker) -> void {
	trace::scope_guard _{ trace_id<"vbd_cpu::broad_phase">() };

	const auto& cfg = solver.config();
	const stiffness penalty_floor = cfg.penalty_min;

	struct pending_pair_meta {
		id owner_a;
		id owner_b;
		std::uint32_t body_a;
		std::uint32_t body_b;
		vec3f sat_normal;
		gap separation;
	};

	struct pending_point {
		vbd::contact_constraint constraint;
		vec3<position> position_on_a;
		id owner_a;
	};

	const auto worker_count = std::max<std::size_t>(1, task::thread_count());
	const std::size_t target_chunks = std::max<std::size_t>(1, worker_count * chunks_per_worker);
	const std::size_t chunk_size = std::max<std::size_t>(1, (candidates.size() + target_chunks - 1) / target_chunks);
	const std::size_t chunk_count = (candidates.size() + chunk_size - 1) / chunk_size;
	std::vector<std::vector<pending_pair_meta>> per_chunk_pairs(chunk_count);
	std::vector<std::vector<pending_point>> per_chunk_points(chunk_count);

	{
		task::parallel_invoke_range(
			0,
			chunk_count,
			[&](std::size_t c) {
				auto& pairs_bucket = per_chunk_pairs[c];
				auto& points_bucket = per_chunk_points[c];

				const std::size_t chunk_begin = c * chunk_size;
				const std::size_t chunk_end = std::min(chunk_begin + chunk_size, candidates.size());
				for (std::size_t ci = chunk_begin; ci < chunk_end; ++ci) {
					const auto& obj_a = objects[candidates[ci].a];
					const auto& obj_b = objects[candidates[ci].b];

					const auto owner_a = obj_a.owner;
					const auto owner_b = obj_b.owner;

					const std::uint32_t body_a = obj_a.body_index;
					const std::uint32_t body_b = obj_b.body_index;
					const auto& bs_a = solver.body_states()[body_a];
					const auto& bs_b = solver.body_states()[body_b];

					if ((bs_a.locked || bs_a.sleeping()) && (bs_b.locked || bs_b.sleeping())) {
						const bool a_is_lo = owner_a.number() <= owner_b.number();
						const auto lo_owner = a_is_lo ? owner_a : owner_b;
						const auto hi_owner = a_is_lo ? owner_b : owner_a;
						const std::uint32_t lo_index = a_is_lo ? body_a : body_b;
						const std::uint32_t hi_index = a_is_lo ? body_b : body_a;
						const auto& bs_lo = a_is_lo ? bs_a : bs_b;
						const auto& bs_hi = a_is_lo ? bs_b : bs_a;

						bool meta_pushed = false;
						const std::size_t replayed = contact_cache.for_each_pair(owner_a, owner_b, [&](const std::uint64_t feature, const vbd::cached_lambda& payload) {
							const vec3<position> point_on_lo = bs_lo.position + rotate_vector(bs_lo.orientation, payload.local_anchor_a);
							if (!meta_pushed) {
								meta_pushed = true;
								const vec3<position> point_on_hi = bs_hi.position + rotate_vector(bs_hi.orientation, payload.local_anchor_b);
								pairs_bucket.push_back(
									pending_pair_meta{
										.owner_a = lo_owner,
										.owner_b = hi_owner,
										.body_a = lo_index,
										.body_b = hi_index,
										.sat_normal = -payload.normal,
										.separation = dot(-payload.normal, point_on_hi - point_on_lo),
									}
								);
							}
							points_bucket.push_back(
								pending_point{
									.constraint =
										vbd::contact_constraint{
											.body_a = lo_index,
											.body_b = hi_index,
											.feature_key = feature,
											.sticking = payload.sticking ? 1u : 0u,
											.normal = payload.normal,
											.tangent_u = payload.tangent_u,
											.tangent_v = payload.tangent_v,
											.local_anchor_a = payload.local_anchor_a,
											.local_anchor_b = payload.local_anchor_b,
											.friction_coeff = cfg.friction_coefficient,
											.penalty_floor = penalty_floor,
											.lambda = payload.lambda,
											.penalty = payload.penalty,
											.replayed = 1u,
										},
									.position_on_a = point_on_lo,
									.owner_a = lo_owner,
								}
							);
						});
						if (replayed > 0) {
							continue;
						}
					}

					const narrow_phase_collision::shape_data sd_a{
						.tc = obj_a.tc,
						.shape = &obj_a.cc->shape,
						.hull = obj_a.hull,
						.body_index = obj_a.body_index,
					};
					const narrow_phase_collision::shape_data sd_b{
						.tc = obj_b.tc,
						.shape = &obj_b.cc->shape,
						.hull = obj_b.hull,
						.body_index = obj_b.body_index,
					};

					const auto relative_velocity = bs_a.velocity - bs_b.velocity;
					const auto center_delta = bs_b.position - bs_a.position;
					const auto center_distance = magnitude(center_delta);
					const velocity closing_speed = center_distance > meters(1e-6f) ? std::max<velocity>(dot(relative_velocity, center_delta) / center_distance, velocity{}) : magnitude(relative_velocity);
					const auto pair_margin = std::clamp<gap>(closing_speed * sub_dt * 2.f, cfg.speculative_margin_floor, cfg.speculative_margin);
					auto sat_result = narrow_phase_collision::speculative_test(sd_a, sd_b, pair_margin);
					if (!sat_result) {
						continue;
					}

					auto& sat = *sat_result;
					if (dot(sat.normal, obj_b.tc->position - obj_a.tc->position) < meters(0.f)) {
						sat.normal = -sat.normal;
					}

					auto manifold = narrow_phase_collision::generate_shape_manifold(sd_a, sd_b, sat.normal, sat.separation);
					if (manifold.point_count == 0) {
						continue;
					}

					const vec3f constraint_normal = -sat.normal;

					pairs_bucket.push_back(
						pending_pair_meta{
							.owner_a = owner_a,
							.owner_b = owner_b,
							.body_a = body_a,
							.body_b = body_b,
							.sat_normal = sat.normal,
							.separation = sat.separation,
						}
					);

					const float pair_restitution = std::max(obj_a.restitution, obj_b.restitution);

					for (std::uint32_t pi = 0; pi < manifold.point_count; ++pi) {
						const auto& [position_on_a, position_on_b, normal, separation, feature] = manifold.points[pi];

						const vec3<lever_arm> world_r_a = position_on_a - bs_a.position;
						const vec3<lever_arm> world_r_b = position_on_b - bs_b.position;

						vec3<lever_arm> local_r_a = inverse_rotate_vector(bs_a.orientation, world_r_a);
						vec3<lever_arm> local_r_b = inverse_rotate_vector(bs_b.orientation, world_r_b);

						auto cached = contact_cache.lookup(owner_a, owner_b, feature);
						const vec3<gap> current_d = position_on_a - position_on_b;
						const length current_normal_gap = dot(constraint_normal, current_d) + cfg.collision_margin;
						const bool reuse_cached_normal =
							cached && (cached->lambda[0] < newtons(-1e-3f) || current_normal_gap < meters(-1e-4f));
						const bool reuse_cached_tangent = reuse_cached_normal && cached.has_value();
						const bool reuse_cached_sticking = reuse_cached_tangent && cached->sticking;
						const bool restore_penalty =
							cached && (cached->lambda[0] < newtons(-1e-3f) || current_normal_gap < cfg.stick_threshold);

						vec3<force> init_lambda;
						vec3<stiffness> init_penalty = { penalty_floor, penalty_floor, penalty_floor };

						if (restore_penalty) {
							init_penalty[0] = std::max(cached->penalty[0], penalty_floor);
							init_penalty[1] = std::max(cached->penalty[1], penalty_floor);
							init_penalty[2] = std::max(cached->penalty[2], penalty_floor);
						}

						if (reuse_cached_normal) {
							const vec3<force> cached_normal_force = cached->normal * cached->lambda[0];
							init_lambda[0] = std::min(
								dot(cached_normal_force, constraint_normal),
								force{}
							);
						}

						if (reuse_cached_tangent) {
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
										.replayed = cached ? 0u : 2u,
									},
								.position_on_a = position_on_a,
								.owner_a = owner_a,
							}
						);
					}
				}
			},
			trace_id<"vbd_cpu::broad_phase::pair_loop">(),
			chunks_per_worker
		);
	}

	trace::scope_guard _{ trace_id<"vbd_cpu::broad_phase::emit">() };

	const auto airborne_capacity = body_airborne.size();

	for (const auto& m : per_chunk_pairs | std::views::join) {
		if (m.sat_normal.y() > 0.7f && m.body_b < airborne_capacity) {
			body_airborne[m.body_b] = 0;
		}
		if (m.sat_normal.y() < -0.7f && m.body_a < airborne_capacity) {
			body_airborne[m.body_a] = 0;
		}

		if (auto* res_a = results.find(m.owner_a)) {
			res_a->colliding = true;
			res_a->collision_normal = m.sat_normal;
			res_a->penetration = -m.separation;
		}
		if (auto* res_b = results.find(m.owner_b)) {
			res_b->colliding = true;
			res_b->collision_normal = -m.sat_normal;
			res_b->penetration = -m.separation;
		}
	}

	for (const auto& pp : per_chunk_points | std::views::join) {
		solver.add_contact_constraint(pp.constraint);

		if (auto* res_a = results.find(pp.owner_a)) {
			res_a->collision_points.push_back(pp.position_on_a);
		}
	}
}

auto gse::physics::init(context& ctx, const std::optional<shared_view<gpu::context::data>> gpu_s, data& d) -> async::task<> {
	d.vbd_solver.configure(solver_config_from_settings(d));
	d.vbd_solver.set_ordered_sweep(d.use_ordered_sweep);

	if (gpu_s && d.use_gpu_solver) {
		co_await d.gpu_solver.initialize_compute(ctx, *gpu_s, capacities_from_settings(d), d.gpu_sync_readback);
	}

	co_return;
}

auto gse::physics::apply_kinematic_targets(read<kinematic_target_component>& targets, write<transform_component>& transform, write<motion_component>& motion, std::flat_map<id, transform_component>& step_start, const time_t<float, seconds> dt) -> void {
	trace::scope_guard _{ trace_id<"physics::kinematic_targets">() };

	const auto target_owners = targets.owner_ids();
	for (std::size_t i = 0; i < targets.size(); ++i) {
		const auto eid = target_owners[i];
		auto* mc = motion.find(eid);
		if (!mc || !is_kinematic(*mc)) {
			continue;
		}
		auto* tc = transform.find(eid);
		if (!tc) {
			continue;
		}

		const auto& target = targets[i];
		const auto start = step_start.try_emplace(eid, *tc).first;

		*tc = start->second;

		mc->current_velocity = (target.position - tc->position) / dt;
		mc->angular_velocity = difference_axis_angle(tc->orientation, target.orientation) / dt;

		start->second = transform_component{
			.position = target.position,
			.orientation = target.orientation,
		};
	}
}

auto gse::physics::prepare(context& ctx, data& d, const channel_write<interpolation_state> interp_out, write<joint_spec> specs, read<muscle_component> muscles, read<joint_drive_component> drives, read<kinematic_target_component> targets, write<transform_component> transform, write<motion_component> motion) -> async::task<> {
	if (const int steps = system_clock::fixed_steps_this_frame(); steps > 0 && d.update_phys) {
		int effective_steps = steps;
		if (d.gpu_async_dispatch && gpu_solver_active(d)) {
			effective_steps = d.gpu_solver.latest_dispatch_complete() ? 1 : 0;
		}
		d.interpolation.steps = effective_steps;
		if (effective_steps > 0) {
			apply_kinematic_targets(
				targets,
				transform,
				motion,
				d.kinematic_step_start,
				system_clock::fixed_dt() * static_cast<float>(effective_steps)
			);
		}
	}
	else {
		d.interpolation.steps = 0;
	}

	const auto spec_owners = specs.owner_ids();
	for (std::size_t i = 0; i < specs.size(); ++i) {
		auto& spec = specs[i];
		if (spec.resolved) {
			continue;
		}
		const auto def = make_joint_definition(spec.entity_a, spec.entity_b, spec.config);
		if (auto* existing = d.joints.try_get(spec_owners[i])) {
			*existing = def;
			++d.joints_generation;
		}
		else {
			create_joint(d, spec_owners[i], def);
		}
		spec.resolved = true;
	}

	const auto muscle_owners = muscles.owner_ids();
	for (std::size_t i = 0; i < muscles.size(); ++i) {
		auto* jd = d.joints.try_get(muscle_owners[i]);
		if (!jd) {
			continue;
		}
		if (apply_muscle_activation(*jd, muscles[i])) {
			++d.joints_generation;
		}
	}

	const auto drive_owners = drives.owner_ids();
	for (std::size_t i = 0; i < drives.size(); ++i) {
		auto* jd = d.joints.try_get(drive_owners[i]);
		if (!jd) {
			continue;
		}
		apply_joint_drive(*jd, drives[i]);
		if (drives[i].enabled) {
			++d.joints_generation;
		}
	}

	d.interpolation.advancing = d.update_phys;
	d.interpolation.readback_age_steps = gpu_solver_active(d) ? d.gpu_solver.readback_age_steps() : 0;

	interp_out.push<interpolation_state>(d.interpolation);

	if (d.update_phys) {
		d.vbd_solver.set_color_grain(static_cast<std::size_t>(std::max(d.color_chunk_grain, 1)));
		d.vbd_solver.set_ordered_sweep(d.use_ordered_sweep);
		d.vbd_solver.configure(solver_config_from_settings(d));
	}

	co_return;
}

auto gse::physics::ensure_results(write<collision_component> collision, structural<collision_result_component> results) -> async::task<> {
	for (const auto owner : collision.owner_ids()) {
		if (!results.contains(owner)) {
			results.add(owner);
		}
	}
	co_return;
}

auto gse::physics::integrate(context& ctx, data& d, const channel_read<impulse_request, reset_physics_request, rollback_request, rollback_history_request, gpu_upload_report> requests_in, const channel_write<gpu_solver_frame_info> solver_out, write<transform_component> transform, write<motion_component> motion, read<motor_component> motor, write<collision_component> collision, write<collision_result_component> results, write<hull_definition> hull_definitions) -> async::task<> {
	{
		const auto definition_owners = hull_definitions.owner_ids();
		for (std::size_t i = 0; i < hull_definitions.size(); ++i) {
			auto& definition = hull_definitions[i];
			if (definition.interned) {
				continue;
			}
			definition.interned = true;

			auto hull = build_convex_hull(definition.points);
			if (!hull.valid()) {
				log::println(
					log::category::physics,
					"hull definition on entity {} produced no valid hull from {} points",
					definition_owners[i],
					definition.points.size()
				);
				continue;
			}

			if (auto* cc = collision.find(definition_owners[i])) {
				cc->shape = hull_shape{ .index = static_cast<std::uint32_t>(d.hulls.size()) };
			}
			d.hulls.push_back(std::move(hull));
		}
	}

	if (!d.update_phys) {
		co_return;
	}

	const int steps = system_clock::fixed_steps_this_frame();

	const auto impulses = requests_in.of<impulse_request>();
	const bool reset = !requests_in.of<reset_physics_request>().empty();
	const auto rollbacks = requests_in.of<rollback_request>();
	for (const auto& req : requests_in.of<rollback_history_request>()) {
		d.rollback_history_floor = std::max(req.steps, 0);
	}
	const auto frame_inputs = gather_step_inputs(motor, transform, motion, impulses);

	if (d.use_gpu_solver && !d.gpu_solver.buffers_created() && !d.gpu_unavailable_reported) {
		d.gpu_unavailable_reported = true;
		log::println(
			log::level::error,
			log::category::physics,
			"Physics.use_gpu_solver is on but the gpu solver has no buffers, which is what setting it after startup looks like. The settings panel stages this one for the next boot; a session override or a command-line assignment does not. Stepping the cpu solver instead; restart to run on the gpu"
		);
	}

	if (motion.empty() && collision.empty() && motor.empty()) {
		clear_runtime_state(d);
	}
	else if (gpu_solver_active(d)) {
		trace::scope_guard _{ trace_id<"physics::tick_gpu">() };
		update_vbd_gpu(
			steps,
			d,
			transform,
			motion,
			results,
			frame_inputs,
			rollbacks,
			requests_in.of<gpu_upload_report>(),
			solver_out,
			reset
		);
	}
	else {
		trace::scope_guard _{ trace_id<"physics::tick_cpu">() };
		for (const auto& req : rollbacks) {
			rollback(d, req, transform, motion, collision, results);
		}
		std::vector<step_inputs> per_step(static_cast<std::size_t>(std::max(steps, 0)), frame_inputs);
		for (std::size_t s = 1; s < per_step.size(); ++s) {
			per_step[s].impulses.clear();
			per_step[s].kinematics.clear();
		}
		update_vbd(steps, d, transform, motion, collision, results, per_step);
		d.observed_step = d.step_index;
	}

	co_return;
}

auto gse::physics::update_vbd_gpu(const int steps, data& d, write<transform_component>& transform, write<motion_component>& motion, write<collision_result_component>& results, const step_inputs& inputs, const std::span<const rollback_request> rollbacks, const std::span<const gpu_upload_report> reports, const channel_write<gpu_solver_frame_info> frame_info_out, const bool reset) -> void {
	for (const auto& report : reports) {
		absorb_upload_report(d, report);
	}
	initialize_joint_rest_orientations(d, transform);

	if (reset) {
		d.sleep_counters.clear();
		d.contact_cache.clear();
		d.kinematic_step_start.clear();
		d.gpu_reset_ticks.clear();
		d.gpu_sweep_fold_bailed = false;
	}

	if (d.gpu_plan.active) {
		record_consumed_resets(d, transform, motion);
	}

	if (!reset) {
		trace::scope_guard _{ trace_id<"vbd_gpu::readback">() };
		{
			trace::scope_guard _wait{ trace_id<"vbd_gpu::rb_wait">() };
			d.gpu_solver.wait_for_latest_dispatch();
		}
		const auto solved = d.gpu_solver.read_body_states();
		const auto readback = d.gpu_solver.readback_tick();

		step_snapshot* observed = nullptr;
		if (readback && !d.rollback_ring.empty()) {
			auto& entry = d.rollback_ring[*readback % d.rollback_ring.size()];
			if (entry.step == *readback) {
				observed = &entry;
				observed->bodies.clear();
			}
			d.observed_step = *readback;
		}

		if (!solved.empty()) {
			trace::scope_guard _apply{ trace_id<"vbd_gpu::rb_apply">() };
			const auto rb_motion_ids = motion.owner_ids();
			const bool rb_transform_order_matches =
				transform.size() == motion.size() && std::ranges::equal(rb_motion_ids, transform.owner_ids());
			bool rb_index_is_identity = d.id_to_body_index.size() == motion.size();
			if (rb_index_is_identity) {
				for (const auto& [owner, index] : d.id_to_body_index) {
					if (index >= rb_motion_ids.size() || rb_motion_ids[index] != owner) {
						rb_index_is_identity = false;
						break;
					}
				}
			}
			for (std::size_t i = 0; i < motion.size(); ++i) {
				auto& mc = motion[i];
				const auto eid = rb_motion_ids[i];
				if (mc.reset_pending != 0) {
					const auto consumed = d.gpu_reset_ticks.find(eid);
					if (consumed == d.gpu_reset_ticks.end() || !readback || *readback < consumed->second) {
						continue;
					}
					mc.reset_pending = 0;
					d.gpu_reset_ticks.erase(consumed);
				}
				auto* tc = rb_transform_order_matches ? std::addressof(transform[i]) : transform.find(eid);
				if (!tc) {
					continue;
				}
				const auto* dyn = std::get_if<dynamic_body>(&mc.body);
				if (is_static(mc)) {
					continue;
				}
				auto body_index = static_cast<std::uint32_t>(i);
				if (!rb_index_is_identity) {
					const auto it = d.id_to_body_index.find(eid);
					if (it == d.id_to_body_index.end()) {
						continue;
					}
					body_index = it->second;
				}
				if (body_index >= solved.size()) {
					continue;
				}
				const auto& bs = solved[body_index];
				if (!dyn) {
					continue;
				}
				tc->position = origin_from_com(bs.position, bs.orientation, bs.com_local);
				mc.current_velocity = bs.velocity;
				if (dyn->update_orientation) {
					tc->orientation = bs.orientation;
					mc.angular_velocity = bs.angular_velocity;
				}
				d.sleep_counters[eid] = bs.sleep_counter;
				if (i < d.body_sleeping.size()) {
					d.body_sleeping[i] = bs.sleeping() ? 1 : 0;
				}
				if (observed) {
					observed->bodies.push_back({
						.owner = eid,
						.transform = *tc,
						.motion = mc,
					});
				}
			}
		}

		const auto diag = d.gpu_solver.diagnostics();
		d.gpu_solver.set_color_launch_hint(diag.max_used_color);

		constexpr int color_cap_dwell_frames = 30;
		constexpr std::uint32_t color_pop_floor = 128;
		if (d.gpu_color_cap < 0) {
			if (d.gpu_solver.reseeding()) {
				d.gpu_color_cap_min = 0;
			}
			++d.gpu_color_cap_dwell;
			if (d.gpu_color_cap_dwell >= color_cap_dwell_frames && diag.max_used_color > 0) {
				d.gpu_color_cap_dwell = 0;
				const std::uint32_t conflict_budget = std::max(8u, diag.attempted_contacts / 256u);
				int resolved = d.gpu_color_cap_resolved;
				if (resolved == 0 && diag.attempted_contacts >= 256) {
					resolved = static_cast<int>(std::max(2u, diag.max_used_color));
				}
				if (resolved > 0) {
					if (diag.coloring_conflicts > conflict_budget) {
						resolved = std::min(resolved + 1, static_cast<int>(vbd::limits.max_colors));
						d.gpu_color_cap_min = resolved;
					}
					else if (diag.coloring_conflicts * 2 <= conflict_budget && resolved > std::max(2, d.gpu_color_cap_min) && diag.color_populations[static_cast<std::size_t>(resolved) - 1] < color_pop_floor) {
						--resolved;
					}
					d.gpu_color_cap_resolved = resolved;
				}
			}
		}
		else {
			d.gpu_color_cap_resolved = 0;
			d.gpu_color_cap_dwell = 0;
			d.gpu_color_cap_min = 0;
		}

		constexpr int sweep_retry_cooldown_frames = 600;
		constexpr int sweep_retry_backoff_max = 3;
		if ((d.gpu_solve_fold && !d.gpu_solve_fold_prev) || d.gpu_solver.reseeding()) {
			d.gpu_sweep_fold_bailed = false;
			d.gpu_sweep_retry_cooldown = 0;
			d.gpu_sweep_retry_backoff = 0;
			d.gpu_sweep_calm_frames = 0;
		}
		d.gpu_solve_fold_prev = d.gpu_solve_fold;

		if (d.gpu_sweep_fold_bailed && d.gpu_sweep_retry_cooldown > 0 && --d.gpu_sweep_retry_cooldown == 0) {
			d.gpu_sweep_fold_bailed = false;
		}

		if (!d.gpu_sweep_fold_bailed && d.gpu_sweep_retry_backoff > 0 && diag.sweep_bails == 0) {
			++d.gpu_sweep_calm_frames;
			if (d.gpu_sweep_calm_frames >= sweep_retry_cooldown_frames) {
				d.gpu_sweep_retry_backoff = 0;
				d.gpu_sweep_calm_frames = 0;
			}
		}

		if (diag.sweep_bails > 0 && !d.gpu_sweep_fold_bailed) {
			d.gpu_sweep_fold_bailed = true;
			d.gpu_sweep_retry_cooldown = sweep_retry_cooldown_frames << std::min(d.gpu_sweep_retry_backoff, sweep_retry_backoff_max);
			d.gpu_sweep_retry_backoff = std::min(d.gpu_sweep_retry_backoff + 1, sweep_retry_backoff_max);
			d.gpu_sweep_calm_frames = 0;
			log::println(
				log::level::warning,
				log::category::physics,
				"vbd gpu solve sweep bailed under contention ({} starved leaders); dropping to the unfolded colour loop, retrying in {} frames",
				diag.sweep_bails,
				d.gpu_sweep_retry_cooldown
			);
		}
	}

	int dispatch_steps = steps;
	if (d.gpu_async_dispatch && !reset && steps > 0) {
		dispatch_steps = d.interpolation.steps;
	}

	std::optional<std::uint64_t> restore_tick;
	int replay_ticks = 0;
	const auto ring_history = static_cast<std::uint64_t>(d.gpu_solver.ring_history());
	const auto history = history_steps(d);
	if (history > 0 && d.rollback_ring.size() != history) {
		d.rollback_ring.assign(history, {});
	}
	for (const auto& req : rollbacks) {
		if (req.steps == 0 || ring_history == 0 || history == 0 || req.steps >= ring_history || req.steps >= history || req.steps > d.step_index) {
			log::println(
				log::level::warning,
				log::category::physics,
				"gpu rollback of {} step(s) refused: ring holds {} tick(s) and {} have been simulated",
				req.steps,
				ring_history,
				d.step_index
			);
			continue;
		}
		const auto target = d.step_index - req.steps;
		bool inputs_known = true;
		for (std::uint64_t s = target + 1; s <= d.step_index; ++s) {
			if (d.rollback_ring[s % history].step != s) {
				inputs_known = false;
				break;
			}
		}
		if (!inputs_known) {
			log::println(log::level::warning, log::category::physics, "gpu rollback to tick {} refused: the recorded inputs for the replay window are incomplete", target);
			continue;
		}
		restore_tick = target;
		replay_ticks = static_cast<int>(req.steps);
		for (const auto& body : req.body_overrides) {
			if (body.step != target) {
				continue;
			}
			if (auto* tc = transform.find(body.owner)) {
				*tc = body.transform;
			}
			if (auto* mc = motion.find(body.owner)) {
				*mc = body.motion;
				mc->reset_pending = 1;
				d.sleep_counters[body.owner] = 0;
				const auto index = static_cast<std::size_t>(mc - motion.data());
				if (index < d.body_sleeping.size()) {
					d.body_sleeping[index] = 0;
				}
			}
		}
		log::println(log::category::physics, "gpu rollback: restoring tick {} and replaying {} tick(s) with this frame's {} step(s)", *restore_tick, replay_ticks, dispatch_steps);
	}

	if (dispatch_steps <= 0 && replay_ticks == 0) {
		d.gpu_pending_impulses.insert(d.gpu_pending_impulses.end(), inputs.impulses.begin(), inputs.impulses.end());
		if (d.gpu_solver.body_count() > 0) {
			frame_info_out.push<gpu_solver_frame_info>(gpu_solver_frame_info_of(d));
		}
		d.gpu_plan.active = false;
		return;
	}

	std::vector<impulse_request> merged_impulses;
	std::span<const impulse_request> step_impulses = inputs.impulses;
	if (!d.gpu_pending_impulses.empty()) {
		merged_impulses.reserve(d.gpu_pending_impulses.size() + inputs.impulses.size());
		merged_impulses.insert(merged_impulses.end(), d.gpu_pending_impulses.begin(), d.gpu_pending_impulses.end());
		merged_impulses.insert(merged_impulses.end(), inputs.impulses.begin(), inputs.impulses.end());
		d.gpu_pending_impulses.clear();
		step_impulses = std::span<const impulse_request>(merged_impulses.data(), merged_impulses.size());
	}

	{
		trace::scope_guard _{ trace_id<"vbd_gpu::airborne_update">() };
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
			const auto& capacities = d.gpu_solver.capacities();
			d.gpu_grounded_bits.assign(capacities.max_grounded_uints, 0u);
			const auto copy_count = std::min(grounded_bits_raw.size(), d.gpu_grounded_bits.size());
			std::ranges::copy_n(
				grounded_bits_raw.begin(),
				static_cast<std::ptrdiff_t>(copy_count),
				d.gpu_grounded_bits.begin()
			);

			const auto bit_count = std::min<std::size_t>(motion_count, capacities.max_bodies);
			const auto* grounded_bits = d.gpu_grounded_bits.data();
			task::coarse_parallel(
				bit_count,
				512,
				[airborne_data, grounded_bits](std::size_t i) {
					const auto bit = (grounded_bits[i / 32u] >> (i % 32u)) & 1u;
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
		trace::scope_guard _{ trace_id<"vbd_gpu::clear_collision_results">() };
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
		trace::scope_guard _{ trace_id<"vbd_gpu::impulses_cpu">() };

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

	const int total_ticks = std::max(dispatch_steps, 0) + replay_ticks;
	const std::uint64_t first_tick = restore_tick ? *restore_tick : d.step_index;

	auto& plan = d.gpu_plan;
	plan.per_tick.clear();
	plan.per_tick.reserve(static_cast<std::size_t>(total_ticks));
	for (int t = 0; t < total_ticks; ++t) {
		const std::uint64_t tick = first_tick + static_cast<std::uint64_t>(t) + 1;
		if (t < replay_ticks) {
			plan.per_tick.push_back(d.rollback_ring[tick % history].inputs);
			continue;
		}
		step_inputs frame;
		if (t == replay_ticks) {
			frame.impulses.assign(step_impulses.begin(), step_impulses.end());
		}
		if (history > 0) {
			auto& entry = d.rollback_ring[tick % history];
			entry.step = tick;
			entry.inputs = frame;
		}
		plan.per_tick.push_back(std::move(frame));
	}
	if (dispatch_steps <= 0) {
		d.gpu_pending_impulses.assign(step_impulses.begin(), step_impulses.end());
	}

	d.step_index = first_tick + static_cast<std::uint64_t>(total_ticks);

	plan.active = true;
	plan.reset = reset;
	plan.replay_ticks = replay_ticks;
	plan.total_ticks = total_ticks;
	plan.first_tick = first_tick;
	plan.restore_tick = restore_tick;
	++plan.generation;
}

auto gse::physics::apply_step_inputs(const step_inputs& inputs, data& d, write<transform_component>& transform, write<motion_component>& motion) -> void {
	for (const auto& kinematic : inputs.kinematics) {
		auto* mc = motion.find(kinematic.owner);
		auto* tc = transform.find(kinematic.owner);
		if (!mc || !tc || !is_kinematic(*mc)) {
			continue;
		}
		*tc = kinematic.start;
		mc->current_velocity = kinematic.velocity;
		mc->angular_velocity = kinematic.angular_velocity;
	}

	for (const auto& req : inputs.impulses) {
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
}

auto gse::physics::step_all_asleep(const step_inputs& inputs, const data& d, write<motion_component>& motion) -> bool {
	for (const auto& input : inputs.motors) {
		if (magnitude(input.motor.velocity_drive_target) > meters_per_second(.01f)) {
			return false;
		}
	}

	const auto motion_ids = motion.owner_ids();
	for (std::size_t i = 0; i < motion.size(); ++i) {
		if (!is_dynamic(motion[i])) {
			continue;
		}
		const auto it = d.sleep_counters.find(motion_ids[i]);
		if (it == d.sleep_counters.end() || it->second < vbd::limits.sleep_threshold) {
			return false;
		}
	}
	return true;
}

auto gse::physics::history_steps(const data& d) -> std::size_t {
	return static_cast<std::size_t>(std::max({ d.rollback_history_steps, d.rollback_history_floor, 0 }));
}

auto gse::physics::snapshot_step(data& d, write<transform_component>& transform, write<motion_component>& motion, const step_inputs& inputs) -> void {
	const auto history = history_steps(d);
	if (history == 0) {
		if (!d.rollback_ring.empty()) {
			d.rollback_ring.clear();
		}
		return;
	}

	trace::scope_guard _{ trace_id<"physics::snapshot_step">() };

	if (d.rollback_ring.size() != history) {
		d.rollback_ring.assign(history, {});
	}

	auto& snap = d.rollback_ring[d.step_index % history];
	snap.step = d.step_index;

	const auto owners = motion.owner_ids();
	snap.bodies.clear();
	snap.bodies.reserve(motion.size());
	for (std::size_t i = 0; i < motion.size(); ++i) {
		if (const auto* tc = transform.find(owners[i])) {
			snap.bodies.push_back({
				.owner = owners[i],
				.transform = *tc,
				.motion = motion[i],
			});
		}
	}

	const auto previous = d.vbd_solver.previous_velocities();
	const auto weights = d.vbd_solver.accel_weights();
	const auto carried_count = std::min({ previous.size(), weights.size(), owners.size() });
	snap.carried.clear();
	snap.carried.reserve(carried_count);
	for (std::size_t i = 0; i < carried_count; ++i) {
		snap.carried.push_back({
			.owner = owners[i],
			.previous_velocity = previous[i],
			.accel_weight = weights[i],
		});
	}

	snap.sleep_counters = d.sleep_counters;
	snap.joints = d.joints;
	snap.contact_cache = d.contact_cache;
	snap.inputs = inputs;
}

auto gse::physics::restore_step(data& d, const step_snapshot& snap, write<transform_component>& transform, write<motion_component>& motion) -> void {
	trace::scope_guard _{ trace_id<"physics::restore_step">() };

	for (const auto& body : snap.bodies) {
		if (auto* tc = transform.find(body.owner)) {
			*tc = body.transform;
		}
		if (auto* mc = motion.find(body.owner)) {
			*mc = body.motion;
		}
	}

	d.sleep_counters = snap.sleep_counters;
	d.joints = snap.joints;
	d.contact_cache = snap.contact_cache;

	std::vector<vec3<velocity>> previous(motion.size());
	std::vector<float> weights(motion.size(), 0.f);
	for (const auto& carried : snap.carried) {
		const auto* mc = motion.find(carried.owner);
		if (!mc) {
			continue;
		}
		const auto index = static_cast<std::size_t>(mc - motion.data());
		previous[index] = carried.previous_velocity;
		weights[index] = carried.accel_weight;
	}
	d.vbd_solver.seed_carried(previous, weights);

	d.step_index = snap.step;
}

auto gse::physics::rollback(data& d, const rollback_request& request, write<transform_component>& transform, write<motion_component>& motion, write<collision_component>& collision, write<collision_result_component>& results) -> void {
	const auto steps = request.steps;
	const auto history = static_cast<std::uint64_t>(history_steps(d));
	if (steps == 0 || history == 0 || steps >= history || steps > d.step_index || d.rollback_ring.size() != history) {
		log::println(
			log::level::warning,
			log::category::physics,
			"rollback of {} step(s) refused: history holds {} step(s) and {} have been simulated",
			steps,
			history,
			d.step_index
		);
		return;
	}

	const auto target = d.step_index - steps;
	const auto& snap = d.rollback_ring[target % history];
	if (snap.step != target) {
		log::println(
			log::level::warning,
			log::category::physics,
			"rollback to step {} refused: the ring slot holds step {}",
			target,
			snap.step
		);
		return;
	}

	for (const auto& override : request.overrides) {
		if (override.step <= target || override.step > d.step_index) {
			continue;
		}
		auto& entry = d.rollback_ring[override.step % history];
		if (entry.step != override.step) {
			continue;
		}
		if (const auto it = std::ranges::find(entry.inputs.motors, override.owner, &motor_input::owner); it != entry.inputs.motors.end()) {
			it->motor = override.motor;
		}
		else {
			entry.inputs.motors.push_back({
				.owner = override.owner,
				.motor = override.motor,
			});
		}
	}

	std::vector<step_inputs> replay;
	replay.reserve(steps);
	for (std::uint64_t s = target + 1; s <= d.step_index; ++s) {
		const auto& entry = d.rollback_ring[s % history];
		if (entry.step != s) {
			log::println(
				log::level::warning,
				log::category::physics,
				"rollback refused: the ring slot for step {} holds step {}",
				s,
				entry.step
			);
			return;
		}
		replay.push_back(entry.inputs);
	}

	const auto apply_body_overrides = [&](const std::uint64_t at) {
		for (const auto& body : request.body_overrides) {
			if (body.step != at) {
				continue;
			}
			if (auto* tc = transform.find(body.owner)) {
				*tc = body.transform;
			}
			if (auto* mc = motion.find(body.owner)) {
				*mc = body.motion;
				d.sleep_counters[body.owner] = 0;
				const auto index = static_cast<std::size_t>(mc - motion.data());
				if (index < d.body_sleeping.size()) {
					d.body_sleeping[index] = 0;
				}
			}
		}
	};

	trace::scope_guard _{ trace_id<"physics::rollback">() };
	restore_step(d, snap, transform, motion);
	apply_body_overrides(target);
	for (std::size_t i = 0; i < replay.size(); ++i) {
		update_vbd(1, d, transform, motion, collision, results, std::span<const step_inputs>(replay.data() + i, 1));
		apply_body_overrides(d.step_index);
	}
}

auto gse::physics::update_vbd(const int steps, data& d, write<transform_component>& transform, write<motion_component>& motion, write<collision_component>& collision, write<collision_result_component>& results, const std::span<const step_inputs> per_step) -> void {
	trace::scope_guard _{ trace_id<"vbd_cpu::update">() };

	const auto const_update_time = system_clock::constant_update_time();
	const int substeps = std::max(d.physics_substeps, 1);
	const auto sub_dt = const_update_time / static_cast<float>(substeps);

	d.body_airborne.resize(motion.size(), 1);
	d.body_sleeping.resize(motion.size(), 0);

	if (per_step.size() < static_cast<std::size_t>(std::max(steps, 0))) {
		log::println(
			log::level::error,
			log::category::physics,
			"update_vbd asked for {} step(s) but was given inputs for {}; stepping none",
			steps,
			per_step.size()
		);
		return;
	}

	std::flat_set<std::pair<std::uint64_t, std::uint64_t>> jointed_pairs;
	{
		std::vector<std::pair<std::uint64_t, std::uint64_t>> pair_staging;
		pair_staging.reserve(d.joints.size());
		for (const auto& jd : d.joints.items()) {
			const auto a = jd.entity_a.number();
			const auto b = jd.entity_b.number();
			pair_staging.emplace_back(std::min(a, b), std::max(a, b));
		}
		jointed_pairs.insert(pair_staging.begin(), pair_staging.end());
	}

	const auto& solver_cfg = d.vbd_solver.config();
	const length max_travel = solver_cfg.max_linear_speed * sub_dt * static_cast<float>(substeps - 1);
	const auto pair_sweep_margin = solver_cfg.speculative_margin + 2.f * max_travel;
	const auto broad_phase_chunks = static_cast<std::size_t>(std::max(d.broad_phase_chunks_per_worker, 1));

	std::vector<collision_pair> objects;
	std::vector<candidate_pair> pair_candidates;
	std::vector<vbd::velocity_motor_constraint> motor_constraints;
	std::vector<vbd::joint_constraint> joint_constraints;
	std::vector<vbd::body_state> bodies;
	std::vector<vbd::body_state> result_bodies;
	std::vector<std::uint8_t> has_transform;

	body_build_view view{
		.motion_owners = motion.owner_ids(),
		.motions = { motion.data(), motion.size() },
		.transform_owners = transform.owner_ids(),
		.transforms = { transform.data(), transform.size() },
		.collision_owners = collision.owner_ids(),
		.collisions = { collision.data(), collision.size() },
		.hulls = d.hulls,
	};

	std::vector<mass_properties> body_props;
	build_mass_properties(view, body_props);
	view.mass_props = body_props;

	const int total_substeps = steps * substeps;
	for (int step = 0; step < total_substeps; ++step) {
		trace::scope_guard _{ trace_id<"vbd_cpu::substep">() };

		const auto& in = per_step[static_cast<std::size_t>(step / substeps)];
		if (step % substeps == 0) {
			apply_step_inputs(in, d, transform, motion);
			if (step_all_asleep(in, d, motion)) {
				trace::scope_guard _{ trace_id<"vbd_cpu::asleep_skip">() };
				step += substeps - 1;
				++d.step_index;
				snapshot_step(d, transform, motion, in);
				continue;
			}
		}

		build_body_states(view, d.sleep_counters, bodies, d.id_to_body_index, has_transform);

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
			trace::scope_guard _{ trace_id<"vbd_cpu::begin_frame">() };
			d.vbd_solver.begin_frame(bodies, view.motion_owners, d.contact_cache);
		}

		const bool rebuild_pairs = step % substeps == 0 || !pair_set_valid(objects, max_travel);
		if (rebuild_pairs) {
			objects = collect_collision_objects(transform, collision, motion, d.id_to_body_index, d.hulls);
			pair_candidates = build_pair_set(objects, jointed_pairs, pair_sweep_margin, broad_phase_chunks);
		}
		add_scene_contacts_to_solver(
			d.vbd_solver,
			d.contact_cache,
			objects,
			pair_candidates,
			sub_dt,
			results,
			d.body_airborne,
			broad_phase_chunks
		);

		build_motor_constraints(in.motors, d.id_to_body_index, d.body_airborne, d.vbd_solver.body_states(), motor_constraints);
		for (const auto& constraint : motor_constraints) {
			d.vbd_solver.add_motor_constraint(constraint);
		}

		build_joint_constraints(d.joints.items(), d.id_to_body_index, bodies, joint_constraints);
		for (const auto& constraint : joint_constraints) {
			d.vbd_solver.add_joint_constraint(constraint);
		}

		d.vbd_solver.solve(sub_dt);

		{
			std::uint32_t ji = 0;
			const auto& solved_joints = d.vbd_solver.graph().joint_constraints();
			for (auto& jd : d.joints.items()) {
				const auto it_a = d.id_to_body_index.find(jd.entity_a);
				const auto it_b = d.id_to_body_index.find(jd.entity_b);
				if (it_a == d.id_to_body_index.end() || it_b == d.id_to_body_index.end()) {
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

		{
			trace::scope_guard _{ trace_id<"vbd_cpu::end_frame">() };
			d.vbd_solver.end_frame(result_bodies, d.contact_cache);
		}

		{
			trace::scope_guard _{ trace_id<"vbd_cpu::writeback">() };
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
					tc->position = origin_from_com(bs.position, bs.orientation, bs.com_local);
					tc->orientation = bs.orientation;
					continue;
				}

				tc->position = origin_from_com(bs.position, bs.orientation, bs.com_local);
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

		if ((step + 1) % substeps == 0) {
			++d.step_index;
			snapshot_step(d, transform, motion, in);
		}
	}
}

auto gse::physics::frame(context& ctx, const std::optional<shared_view<gpu::context::data>> gpu_s, data& d, const channel_read<vbd::solver_upload> uploads_in, const channel_write<gpu_solver_frame_info> frame_out, const channel_write<gpu::render_pass_request> pass_out) -> async::task<> {
	if (!gpu_s || !gpu_solver_active(d) || !d.gpu_solver.compute_initialized()) {
		co_return;
	}

	if (const auto payloads = ctx.drain_channel<gpu_upload_payload>(); !payloads.empty()) {
		trace::scope_guard _{ trace_id<"physics::frame::upload">() };
		d.gpu_solver.upload(payloads.back().upload);
	}
	else if (const auto& uploads = uploads_in.of<vbd::solver_upload>(); !uploads.empty()) {
		trace::scope_guard _{ trace_id<"physics::frame::upload">() };
		d.gpu_solver.upload(uploads[0]);
	}

	d.gpu_solver.ensure_ring(*gpu_s->device, static_cast<std::uint32_t>(history_steps(d)));

	if (d.gpu_solver.pending_dispatch()) {
		{
			trace::scope_guard _{ trace_id<"physics::frame::commit_upload">() };
			d.gpu_solver.commit_upload();
		}
		co_await d.gpu_solver.dispatch_compute(ctx, pass_out);
	}

	frame_out.push<gpu_solver_frame_info>(gpu_solver_frame_info_of(d));
}