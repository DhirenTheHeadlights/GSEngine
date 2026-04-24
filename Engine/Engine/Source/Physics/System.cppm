export module gse.physics:system;

import std;

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
		vec3<lever_arm> local_anchor_a;
		vec3<lever_arm> local_anchor_b;
		vec3f local_axis_a = { 0.f, 1.f, 0.f };
		vec3f local_axis_b = { 0.f, 1.f, 0.f };
		displacement target_distance = {};
		linear_compliance compliance = {};
		float damping = 0.f;
		angle limit_lower = radians(-std::numbers::pi_v<float>);
		angle limit_upper = radians(std::numbers::pi_v<float>);
		bool limits_enabled = false;
		quat rest_orientation;
		bool rest_orientation_initialized = false;

		vec3<force> pos_lambda;
		vec3<stiffness> pos_penalty;
		vec3<torque> ang_lambda;
		vec3<angular_stiffness> ang_penalty;
		torque limit_lambda = {};
		angular_stiffness limit_penalty = {};
	};

	using joint_handle = std::uint32_t;

	struct gpu_upload_payload {
		std::vector<vbd::body_state> bodies;
		std::vector<vbd::collision_body_data> collision_data;
		std::vector<float> accel_weights;
		std::vector<vbd::velocity_motor_constraint> motors;
		std::vector<vbd::joint_constraint> joints;
		std::vector<vbd::warm_start_entry> warm_starts;
		std::vector<std::uint32_t> authoritative_body_indices;
		vbd::solver_config solver_cfg;
		time_t<float, seconds> dt{};
		int steps = 1;
		std::vector<id> entity_ids;
		std::uint32_t joint_count = 0;
	};

	struct gpu_body_index_map {
		std::vector<std::pair<id, std::uint32_t>> entries;
	};

	struct gpu_readback_result {
		std::vector<id> entity_ids;
		std::vector<vbd::body_state> gpu_input_bodies;
		std::vector<vbd::body_state> gpu_result_bodies;
		std::vector<vbd::contact_readback_entry> gpu_contacts;
		std::vector<vbd::joint_constraint> gpu_joint_readback;
		std::uint32_t gpu_joint_count = 0;
	};

	struct gpu_solver_stats {
		bool active = false;
		std::uint32_t contact_count = 0;
		std::uint32_t motor_count = 0;
		time_t<float, seconds> solve_time{};
	};

	struct gpu_solver_frame_info {
		const gpu::buffer* snapshot = nullptr;
		gpu::compute_semaphore_state semaphore{};
		std::uint32_t body_count = 0;
		std::uint32_t body_stride = 0;
		std::uint32_t position_offset = 0;
	};

	struct state {
		bool update_phys = true;
		bool use_gpu_solver = false;
		bool gpu_buffers_created = false;
		int solver_iterations = 15;
		bool compare_solvers = false;
		bool use_jacobi = false;
		float jacobi_omega = 0.67f;
		int physics_substeps = 2;
		gpu_solver_stats gpu_stats;
		std::vector<joint_definition> joints;
	};

	auto create_joint(
		state& s,
		const joint_definition& def
	) -> joint_handle;

	auto remove_joint(
		state& s,
		joint_handle handle
	) -> void;

	struct system {
		struct update_data {
			time_t<float, seconds> accumulator{};
			clock tick_clock;
			bool tick_clock_primed = false;
			vbd::solver vbd_solver;
			vbd::contact_cache contact_cache;
			std::unordered_map<id, std::uint32_t> sleep_counters;
			interval_timer<> comparison_timer{ seconds(0.25f) };
			struct solver_comparison_snapshot {
				std::vector<vbd::body_state> cpu_result;
				std::vector<vbd::contact_constraint> cpu_contacts;
				std::vector<vbd::joint_constraint> cpu_joints;
			};
			std::optional<solver_comparison_snapshot> comparison_pending;
			struct gpu_prev_frame {
				std::vector<vbd::body_state> result_bodies;
				std::vector<id> result_entity_ids;
				std::vector<vbd::warm_start_entry> warm_start_contacts;

				gpu_prev_frame() {
					result_bodies.reserve(vbd::max_bodies);
					result_entity_ids.reserve(vbd::max_bodies);
					warm_start_contacts.reserve(vbd::max_contacts);
				}
			} gpu_prev;
			std::optional<gpu_readback_result> completed_readback;
		};

		struct frame_data {
			vbd::gpu_solver gpu_solver;
			struct readback_frame {
				std::vector<id> entity_ids;
				std::vector<vbd::body_state> gpu_input_bodies;
				std::uint32_t gpu_joint_count = 0;
			};
			std::optional<readback_frame> in_flight;
		};

		static auto initialize(
			const init_context& phase,
			update_data& ud,
			frame_data& fd,
			state& s
		) -> void;

		static auto update(
			update_context& ctx,
			update_data& ud,
			state& s
		) -> async::task<>;

		static auto frame(
			frame_context& ctx,
			frame_data& fd,
			const state& s
		) -> async::task<>;
	};
}

