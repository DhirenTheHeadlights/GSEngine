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

#ifndef GSE_GPU_COMPARE
#define GSE_GPU_COMPARE 0
#endif

export namespace gse::physics {
	struct state {
		time_t<float, seconds> accumulator{};
		time_t<float, seconds> gpu_interp_accumulator{};
		bool update_phys = true;
		bool use_gpu_solver = false;
		gpu::context* gpu_ctx = nullptr;

		state() = default;
		explicit state(gpu::context& ctx) : gpu_ctx(&ctx) {}

		vbd::solver vbd_solver;
		mutable vbd::gpu_solver gpu_solver;
		vbd::contact_cache contact_cache;
		std::unordered_map<id, std::uint32_t> sleep_counters;

		struct gpu_prev_frame {
			std::vector<vbd::body_state> result_bodies;
			std::vector<id> result_entity_ids;
			std::vector<vbd::warm_start_entry> warm_start_contacts;
		} gpu_prev;

#if GSE_GPU_COMPARE
		mutable struct gpu_compare_state {
			bool enabled = true;
			std::uint64_t next_sequence = 1;
			vbd::solver cpu_reference_solver;

			struct contact_snapshot {
				std::uint32_t body_a = 0;
				std::uint32_t body_b = 0;
				std::uint64_t feature_key = 0;
				unitless::vec3 normal = {};
				unitless::vec3 tangent_u = {};
				unitless::vec3 tangent_v = {};
				vec3<length> local_anchor_a = {};
				vec3<length> local_anchor_b = {};
				std::array<float, 3> c0 = {};
				std::array<float, 3> lambda = {};
				std::array<float, 3> penalty = {};
				float friction_coeff = 0.f;
			};

			struct snapshot {
				std::uint64_t sequence = 0;
				std::uint32_t substeps = 1;
				std::size_t warm_start_count = 0;
				std::vector<id> entity_ids;
				std::vector<vbd::body_state> gpu_input_bodies;
				std::vector<vbd::body_state> gpu_result_bodies;
				std::vector<vbd::contact_readback_entry> gpu_contacts;
				std::vector<vbd::narrow_phase_debug_entry> gpu_narrow_phase_debug;
				std::vector<vbd::body_state> cpu_result_bodies;
				std::vector<contact_snapshot> cpu_contacts;
			};

			std::optional<snapshot> pending;
			per_frame_resource<std::optional<snapshot>> in_flight;
			std::deque<snapshot> completed;
		} gpu_compare;
#else
		mutable struct gpu_readback_state {
			struct snapshot {
				std::vector<id> entity_ids;
				std::vector<vbd::body_state> gpu_input_bodies;
				std::vector<vbd::body_state> gpu_result_bodies;
				std::vector<vbd::contact_readback_entry> gpu_contacts;
			};

			std::optional<snapshot> pending;
			per_frame_resource<std::optional<snapshot>> in_flight;
			std::deque<snapshot> completed;
		} gpu_readback;
#endif
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
				.lambda = { c.lambda[0], c.lambda[1], c.lambda[2] },
				.penalty = { c.penalty[0], c.penalty[1], c.penalty[2] },
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

#if GSE_GPU_COMPARE
	struct compare_collision_object {
		std::uint32_t body_index = 0;
		bounding_box box;
	};
#endif

#if GSE_GPU_COMPARE
	auto to_local_anchor(const quat& q, const vec3<length>& v) -> vec3<length> {
		const unitless::vec3 v_unitless = v.as<meters>();
		const unitless::vec3 rotated = mat3_cast(conjugate(q)) * v_unitless;
		return rotated * meters(1.f);
	}

	auto quaternion_angle_difference(const quat& a, const quat& b) -> float {
		const quat dq = normalize(a * conjugate(b));
		return 2.f * std::acos(std::clamp(std::abs(dq.s()), 0.f, 1.f));
	}