namespace gse::physics {
	struct collision_pair {
		collision_component* collision;
		motion_component* motion;
	};

	struct contact_compare_key {
		std::uint32_t body_a = 0;
		std::uint32_t body_b = 0;
		std::uint64_t feature_key = 0;

		auto operator==(
			const contact_compare_key&
		) const -> bool = default;
	};

	struct contact_compare_key_hash {
		auto operator()(
			const contact_compare_key& key
		) const noexcept -> std::size_t;
	};

	auto collect_collision_objects(
		write<motion_component>& motion,
		write<collision_component>& collision
	) -> std::vector<collision_pair>;

	auto add_scene_contacts_to_solver(
		vbd::solver& solver,
		vbd::contact_cache& contact_cache,
		const std::vector<collision_pair>& objects,
		const flat_map<id, std::uint32_t>& id_to_body_index,
		bool update_scene_state
	) -> void;

	auto pack_feature(
		const feature_id& feature
	) -> std::uint64_t;

	auto unpack_feature(
		std::uint64_t packed
	) -> feature_id;

	auto build_contact_cache_from_warm_start(
		const std::span<const vbd::warm_start_entry> warm_start_contacts
	) -> vbd::contact_cache;

	auto invalidate_warm_start_entries(
		std::vector<vbd::warm_start_entry>& warm_start_contacts,
		const std::span<const std::uint32_t> body_indices
	) -> void;

	auto update_vbd(
		int steps,
		system::update_data& ud,
		state& s,
		write<motion_component>& motion,
		write<collision_component>& collision
	) -> void;

	auto update_vbd_gpu(
		int steps,
		system::update_data& ud,
		state& s,
		write<motion_component>& motion,
		write<collision_component>& collision,
		time_t<float, seconds> dt,
		channel_writer& channels
	) -> void;
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

auto gse::physics::contact_compare_key_hash::operator()(const contact_compare_key& key) const noexcept -> std::size_t {
	return gse::hash_combine(key);
}

auto gse::physics::collect_collision_objects(write<motion_component>& motion, write<collision_component>& collision) -> std::vector<collision_pair> {
	std::vector<collision_pair> objects;
	objects.reserve(collision.size());
	for (collision_component& cc : collision) {
		if (!cc.resolve_collisions) {
			continue;
		}
		objects.push_back({
			.collision = std::addressof(cc),
			.motion = motion.find(cc.owner_id())
		});
	}
	return objects;
}

auto gse::physics::add_scene_contacts_to_solver(vbd::solver& solver, vbd::contact_cache& contact_cache, const std::vector<collision_pair>& objects, const flat_map<id, std::uint32_t>& id_to_body_index, const bool update_scene_state) -> void {
	for (std::size_t i = 0; i < objects.size(); ++i) {
		for (std::size_t j = i + 1; j < objects.size(); ++j) {
			auto& [collision_a, motion_a] = objects[i];
			auto& [collision_b, motion_b] = objects[j];

			const auto& aabb_a = collision_a->bounding_box.aabb();
			const auto& aabb_b = collision_b->bounding_box.aabb();

			if (!aabb_a.overlaps(aabb_b, solver.config().speculative_margin)) {
				continue;
			}

			const narrow_phase_collision::shape_data sd_a{
				.bb = &collision_a->bounding_box,
				.type = collision_a->shape,
				.radius = collision_a->shape_radius,
				.half_height = collision_a->shape_half_height
			};
			const narrow_phase_collision::shape_data sd_b{
				.bb = &collision_b->bounding_box,
				.type = collision_b->shape,
				.radius = collision_b->shape_radius,
				.half_height = collision_b->shape_half_height
			};

			auto sat_result = narrow_phase_collision::speculative_test(
				sd_a, sd_b, solver.config().speculative_margin
			);

			if (!sat_result) {
				continue;
			}

			auto& sat = *sat_result;
			if (dot(sat.normal, collision_b->bounding_box.center() - collision_a->bounding_box.center()) < meters(0.f)) {
				sat.normal = -sat.normal;
			}

			auto manifold = narrow_phase_collision::generate_shape_manifold(
				sd_a, sd_b, sat.normal, sat.separation
			);

			if (manifold.point_count == 0) {
				continue;
			}

			const auto it_a = id_to_body_index.find(collision_a->owner_id());
			const auto it_b = id_to_body_index.find(collision_b->owner_id());
			if (it_a == id_to_body_index.end() || it_b == id_to_body_index.end()) {
				continue;
			}

			const std::uint32_t body_a = it_a->second;
			const std::uint32_t body_b = it_b->second;

			if (update_scene_state) {
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

				const float pair_restitution = std::max(
					motion_a ? motion_a->restitution : 0.f,
					motion_b ? motion_b->restitution : 0.f
				);

				solver.add_contact_constraint(vbd::contact_constraint{
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
					.restitution = pair_restitution,
					.sticking = cached ? cached->sticking : false,
					.feature = feature
				});

				if (update_scene_state) {
					collision_a->collision_information.collision_points.push_back(position_on_a);
				}
			}
		}
	}
}

auto gse::physics::pack_feature(const feature_id& feature) -> std::uint64_t {
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

auto gse::physics::unpack_feature(const std::uint64_t packed) -> feature_id {
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

auto gse::physics::build_contact_cache_from_warm_start(const std::span<const vbd::warm_start_entry> warm_start_contacts) -> vbd::contact_cache {
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

auto gse::physics::invalidate_warm_start_entries(std::vector<vbd::warm_start_entry>& warm_start_contacts, const std::span<const std::uint32_t> body_indices) -> void {
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

auto gse::physics::system::initialize(const init_context& phase, update_data& ud, frame_data& fd, state& s) -> void {
	phase.channels.push<save::register_property>({
		.category = "Physics",
		.name = "Update Physics",
		.description = "Enable or disable the physics system update loop",
		.ref = &s.update_phys,
		.type = typeid(bool)
	});

	phase.channels.push<save::register_property>({
		.category = "Physics",
		.name = "Use GPU Solver",
		.description = "Run VBD solver on GPU via compute shaders",
		.ref = &s.use_gpu_solver,
		.type = typeid(bool)
	});

	phase.channels.push<save::register_property>({
		.category = "Physics",
		.name = "Compare Solvers",
		.description = "Log CPU vs GPU solver comparison every 0.25 seconds",
		.ref = &s.compare_solvers,
		.type = typeid(bool)
	});

	phase.channels.push<save::register_property>({
		.category = "Physics",
		.name = "Solver Iterations",
		.description = "Number of VBD solver iterations per substep (CPU and GPU)",
		.ref = &s.solver_iterations,
		.type = typeid(int),
		.range_min = 1,
		.range_max = 40,
		.range_step = 1
	});

	phase.channels.push<save::register_property>({
		.category = "Physics",
		.name = "Use Jacobi Solver",
		.description = "Use Jacobi instead of Gauss-Seidel (needs more iterations)",
		.ref = &s.use_jacobi,
		.type = typeid(bool)
	});

	phase.channels.push<save::register_property>({
		.category = "Physics",
		.name = "Jacobi Omega",
		.description = "Under-relaxation factor for Jacobi (lower = more stable, slower convergence)",
		.ref = &s.jacobi_omega,
		.type = typeid(float),
		.range_min = 0.1f,
		.range_max = 1.0f,
		.range_step = 0.01f
	});

	phase.channels.push<save::register_property>({
		.category = "Physics",
		.name = "Physics Substeps",
		.description = "Subdivide each 60Hz physics tick into N mini-steps (higher = stabler against fast kinematic boundaries)",
		.ref = &s.physics_substeps,
		.type = typeid(int),
		.range_min = 1,
		.range_max = 8,
		.range_step = 1
	});

	ud.vbd_solver.configure(vbd::solver_config{
		.iterations = static_cast<std::uint32_t>(s.solver_iterations),
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

	if (phase.try_get<gpu::context>()) {
		auto& ctx = phase.get<gpu::context>();
		fd.gpu_solver.initialize_compute(ctx);
		s.gpu_buffers_created = fd.gpu_solver.buffers_created();
	}
}

auto gse::physics::system::update(update_context& ctx, update_data& ud, state& s) -> async::task<> {
	if (const auto& readbacks = ctx.read_channel<gpu_readback_result>(); !readbacks.empty()) {
		auto completed = readbacks[0];
		ud.completed_readback = std::move(completed);
	}
	if (const auto& stats_channel = ctx.read_channel<gpu_solver_stats>(); !stats_channel.empty()) {
		s.gpu_stats = stats_channel[0];
	}

	if (!s.update_phys) {
		co_return;
	}

	if (auto cfg = ud.vbd_solver.config();
		cfg.iterations != static_cast<std::uint32_t>(s.solver_iterations) ||
		cfg.use_jacobi != s.use_jacobi ||
		cfg.jacobi_omega != s.jacobi_omega) {
		cfg.iterations = static_cast<std::uint32_t>(s.solver_iterations);
		cfg.use_jacobi = s.use_jacobi;
		cfg.jacobi_omega = s.jacobi_omega;
		ud.vbd_solver.configure(cfg);
	}

	if (!ud.tick_clock_primed) {
		ud.tick_clock.reset<float>();
		ud.tick_clock_primed = true;
	}

	const time_t<float, seconds> elapsed = ud.tick_clock.reset<float>();
	constexpr time_t<float, seconds> max_time_step = seconds(0.25f);
	const time_t<float, seconds> frame_time = std::min(elapsed, max_time_step);
	ud.accumulator += frame_time;

	const auto const_update_time = system_clock::constant_update_time<time_t<float, seconds>>();

	int steps = 0;
	while (ud.accumulator >= const_update_time) {
		ud.accumulator -= const_update_time;
		steps++;
	}

	if (constexpr int max_physics_steps = 4; steps > max_physics_steps) {
		ud.accumulator = {};
		steps = max_physics_steps;
	}

	auto [motion, collision] = co_await ctx.acquire<write<motion_component>, write<collision_component>>();

	if (s.use_gpu_solver) {
		update_vbd_gpu(steps, ud, s, motion, collision, const_update_time, ctx.channels);
		co_return;
	}

	update_vbd(steps, ud, s, motion, collision);
}

auto gse::physics::update_vbd_gpu(const int steps, system::update_data& ud, state& s, write<motion_component>& motion, write<collision_component>& collision, const time_t<float, seconds> dt, channel_writer& channels) -> void {
	if (!s.gpu_buffers_created) {
		return;
	}

	if (ud.completed_readback) {
		trace::scope(
			find_or_generate_id("vbd_gpu::readback"),
			[&] {
				auto completed = std::move(*ud.completed_readback);
				ud.completed_readback.reset();

				trace::scope(
					find_or_generate_id("vbd_gpu::readback_bodies"),
					[&] {
						const auto body_count = completed.entity_ids.size();
						bool motion_order_matches = body_count == motion.size();
						for (std::size_t i = 0; motion_order_matches && i < body_count; ++i) {
							motion_order_matches = completed.entity_ids[i] == motion[i].owner_id();
						}

						bool collision_order_matches = body_count == collision.size();
						for (std::size_t i = 0; collision_order_matches && i < body_count; ++i) {
							collision_order_matches = completed.entity_ids[i] == collision[i].owner_id();
						}

						for (std::size_t i = 0; i < body_count; ++i) {
							const auto eid = completed.entity_ids[i];
							auto* mc = motion_order_matches ? std::addressof(motion[i]) : motion.find(eid);
							if (!mc) {
								continue;
							}

							const auto& bs = completed.gpu_result_bodies[i];

							if (!mc->position_locked) {
								mc->current_position = bs.position;
								mc->current_velocity = bs.body_velocity;
								if (mc->update_orientation) {
									mc->orientation = bs.orientation;
									mc->angular_velocity = bs.body_angular_velocity;
								}

								ud.sleep_counters[eid] = bs.sleep_counter;
								mc->sleeping = bs.sleeping();
							}

							mc->airborne = true;

							auto* cc = collision_order_matches ? std::addressof(collision[i]) : collision.find(eid);
							if (cc) {
								cc->bounding_box.update(mc->current_position, mc->orientation);
								if (cc->resolve_collisions) {
									auto& info = cc->collision_information;
									info.colliding = false;
									info.collision_normal = {};
									info.penetration = {};
									info.collision_points.clear();
								}
							}
						}
					}
				);

				trace::scope(
					find_or_generate_id("vbd_gpu::readback_contacts"),
					[&] {
						const auto& cfg = ud.vbd_solver.config();
						ud.gpu_prev.warm_start_contacts.clear();
						ud.gpu_prev.warm_start_contacts.reserve(completed.gpu_contacts.size());

						for (const auto& c : completed.gpu_contacts) {
							const force friction_bound = abs(c.lambda[0]) * c.friction_coeff;
							const force tangential_lambda = hypot(c.lambda[1], c.lambda[2]);
							const length tangential_gap = hypot(c.c0[1], c.c0[2]);
							const bool sticking =
								c.lambda[0] < newtons(-1e-3f) &&
								tangential_gap < cfg.stick_threshold &&
								tangential_lambda < friction_bound;

							ud.gpu_prev.warm_start_contacts.push_back({
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

							const auto body_a = c.body_a;
							const auto body_b = c.body_b;
							if (body_a >= completed.entity_ids.size() || body_b >= completed.entity_ids.size()) {
								continue;
							}

							const auto eid_a = completed.entity_ids[body_a];
							const auto eid_b = completed.entity_ids[body_b];
							const auto& normal = c.normal;

							if (auto* mc_b = motion.find(eid_b)) {
								if (normal.y() < -0.7f) {
									mc_b->airborne = false;
								}
							}
							if (auto* mc_a = motion.find(eid_a)) {
								if (normal.y() > 0.7f) {
									mc_a->airborne = false;
								}
							}

							const auto& bs_a = completed.gpu_result_bodies[body_a];
							const auto& bs_b = completed.gpu_result_bodies[body_b];
							const auto world_r_a = rotate_vector(bs_a.orientation, c.local_anchor_a);
							const auto world_r_b = rotate_vector(bs_b.orientation, c.local_anchor_b);
							const auto contact_point_a = bs_a.position + world_r_a;
							const auto contact_point_b = bs_b.position + world_r_b;
							const auto midpoint = contact_point_a + (contact_point_b - contact_point_a) * 0.5f;

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
						}
					}
				);

				trace::scope(
					find_or_generate_id("vbd_gpu::readback_sort"),
					[&] {
						std::ranges::sort(
							ud.gpu_prev.warm_start_contacts,
							[](const vbd::warm_start_entry& a, const vbd::warm_start_entry& b) {
								if (a.body_a != b.body_a) {
									return a.body_a < b.body_a;
								}
								if (a.body_b != b.body_b) {
									return a.body_b < b.body_b;
								}
								return a.feature_key < b.feature_key;
							}
						);
					}
				);

				trace::scope(
					find_or_generate_id("vbd_gpu::readback_joints"),
					[&] {
						std::uint32_t ji = 0;
						for (auto& jd : s.joints) {
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
				);

		if (ud.comparison_pending.has_value()) {
			const auto& cpu = ud.comparison_pending->cpu_result;
			const auto& gpu = completed.gpu_result_bodies;
			const auto& cpu_contacts = ud.comparison_pending->cpu_contacts;
			const auto& cpu_joints = ud.comparison_pending->cpu_joints;
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
				const auto ve = magnitude(gpu[i].body_velocity - cpu[i].body_velocity);
				const auto ae = magnitude(gpu[i].body_angular_velocity - cpu[i].body_angular_velocity);
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
					.feature_key = pack_feature(c.feature)
				}] = std::addressof(c);
			}

			std::unordered_map<contact_compare_key, const vbd::contact_readback_entry*, contact_compare_key_hash> gpu_contact_map;
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
			const vbd::contact_readback_entry* worst_gpu_contact = nullptr;

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
					cb.body_velocity,
					gb.body_velocity,
					cb.body_angular_velocity,
					gb.body_angular_velocity
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

			ud.comparison_pending.reset();
		}

				trace::scope(
					find_or_generate_id("vbd_gpu::readback_cache"),
					[&] {
						ud.gpu_prev.result_bodies.assign(
							completed.gpu_result_bodies.begin(),
							completed.gpu_result_bodies.end()
						);
						ud.gpu_prev.result_entity_ids.assign(
							completed.entity_ids.begin(),
							completed.entity_ids.end()
						);
					}
				);
		});
	}

	if (steps <= 0) {
		return;
	}

	std::vector<id> launched_entities;
	trace::scope(find_or_generate_id("vbd_gpu::impulses"), [&] {
		launched_entities.reserve(motion.size());

		for (motion_component& mc : motion) {
			if (mc.position_locked) {
				continue;
			}
			if (magnitude(mc.pending_impulse) > newton_seconds(1e-6f)) {
				mc.current_velocity += mc.pending_impulse / mc.mass;
				ud.sleep_counters[mc.owner_id()] = 0;
				launched_entities.push_back(mc.owner_id());
				mc.pending_impulse = {};
			}
		}
	});

	flat_map<id, std::uint32_t> id_to_body_index;
	std::vector<vbd::body_state> bodies;
	std::vector<id> entity_ids;
	std::vector<float> accel_weights;
	std::optional<flat_map<id, vec3<velocity>>> prev_gpu_velocity;
	const auto& prev_result_entity_ids = ud.gpu_prev.result_entity_ids;
	const auto& prev_result_bodies = ud.gpu_prev.result_bodies;
	bool prev_order_matches = false;

	std::vector<std::pair<id, std::uint32_t>> id_to_body_index_staging;

	trace::scope(find_or_generate_id("vbd_gpu::build_bodies"), [&] {
		const auto body_count = motion.size();
		bodies.resize(body_count);
		entity_ids.resize(body_count);
		accel_weights.resize(body_count);
		id_to_body_index_staging.resize(body_count);

		const auto prev_count = std::min(prev_result_entity_ids.size(), prev_result_bodies.size());
		prev_order_matches = prev_count == body_count;
		for (std::size_t i = 0; prev_order_matches && i < body_count; ++i) {
			prev_order_matches = prev_result_entity_ids[i] == motion[i].owner_id();
		}
		if (!prev_order_matches) {
			std::vector<std::pair<id, vec3<velocity>>> prev_staging;
			prev_staging.reserve(prev_count);
			for (std::size_t i = 0; i < prev_count; ++i) {
				prev_staging.emplace_back(prev_result_entity_ids[i], prev_result_bodies[i].body_velocity);
			}
			prev_gpu_velocity.emplace();
			prev_gpu_velocity->assign_unsorted(std::move(prev_staging));
		}

		constexpr acceleration gravity_mag = meters_per_second_squared(9.8f);
		const auto& sleep_counters_ref = ud.sleep_counters;
		const auto& prev_gpu_velocity_ref = prev_gpu_velocity;

		task::parallel_invoke_range(0, body_count, [&, dt](std::size_t i) {
			motion_component& mc = motion[i];
			const auto eid = mc.owner_id();
			id_to_body_index_staging[i] = { eid, static_cast<std::uint32_t>(i) };

			const auto sc_it = sleep_counters_ref.find(eid);
			const auto sc = sc_it != sleep_counters_ref.end() ? sc_it->second : 0u;

			float accel_weight = 0.f;
			if (!mc.position_locked && sc < 60u && dt > time_t<float, seconds>(seconds(1e-6f))) {
				if (prev_order_matches) {
					const velocity delta_vy = mc.current_velocity.y() - prev_result_bodies[i].body_velocity.y();
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
			accel_weights[i] = accel_weight;

			bodies[i] = {
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
			};
			entity_ids[i] = eid;
		});

		id_to_body_index.assign_unsorted(std::move(id_to_body_index_staging));
	});

	std::vector<std::uint32_t> jumped_body_indices;
	std::vector<vbd::collision_body_data> collision_data;

	trace::scope(
		find_or_generate_id("vbd_gpu::build_collision"),
		[&] {
			jumped_body_indices.reserve(launched_entities.size());
			for (const auto eid : launched_entities) {
				if (const auto it = id_to_body_index.find(eid); it != id_to_body_index.end()) {
					jumped_body_indices.push_back(it->second);
				}
			}
			invalidate_warm_start_entries(ud.gpu_prev.warm_start_contacts, jumped_body_indices);

			collision_data.resize(bodies.size());
			for (auto& cd : collision_data) {
				cd.aabb_min = vec3<position>(position(1e30f));
				cd.aabb_max = vec3<position>(position(-1e30f));
			}

			const auto& id_to_body_index_ref = id_to_body_index;
			task::parallel_invoke_range(
				0,
				collision.size(),
				[&](std::size_t i) {
					collision_component& cc = collision[i];
					if (!cc.resolve_collisions) {
						return;
					}
					const auto it = id_to_body_index_ref.find(cc.owner_id());
					if (it == id_to_body_index_ref.end()) {
						return;
					}

					cc.bounding_box.update(bodies[it->second].position, bodies[it->second].orientation);
					const auto& [max, min] = cc.bounding_box.aabb();
					collision_data[it->second] = {
						.half_extents = cc.bounding_box.half_extents(),
						.aabb_min = min,
						.aabb_max = max
					};
				}
			);
		}
	);

	std::vector<vbd::velocity_motor_constraint> motors;
	trace::scope(find_or_generate_id("vbd_gpu::build_motors"), [&] {
		for (motion_component& mc : motion) {
			if (!mc.velocity_drive_active) {
				continue;
			}
			if (mc.airborne) {
				continue;
			}
			if (!id_to_body_index.contains(mc.owner_id())) {
				continue;
			}

			const auto idx = id_to_body_index[mc.owner_id()];
			if (bodies[idx].sleeping() && magnitude(mc.velocity_drive_target) > meters_per_second(.01f)) {
				bodies[idx].sleep_counter = 0;
			}

			motors.push_back(vbd::velocity_motor_constraint{
				.body_index = idx,
				.target_velocity = mc.velocity_drive_target,
				.compliance = 0.5f,
				.max_force = mc.mass * meters_per_second_squared(50.f),
				.horizontal_only = true,
			});
		}
	});

	std::vector<vbd::joint_constraint> gpu_joints;
	trace::scope(find_or_generate_id("vbd_gpu::build_joints"), [&] {
		gpu_joints.reserve(s.joints.size());
		for (auto& jd : s.joints) {
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
				.local_anchor_a = jd.local_anchor_a,
				.local_anchor_b = jd.local_anchor_b,
				.local_axis_a = jd.local_axis_a,
				.local_axis_b = jd.local_axis_b,
				.target_distance = jd.target_distance,
				.compliance = jd.compliance,
				.damping = jd.damping,
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
	});

	trace::scope(find_or_generate_id("vbd_gpu::upload"), [&] {
		channels.push<gpu_upload_payload>({
			.bodies = bodies,
			.collision_data = collision_data,
			.accel_weights = accel_weights,
			.motors = motors,
			.joints = gpu_joints,
			.warm_starts = std::vector(ud.gpu_prev.warm_start_contacts.begin(), ud.gpu_prev.warm_start_contacts.end()),
			.authoritative_body_indices = jumped_body_indices,
			.solver_cfg = ud.vbd_solver.config(),
			.dt = dt * static_cast<float>(steps),
			.steps = steps * std::max(s.physics_substeps, 1),
			.entity_ids = entity_ids,
			.joint_count = static_cast<std::uint32_t>(gpu_joints.size())
		});

		gpu_body_index_map body_map;
		body_map.entries.reserve(id_to_body_index.size());
		for (const auto& [eid, idx] : id_to_body_index) {
			body_map.entries.emplace_back(eid, idx);
		}
		channels.push(std::move(body_map));
	});

	if (s.compare_solvers && ud.comparison_timer.tick()) {
		trace::scope(find_or_generate_id("vbd_gpu::compare"), [&] {
		if (steps != 1) {
			log::println("SOLVER COMPARE: skipped for {} fixed steps; single-step captures are the only apples-to-apples parity check right now", steps);
		}
		else {
			vbd::solver cpu_ref;
			cpu_ref.configure(ud.vbd_solver.config());
			auto ref_cache = build_contact_cache_from_warm_start(ud.gpu_prev.warm_start_contacts);
			cpu_ref.begin_frame(bodies, ref_cache);

			std::vector<vec3<velocity>> ref_prev_velocities;
			ref_prev_velocities.reserve(entity_ids.size());
			for (std::size_t i = 0; i < entity_ids.size(); ++i) {
				if (prev_order_matches && i < prev_result_bodies.size()) {
					ref_prev_velocities.push_back(prev_result_bodies[i].body_velocity);
				}
				else if (prev_gpu_velocity) {
					if (const auto it = prev_gpu_velocity->find(entity_ids[i]); it != prev_gpu_velocity->end()) {
						ref_prev_velocities.push_back(it->second);
					}
					else {
						ref_prev_velocities.push_back(bodies[i].body_velocity);
					}
				}
				else {
					ref_prev_velocities.push_back(bodies[i].body_velocity);
				}
			}
			cpu_ref.seed_previous_velocities(ref_prev_velocities);

			const auto objects = collect_collision_objects(motion, collision);
			add_scene_contacts_to_solver(cpu_ref, ref_cache, objects, id_to_body_index, false);

			for (const auto& m : motors) {
				cpu_ref.add_motor_constraint(m);
			}
			for (const auto& j : gpu_joints) {
				cpu_ref.add_joint_constraint(j);
			}

			cpu_ref.solve(dt);

			std::vector<vbd::body_state> cpu_result;
			cpu_ref.end_frame(cpu_result, ref_cache);

			ud.comparison_pending = system::update_data::solver_comparison_snapshot{
				.cpu_result = std::move(cpu_result),
				.cpu_contacts = cpu_ref.graph().contact_constraints() | std::views::all | std::ranges::to<std::vector>(),
				.cpu_joints = cpu_ref.graph().joint_constraints() | std::views::all | std::ranges::to<std::vector>(),
			};
		}
		});
	}
}

auto gse::physics::update_vbd(const int steps, system::update_data& ud, state& s, write<motion_component>& motion, write<collision_component>& collision) -> void {
	const auto const_update_time = system_clock::constant_update_time<time_t<float, seconds>>();
	const int substeps = std::max(s.physics_substeps, 1);
	const auto sub_dt = const_update_time / static_cast<float>(substeps);

	flat_map<id, std::uint32_t> id_to_body_index;
	std::vector<motion_component*> motion_ptrs;
	motion_ptrs.reserve(motion.size());

	{
		std::vector<std::pair<id, std::uint32_t>> id_staging;
		id_staging.reserve(motion.size());
		std::uint32_t body_idx = 0;
		for (motion_component& mc : motion) {
			id_staging.emplace_back(mc.owner_id(), body_idx++);
			motion_ptrs.push_back(std::addressof(mc));
		}
		id_to_body_index.assign_unsorted(std::move(id_staging));
	}

	const auto objects = collect_collision_objects(motion, collision);

	const int total_substeps = steps * substeps;
	for (int step = 0; step < total_substeps; ++step) {
		std::vector<vbd::body_state> bodies;
		bodies.reserve(motion.size());

		for (motion_component& mc : motion) {
			const auto eid = mc.owner_id();
			const auto sc_it = ud.sleep_counters.find(eid);
			const auto sc = sc_it != ud.sleep_counters.end() ? sc_it->second : 0u;

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
			if (!cc.resolve_collisions) {
				continue;
			}

			cc.collision_information = {
				.colliding = false,
				.collision_normal = {},
				.penetration = {},
				.collision_points = {}
			};
		}

		ud.vbd_solver.begin_frame(bodies, ud.contact_cache);
		add_scene_contacts_to_solver(ud.vbd_solver, ud.contact_cache, objects, id_to_body_index, true);

		for (motion_component& mc : motion) {
			if (!mc.velocity_drive_active) {
				continue;
			}
			if (mc.airborne) {
				continue;
			}
			const auto it = id_to_body_index.find(mc.owner_id());
			if (it == id_to_body_index.end()) {
				continue;
			}

			auto& solver_body = ud.vbd_solver.body_states()[it->second];
			if (solver_body.sleeping() && magnitude(mc.velocity_drive_target) > meters_per_second(.01f)) {
				solver_body.sleep_counter = 0;
			}

			ud.vbd_solver.add_motor_constraint(vbd::velocity_motor_constraint{
				.body_index = it->second,
				.target_velocity = mc.velocity_drive_target,
				.compliance = 0.5f,
				.max_force = mc.mass * meters_per_second_squared(50.f),
				.horizontal_only = true,
			});
		}

		for (auto& jd : s.joints) {
			const auto it_a = id_to_body_index.find(jd.entity_a);
			const auto it_b = id_to_body_index.find(jd.entity_b);
			if (it_a == id_to_body_index.end() || it_b == id_to_body_index.end()) {
				continue;
			}

			if (!jd.rest_orientation_initialized && jd.type != vbd::joint_type::distance) {
				jd.rest_orientation = bodies[it_b->second].orientation * conjugate(bodies[it_a->second].orientation);
				jd.rest_orientation_initialized = true;
			}

			ud.vbd_solver.add_joint_constraint(vbd::joint_constraint{
				.body_a = it_a->second,
				.body_b = it_b->second,
				.type = jd.type,
				.local_anchor_a = jd.local_anchor_a,
				.local_anchor_b = jd.local_anchor_b,
				.local_axis_a = jd.local_axis_a,
				.local_axis_b = jd.local_axis_b,
				.target_distance = jd.target_distance,
				.compliance = jd.compliance,
				.damping = jd.damping,
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

		ud.vbd_solver.solve(sub_dt);

		{
			std::uint32_t ji = 0;
			const auto& solved_joints = ud.vbd_solver.graph().joint_constraints();
			for (auto& jd : s.joints) {
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
		ud.vbd_solver.end_frame(result_bodies, ud.contact_cache);

		for (std::size_t i = 0; i < motion_ptrs.size(); ++i) {
			auto* mc = motion_ptrs[i];
			const auto& bs = result_bodies[i];

			mc->current_position = bs.position;
			mc->current_velocity = bs.body_velocity;
			if (mc->update_orientation) {
				mc->orientation = bs.orientation;
				mc->angular_velocity = bs.body_angular_velocity;
			}

			ud.sleep_counters[mc->owner_id()] = bs.sleep_counter;
			mc->sleeping = bs.sleeping();

			if (auto* cc = collision.find(mc->owner_id())) {
				cc->bounding_box.update(mc->current_position, mc->orientation);
			}
		}
	}
}

auto gse::physics::system::frame(frame_context& ctx, frame_data& fd, const state& s) -> async::task<> {
	if (!ctx.try_get<gpu::context>() || !s.use_gpu_solver) {
		co_return;
	}
	if (!fd.gpu_solver.compute_initialized()) {
		co_return;
	}

	fd.gpu_solver.stage_readback();

	if (fd.gpu_solver.has_readback_data()) {
		if (fd.in_flight.has_value()) {
			gpu_readback_result result;
			result.entity_ids = std::move(fd.in_flight->entity_ids);
			result.gpu_input_bodies = std::move(fd.in_flight->gpu_input_bodies);
			result.gpu_result_bodies = result.gpu_input_bodies;
			result.gpu_joint_count = fd.in_flight->gpu_joint_count;
			std::vector<vbd::joint_constraint> joint_readback(result.gpu_joint_count);
			fd.gpu_solver.readback(result.gpu_result_bodies, result.gpu_contacts, joint_readback);
			result.gpu_joint_readback = std::move(joint_readback);
			fd.in_flight.reset();
			ctx.channels.push(std::move(result));
		}
		else {
			thread_local std::vector<vbd::body_state> discard_bodies;
			thread_local std::vector<vbd::contact_readback_entry> discard_contacts;
			thread_local std::vector<vbd::joint_constraint> discard_joints;
			discard_bodies.clear();
			discard_contacts.clear();
			discard_joints.clear();
			fd.gpu_solver.readback(discard_bodies, discard_contacts, discard_joints);
		}
	}

	if (const auto& uploads = ctx.read_channel<gpu_upload_payload>(); !uploads.empty()) {
		const auto& [bodies, collision_data, accel_weights, motors, joints, warm_starts, authoritative_body_indices, solver_cfg, dt, steps, entity_ids, joint_count] = uploads[0];

		fd.gpu_solver.upload(
			bodies,
			collision_data,
			accel_weights,
			motors,
			joints,
			warm_starts,
			authoritative_body_indices,
			solver_cfg,
			dt,
			steps
		);

		fd.in_flight = frame_data::readback_frame{
			.entity_ids = entity_ids,
			.gpu_input_bodies = bodies,
			.gpu_joint_count = joint_count
		};
	}

	if (fd.gpu_solver.pending_dispatch() && fd.gpu_solver.ready_to_dispatch()) {
		fd.gpu_solver.commit_upload();
		fd.gpu_solver.dispatch_compute();
	}

	ctx.channels.push(gpu_solver_stats{
		.active = true,
		.contact_count = fd.gpu_solver.contact_count(),
		.motor_count = fd.gpu_solver.motor_count(),
		.solve_time = fd.gpu_solver.solve_time(),
	});

	const auto snap_slot = fd.gpu_solver.latest_snapshot_slot();
	const auto& layout = fd.gpu_solver.body_layout();

	ctx.channels.push(gpu_solver_frame_info{
		.snapshot = &fd.gpu_solver.snapshot_buffer(snap_slot),
		.semaphore = fd.gpu_solver.compute_semaphore(),
		.body_count = fd.gpu_solver.body_count(),
		.body_stride = layout.stride,
		.position_offset = layout["position"]
	});
}