	auto build_cpu_reference_snapshot(
		state& s,
		chunk<collision_component>& collision,
		const std::unordered_map<id, std::uint32_t>& id_to_body_index,
		const std::vector<vbd::body_state>& bodies,
		const std::vector<id>& entity_ids,
		const std::vector<vbd::velocity_motor_constraint>& motors,
		const std::span<const vbd::warm_start_entry> warm_start_contacts,
		const std::span<const vec3<velocity>> previous_velocities,
		const time_t<float, seconds> dt,
		const int steps,
		const std::uint64_t sequence
	) -> state::gpu_compare_state::snapshot {
		state::gpu_compare_state::snapshot snapshot;
		snapshot.sequence = sequence;
		snapshot.substeps = static_cast<std::uint32_t>(std::max(steps, 1));
		snapshot.warm_start_count = warm_start_contacts.size();
		snapshot.entity_ids = entity_ids;

		auto& solver = s.gpu_compare.cpu_reference_solver;
		solver.configure(s.vbd_solver.config());
		solver.seed_previous_velocities(previous_velocities);

		auto cache = build_contact_cache_from_warm_start(warm_start_contacts);

		std::vector<vbd::body_state> step_bodies = bodies;
		std::vector<compare_collision_object> objects;
		objects.reserve(collision.size());
		for (collision_component& cc : collision) {
			if (!cc.resolve_collisions) continue;
			const auto it = id_to_body_index.find(cc.owner_id());
			if (it == id_to_body_index.end()) continue;
			objects.push_back({
				.body_index = it->second,
				.box = cc.bounding_box
			});
		}

		const int total_steps = std::max(steps, 1);
		for (int step = 0; step < total_steps; ++step) {
			for (auto& object : objects) {
				if (object.body_index >= step_bodies.size()) continue;
				const auto& body = step_bodies[object.body_index];
				object.box.update(body.position, body.orientation);
			}

			solver.begin_frame(step_bodies, cache);

			for (std::size_t i = 0; i < objects.size(); ++i) {
				for (std::size_t j = i + 1; j < objects.size(); ++j) {
					auto& object_a = objects[i];
					auto& object_b = objects[j];

					const auto& aabb_a = object_a.box.aabb();
					const auto& aabb_b = object_b.box.aabb();
					if (!aabb_a.overlaps(aabb_b, solver.config().speculative_margin)) continue;

					auto sat_result = narrow_phase_collision::sat_speculative(
						object_a.box,
						object_b.box,
						solver.config().speculative_margin
					);
					if (!sat_result) continue;

					auto& sat = *sat_result;
					if (dot(sat.normal, object_b.box.center() - object_a.box.center()) < meters(0.f)) {
						sat.normal = -sat.normal;
					}

					auto manifold = narrow_phase_collision::generate_manifold(
						object_a.box,
						object_b.box,
						sat.normal,
						sat.separation
					);
					if (manifold.point_count == 0) continue;

					const std::uint32_t body_a = object_a.body_index;
					const std::uint32_t body_b = object_b.body_index;
					const auto& cfg = solver.config();
					const unitless::vec3 constraint_normal = -sat.normal;

					const auto& bs_a = solver.body_states()[body_a];
					const auto& bs_b = solver.body_states()[body_b];
					const float penalty_floor = cfg.penalty_min;

					for (std::uint32_t p = 0; p < manifold.point_count; ++p) {
						const auto& [position_on_a, position_on_b, normal, separation, feature] = manifold.points[p];

						vec3<length> local_r_a = to_local_anchor(bs_a.orientation, position_on_a - bs_a.position);
						vec3<length> local_r_b = to_local_anchor(bs_b.orientation, position_on_b - bs_b.position);

						auto cached = cache.lookup(body_a, body_b, feature);
						const unitless::vec3 current_d = (position_on_a - position_on_b).as<meters>();
						const float current_normal_gap = dot(constraint_normal, current_d) + cfg.collision_margin.as<meters>();
						const bool reuse_cached_normal =
							cached &&
							(cached->lambda[0] < -1e-3f || current_normal_gap < -1e-4f);
						const bool reuse_cached_tangent =
							reuse_cached_normal &&
							cached.has_value();
						const bool reuse_cached_sticking =
							reuse_cached_tangent &&
							cached->sticking;

						float init_lambda[3] = {};
						float init_penalty[3] = { penalty_floor, penalty_floor, penalty_floor };

						if (reuse_cached_normal) {
							init_penalty[0] = std::max(cached->penalty[0], penalty_floor);

							const unitless::vec3 cached_normal_force = cached->normal * cached->lambda[0];
							init_lambda[0] = std::min(dot(cached_normal_force, constraint_normal), 0.f);
						}

						if (reuse_cached_tangent) {
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
						}

						if (reuse_cached_sticking) {
							local_r_a = cached->local_anchor_a;
							local_r_b = cached->local_anchor_b;
						}

						solver.add_contact_constraint(vbd::contact_constraint{
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
					}
				}
			}

			for (const auto& motor : motors) {
				solver.add_motor_constraint(motor);
			}

			solver.solve(dt);

			if (step + 1 == total_steps) {
				const auto& contacts = solver.graph().contact_constraints();
				snapshot.cpu_contacts.reserve(contacts.size());
				for (const auto& c : contacts) {
					snapshot.cpu_contacts.push_back(state::gpu_compare_state::contact_snapshot{
						.body_a = c.body_a,
						.body_b = c.body_b,
						.feature_key = pack_feature(c.feature),
						.normal = c.normal,
						.tangent_u = c.tangent_u,
						.tangent_v = c.tangent_v,
						.local_anchor_a = c.r_a,
						.local_anchor_b = c.r_b,
						.c0 = { c.C0[0], c.C0[1], c.C0[2] },
						.lambda = { c.lambda[0], c.lambda[1], c.lambda[2] },
						.penalty = { c.penalty[0], c.penalty[1], c.penalty[2] },
						.friction_coeff = c.friction_coeff
					});
				}
			}

			solver.end_frame(step_bodies, cache);
		}

		snapshot.cpu_result_bodies = step_bodies;
		return snapshot;
	}

	auto log_gpu_cpu_deviations(
		const state::gpu_compare_state::snapshot& snapshot,
		const std::span<const vbd::contact_readback_entry> gpu_contacts,
		const std::span<const vbd::narrow_phase_debug_entry> gpu_narrow_phase_debug
	) -> void {
		const auto body_count = std::min(snapshot.cpu_result_bodies.size(), snapshot.gpu_result_bodies.size());
		if (body_count == 0) {
			return;
		}

		constexpr float pos_threshold = 0.02f;
		constexpr float vel_threshold = 0.25f;
		constexpr float rot_threshold = 0.05f;
		constexpr float ang_vel_threshold = 0.5f;
		constexpr float lambda_threshold = 5.f;
		constexpr float c0_threshold = 0.01f;
		constexpr float normal_threshold = 0.02f;
		constexpr std::size_t max_detail_logs = 8;

		float max_pos_error = 0.f;
		float max_vel_error = 0.f;
		float max_rot_error = 0.f;
		float max_ang_vel_error = 0.f;
		std::size_t max_pos_body = 0;
		std::size_t max_vel_body = 0;
		std::size_t max_rot_body = 0;
		std::size_t max_ang_vel_body = 0;
		std::size_t large_body_errors = 0;

		for (std::size_t i = 0; i < body_count; ++i) {
			const auto& cpu = snapshot.cpu_result_bodies[i];
			const auto& gpu = snapshot.gpu_result_bodies[i];

			const float pos_error = magnitude((gpu.position - cpu.position).as<meters>());
			const float vel_error = magnitude((gpu.body_velocity - cpu.body_velocity).as<meters_per_second>());
			const float rot_error = quaternion_angle_difference(gpu.orientation, cpu.orientation);
			const float ang_vel_error = magnitude((gpu.body_angular_velocity - cpu.body_angular_velocity).as<radians_per_second>());

			if (pos_error > max_pos_error) { max_pos_error = pos_error; max_pos_body = i; }
			if (vel_error > max_vel_error) { max_vel_error = vel_error; max_vel_body = i; }
			if (rot_error > max_rot_error) { max_rot_error = rot_error; max_rot_body = i; }
			if (ang_vel_error > max_ang_vel_error) { max_ang_vel_error = ang_vel_error; max_ang_vel_body = i; }

			if (pos_error > pos_threshold || vel_error > vel_threshold || rot_error > rot_threshold || ang_vel_error > ang_vel_threshold) {
				large_body_errors++;
			}
		}

		std::unordered_map<contact_compare_key, const vbd::contact_readback_entry*, contact_compare_key_hash> gpu_contact_map;
		gpu_contact_map.reserve(gpu_contacts.size());
		for (const auto& c : gpu_contacts) {
			gpu_contact_map.insert_or_assign(contact_compare_key{
				.body_a = c.body_a,
				.body_b = c.body_b,
				.feature_key = c.feature_key
			}, std::addressof(c));
		}

		const auto pack_body_pair = [](const std::uint32_t body_a, const std::uint32_t body_b) {
			return (static_cast<std::uint64_t>(body_a) << 32) | static_cast<std::uint64_t>(body_b);
		};

		std::unordered_map<std::uint64_t, const vbd::narrow_phase_debug_entry*> gpu_debug_by_pair;
		gpu_debug_by_pair.reserve(gpu_narrow_phase_debug.size());
		for (const auto& debug_entry : gpu_narrow_phase_debug) {
			gpu_debug_by_pair.emplace(pack_body_pair(debug_entry.body_a, debug_entry.body_b), std::addressof(debug_entry));
		}

		std::size_t missing_gpu_contacts = 0;
		std::size_t extra_gpu_contacts = 0;
		float max_lambda_error = 0.f;
		float max_c0_error = 0.f;
		float max_normal_error = 0.f;
		std::size_t contact_detail_logs = 0;

		std::unordered_set<contact_compare_key, contact_compare_key_hash> matched_keys;
		matched_keys.reserve(snapshot.cpu_contacts.size());
		for (const auto& c : snapshot.cpu_contacts) {
			const contact_compare_key key{
				.body_a = c.body_a,
				.body_b = c.body_b,
				.feature_key = c.feature_key
			};

			const auto it = gpu_contact_map.find(key);
			if (it == gpu_contact_map.end()) {
				missing_gpu_contacts++;
				if (contact_detail_logs < max_detail_logs) {
					log::println(
						"  missing gpu contact: bodies=({}, {}) feature={:#018x}",
						c.body_a,
						c.body_b,
						c.feature_key
					);
					contact_detail_logs++;
				}
				continue;
			}

			matched_keys.insert(key);
			const auto& gpu = *it->second;

			float lambda_error = 0.f;
			float c0_error = 0.f;
			for (int row = 0; row < 3; ++row) {
				lambda_error = std::max(lambda_error, std::abs(gpu.lambda[row] - c.lambda[row]));
				c0_error = std::max(c0_error, std::abs(gpu.c0[row] - c.c0[row]));
			}

			const float normal_error = magnitude(gpu.normal - c.normal);
			max_lambda_error = std::max(max_lambda_error, lambda_error);
			max_c0_error = std::max(max_c0_error, c0_error);
			max_normal_error = std::max(max_normal_error, normal_error);

			if ((lambda_error > lambda_threshold || c0_error > c0_threshold || normal_error > normal_threshold) && contact_detail_logs < max_detail_logs) {
				log::println(
					"  contact diff: bodies=({}, {}) feature={:#018x} lambda_err={:.4f} c0_err={:.4f} normal_err={:.4f} cpu_lambda=({:.3f},{:.3f},{:.3f}) gpu_lambda=({:.3f},{:.3f},{:.3f})",
					c.body_a,
					c.body_b,
					c.feature_key,
					lambda_error,
					c0_error,
					normal_error,
					c.lambda[0],
					c.lambda[1],
					c.lambda[2],
					gpu.lambda[0],
					gpu.lambda[1],
					gpu.lambda[2]
				);
				contact_detail_logs++;
			}
		}

		for (const auto& c : gpu_contacts) {
			const contact_compare_key key{
				.body_a = c.body_a,
				.body_b = c.body_b,
				.feature_key = c.feature_key
			};
			if (!matched_keys.contains(key)) {
				extra_gpu_contacts++;
				if (contact_detail_logs < max_detail_logs) {
					log::println(
						"  extra gpu contact: bodies=({}, {}) feature={:#018x}",
						c.body_a,
						c.body_b,
						c.feature_key
					);
					contact_detail_logs++;
				}
			}
		}

		const bool has_large_contact_error =
			snapshot.cpu_contacts.size() != gpu_contacts.size() ||
			missing_gpu_contacts > 0 ||
			extra_gpu_contacts > 0 ||
			max_lambda_error > lambda_threshold ||
			max_c0_error > c0_threshold ||
			max_normal_error > normal_threshold;

		const bool has_large_body_error =
			large_body_errors > 0 ||
			max_pos_error > pos_threshold ||
			max_vel_error > vel_threshold ||
			max_rot_error > rot_threshold ||
			max_ang_vel_error > ang_vel_threshold;

		if (!has_large_body_error && !has_large_contact_error) {
			return;
		}

		log::println(
			"[GPU Compare] seq={} substeps={} warm_starts={} bodies={} cpu_contacts={} gpu_contacts={} max_pos={:.4f} body={} max_vel={:.4f} body={} max_rot={:.4f} body={} max_ang_vel={:.4f} body={}",
			snapshot.sequence,
			snapshot.substeps,
			snapshot.warm_start_count,
			body_count,
			snapshot.cpu_contacts.size(),
			gpu_contacts.size(),
			max_pos_error,
			max_pos_body,
			max_vel_error,
			max_vel_body,
			max_rot_error,
			max_rot_body,
			max_ang_vel_error,
			max_ang_vel_body
		);
		log::println(
			"  contact_mismatch: missing_gpu={} extra_gpu={} max_lambda={:.4f} max_c0={:.4f} max_normal={:.4f}",
			missing_gpu_contacts,
			extra_gpu_contacts,
			max_lambda_error,
			max_c0_error,
			max_normal_error
		);
		if ((missing_gpu_contacts > 0 || extra_gpu_contacts > 0) && snapshot.substeps > 1) {
			log::println(
				"  hint: contact-set drift with multi-substep solve usually points at GPU contact generation staying stale across substeps or feature-id mismatch"
			);
		}
		else if (missing_gpu_contacts == 0 && extra_gpu_contacts == 0 && (max_lambda_error > lambda_threshold || max_c0_error > c0_threshold || max_normal_error > normal_threshold)) {
			log::println(
				"  hint: contacts match but rows diverge, so focus on solve/update_lambda semantics rather than narrow phase"
			);
		}
		else if (missing_gpu_contacts == 0 && extra_gpu_contacts == 0 && max_lambda_error <= lambda_threshold && max_c0_error <= c0_threshold && max_normal_error <= normal_threshold) {
			log::println(
				"  hint: contacts match, so focus on predict/derive/finalize body updates"
			);
		}

		std::size_t debug_pair_logs = 0;
		std::unordered_set<std::uint64_t> logged_debug_pairs;
		logged_debug_pairs.reserve(8);
		for (const auto& c : snapshot.cpu_contacts) {
			const contact_compare_key key{
				.body_a = c.body_a,
				.body_b = c.body_b,
				.feature_key = c.feature_key
			};
			if (gpu_contact_map.contains(key)) {
				continue;
			}

			const std::uint64_t pair_key = pack_body_pair(c.body_a, c.body_b);
			if (logged_debug_pairs.contains(pair_key)) {
				continue;
			}

			const auto debug_it = gpu_debug_by_pair.find(pair_key);
			if (debug_it == gpu_debug_by_pair.end()) {
				continue;
			}

			const auto& debug_entry = *debug_it->second;
			log::println(
				"  gpu narrow-phase: bodies=({}, {}) clipped_valid={} clipped_verts={} raw={} unique={} result={} fallback={} speculative={} ref_is_a={} faces=({}, {})",
				debug_entry.body_a,
				debug_entry.body_b,
				debug_entry.clipped_valid ? 1 : 0,
				debug_entry.clipped_vertices,
				debug_entry.raw_candidates,
				debug_entry.unique_candidates,
				debug_entry.result_count,
				debug_entry.used_fallback ? 1 : 0,
				debug_entry.speculative ? 1 : 0,
				debug_entry.reference_is_a ? 1 : 0,
				debug_entry.reference_face,
				debug_entry.incident_face
			);
			logged_debug_pairs.insert(pair_key);
			debug_pair_logs++;
			if (debug_pair_logs >= 4) {
				break;
			}
		}

		if (debug_pair_logs == 0 && !gpu_narrow_phase_debug.empty()) {
			const std::size_t fallback_debug_logs = std::min<std::size_t>(gpu_narrow_phase_debug.size(), 3);
			for (std::size_t i = 0; i < fallback_debug_logs; ++i) {
				const auto& debug_entry = gpu_narrow_phase_debug[i];
				log::println(
					"  gpu narrow-phase: bodies=({}, {}) clipped_valid={} clipped_verts={} raw={} unique={} result={} fallback={} speculative={} ref_is_a={} faces=({}, {})",
					debug_entry.body_a,
					debug_entry.body_b,
					debug_entry.clipped_valid ? 1 : 0,
					debug_entry.clipped_vertices,
					debug_entry.raw_candidates,
					debug_entry.unique_candidates,
					debug_entry.result_count,
					debug_entry.used_fallback ? 1 : 0,
					debug_entry.speculative ? 1 : 0,
					debug_entry.reference_is_a ? 1 : 0,
					debug_entry.reference_face,
					debug_entry.incident_face
				);
			}
		}

		std::size_t detail_logs = 0;
		for (std::size_t i = 0; i < body_count && detail_logs < max_detail_logs; ++i) {
			const auto& cpu = snapshot.cpu_result_bodies[i];
			const auto& gpu = snapshot.gpu_result_bodies[i];

			const float pos_error = magnitude((gpu.position - cpu.position).as<meters>());
			const float vel_error = magnitude((gpu.body_velocity - cpu.body_velocity).as<meters_per_second>());
			const float rot_error = quaternion_angle_difference(gpu.orientation, cpu.orientation);
			const float ang_vel_error = magnitude((gpu.body_angular_velocity - cpu.body_angular_velocity).as<radians_per_second>());

			if (pos_error <= pos_threshold && vel_error <= vel_threshold && rot_error <= rot_threshold && ang_vel_error <= ang_vel_threshold) {
				continue;
			}

			log::println(
				"  body {}: pos_err={:.4f} vel_err={:.4f} rot_err={:.4f} ang_vel_err={:.4f} cpu_pos=({:.3f},{:.3f},{:.3f}) gpu_pos=({:.3f},{:.3f},{:.3f})",
				i,
				pos_error,
				vel_error,
				rot_error,
				ang_vel_error,
				cpu.position.x().as<meters>(),
				cpu.position.y().as<meters>(),
				cpu.position.z().as<meters>(),
				gpu.position.x().as<meters>(),
				gpu.position.y().as<meters>(),
				gpu.position.z().as<meters>()
			);
			detail_logs++;
		}

		log::flush();
	}
#endif

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
		const int gpu_steps = std::min(steps, 1);
		s.gpu_interp_accumulator += frame_time;
		const float gpu_alpha = std::clamp(s.gpu_interp_accumulator / const_update_time, 0.f, 1.f);
		phase.schedule([gpu_steps, gpu_alpha, &s, const_update_time](chunk<motion_component> motion, chunk<collision_component> collision) {
			update_vbd_gpu(gpu_steps, s, motion, collision, const_update_time);

			for (motion_component& mc : motion) {
				if (mc.position_locked) {
					mc.render_position = mc.current_position;
					mc.render_orientation = mc.orientation;
				}
				else {
					mc.render_position = lerp(mc.previous_position, mc.current_position, gpu_alpha);
					mc.render_orientation = slerp(mc.previous_orientation, mc.orientation, gpu_alpha);
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

#if GSE_GPU_COMPARE
	if (!s.gpu_compare.completed.empty()) {
		auto completed = std::move(s.gpu_compare.completed.front());
		s.gpu_compare.completed.pop_front();

		if (s.gpu_compare.enabled) {
			log_gpu_cpu_deviations(completed, completed.gpu_contacts, completed.gpu_narrow_phase_debug);
		}
#else
	if (!s.gpu_readback.completed.empty()) {
		auto completed = std::move(s.gpu_readback.completed.front());
		s.gpu_readback.completed.pop_front();
#endif

		s.gpu_interp_accumulator = fmod(s.gpu_interp_accumulator, dt);

		for (std::size_t i = 0; i < completed.entity_ids.size(); ++i) {
			const auto eid = completed.entity_ids[i];
			auto* mc = motion.find(eid);
			if (!mc) continue;

			const auto& bs = completed.gpu_result_bodies[i];

			if (!mc->position_locked) {
				mc->previous_position = mc->current_position;
				mc->previous_orientation = mc->orientation;
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

		const auto& cfg = s.vbd_solver.config();
		for (const auto& c : completed.gpu_contacts) {
			const auto body_a = c.body_a;
			const auto body_b = c.body_b;
			if (body_a >= completed.entity_ids.size() || body_b >= completed.entity_ids.size()) continue;

			const auto eid_a = completed.entity_ids[body_a];
			const auto eid_b = completed.entity_ids[body_b];
			const auto& normal = c.normal;
			const float separation = c.c0[0];

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
				cc_a->collision_information.penetration = meters(-separation);
				cc_a->collision_information.collision_points.push_back(midpoint);
			}
			if (auto* cc_b = collision.find(eid_b)) {
				cc_b->collision_information.colliding = true;
				cc_b->collision_information.collision_normal = -normal;
				cc_b->collision_information.penetration = meters(-separation);
				cc_b->collision_information.collision_points.push_back(midpoint);
			}

			const float friction_bound = std::abs(c.lambda[0]) * c.friction_coeff;
			const float tangential_lambda = std::hypot(c.lambda[1], c.lambda[2]);
			const float tangential_gap = std::hypot(c.c0[1], c.c0[2]);
			const bool sticking =
				c.lambda[0] < -1e-3f &&
				tangential_gap < cfg.stick_threshold.as<meters>() &&
				tangential_lambda < friction_bound;

			s.contact_cache.store(body_a, body_b, unpack_feature(c.feature_key), vbd::cached_lambda{
				.lambda = { c.lambda[0], c.lambda[1], c.lambda[2] },
				.penalty = { c.penalty[0], c.penalty[1], c.penalty[2] },
				.normal = c.normal,
				.tangent_u = c.tangent_u,
				.tangent_v = c.tangent_v,
				.local_anchor_a = c.local_anchor_a,
				.local_anchor_b = c.local_anchor_b,
				.sticking = sticking,
				.age = 0
			});
		}

		s.gpu_prev.warm_start_contacts.clear();
		s.gpu_prev.warm_start_contacts.reserve(completed.gpu_contacts.size());
		for (const auto& c : completed.gpu_contacts) {
			const float friction_bound = std::abs(c.lambda[0]) * c.friction_coeff;
			const float tangential_lambda = std::hypot(c.lambda[1], c.lambda[2]);
			const float tangential_gap = std::hypot(c.c0[1], c.c0[2]);
			const bool sticking =
				c.lambda[0] < -1e-3f &&
				tangential_gap < cfg.stick_threshold.as<meters>() &&
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
				.lambda = { c.lambda[0], c.lambda[1], c.lambda[2] },
				.penalty = { c.penalty[0], c.penalty[1], c.penalty[2] }
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
	refresh_airborne_from_collisions(s, motion, collision);

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
		if (!mc.position_locked && sc < 300u && dt_s > 1e-6f) {
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
			s.contact_cache.remove_body(it->second);
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
			.lambda = { 0.f, 0.f, 0.f }
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

#if GSE_GPU_COMPARE
	const std::uint64_t sequence = s.gpu_compare.next_sequence++;
	if (s.gpu_compare.enabled) {
		std::vector<vec3<velocity>> previous_velocities;
		previous_velocities.reserve(entity_ids.size());
		for (std::size_t i = 0; i < entity_ids.size(); ++i) {
			if (const auto prev_it = prev_gpu_velocity.find(entity_ids[i]); prev_it != prev_gpu_velocity.end()) {
				previous_velocities.push_back(prev_it->second);
			}
			else {
				previous_velocities.push_back(bodies[i].body_velocity);
			}
		}

		s.gpu_compare.pending = build_cpu_reference_snapshot(
			s,
			collision,
			id_to_body_index,
			bodies,
			entity_ids,
			motors,
			s.gpu_prev.warm_start_contacts,
			previous_velocities,
			dt,
			steps,
			sequence
		);
		s.gpu_compare.pending->gpu_input_bodies = bodies;
	}
	else {
		s.gpu_compare.pending = state::gpu_compare_state::snapshot{
			.sequence = sequence,
			.substeps = static_cast<std::uint32_t>(std::max(steps, 1)),
			.warm_start_count = s.gpu_prev.warm_start_contacts.size(),
			.entity_ids = entity_ids,
			.gpu_input_bodies = bodies
		};
	}
#else
	s.gpu_readback.pending = state::gpu_readback_state::snapshot{
		.entity_ids = entity_ids,
		.gpu_input_bodies = bodies
	};
#endif
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
					const unitless::vec3 current_d = (position_on_a - position_on_b).as<meters>();
					const float current_normal_gap = dot(constraint_normal, current_d) + cfg.collision_margin.as<meters>();
					const bool reuse_cached_normal =
						cached &&
						(cached->lambda[0] < -1e-3f || current_normal_gap < -1e-4f);
					const bool reuse_cached_tangent =
						reuse_cached_normal &&
						cached.has_value();
					const bool reuse_cached_sticking =
						reuse_cached_tangent &&
						cached->sticking;

					float init_lambda[3] = {};
					float init_penalty[3] = { penalty_floor, penalty_floor, penalty_floor };

					if (reuse_cached_normal) {
						init_penalty[0] = std::max(cached->penalty[0], penalty_floor);

						const unitless::vec3 cached_normal_force = cached->normal * cached->lambda[0];
						init_lambda[0] = std::min(dot(cached_normal_force, constraint_normal), 0.f);
					}

					if (reuse_cached_tangent) {
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
					}

					if (reuse_cached_sticking) {
						bool reused_anchors = false;
						local_r_a = cached->local_anchor_a;
						local_r_b = cached->local_anchor_b;
						reused_anchors = true;
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

	auto& config = s.gpu_ctx->config();
	if (!config.frame_in_progress()) return;

	const auto command = config.frame_context().command_buffer;
	const auto frame_index = config.current_frame();

	s.gpu_solver.stage_readback(frame_index);
	if (s.gpu_solver.has_readback_data()) {
#if GSE_GPU_COMPARE
		auto& in_flight = s.gpu_compare.in_flight[frame_index];
		if (in_flight.has_value()) {
			auto completed = std::move(*in_flight);
			in_flight.reset();
			completed.gpu_result_bodies = completed.gpu_input_bodies;
			s.gpu_solver.readback(completed.gpu_result_bodies, completed.gpu_contacts);
			const auto gpu_narrow_phase_debug = s.gpu_solver.narrow_phase_debug_entries();
			completed.gpu_narrow_phase_debug.assign(gpu_narrow_phase_debug.begin(), gpu_narrow_phase_debug.end());
			s.gpu_compare.completed.push_back(std::move(completed));
		}
		else {
			std::vector<vbd::body_state> discard_bodies;
			std::vector<vbd::contact_readback_entry> discard_contacts;
			s.gpu_solver.readback(discard_bodies, discard_contacts);
			log::println("[GPU Compare] readback staged without an in-flight snapshot for frame={}", frame_index);
			log::flush();
		}
#else
		auto& in_flight = s.gpu_readback.in_flight[frame_index];
		if (in_flight.has_value()) {
			auto completed = std::move(*in_flight);
			in_flight.reset();
			completed.gpu_result_bodies = completed.gpu_input_bodies;
			s.gpu_solver.readback(completed.gpu_result_bodies, completed.gpu_contacts);
			s.gpu_readback.completed.push_back(std::move(completed));
		}
		else {
			std::vector<vbd::body_state> discard_bodies;
			std::vector<vbd::contact_readback_entry> discard_contacts;
			s.gpu_solver.readback(discard_bodies, discard_contacts);
		}
#endif
	}

	if (s.gpu_solver.pending_dispatch() && s.gpu_solver.ready_to_dispatch(frame_index)) {
#if GSE_GPU_COMPARE
		if (s.gpu_compare.pending.has_value()) {
			s.gpu_compare.in_flight[frame_index] = std::move(s.gpu_compare.pending);
			s.gpu_compare.pending.reset();
		}
		else {
			s.gpu_compare.in_flight[frame_index].reset();
		}
#else
		if (s.gpu_readback.pending.has_value()) {
			s.gpu_readback.in_flight[frame_index] = std::move(s.gpu_readback.pending);
			s.gpu_readback.pending.reset();
		}
		else {
			s.gpu_readback.in_flight[frame_index].reset();
		}
#endif

		s.gpu_solver.commit_upload(frame_index);
		s.gpu_solver.dispatch_compute(command, config, frame_index);
	}
}
