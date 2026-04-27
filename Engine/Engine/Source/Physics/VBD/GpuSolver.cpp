module gse.physics;

import std;

import :vbd_gpu_solver;
import :vbd_constraints;
import :vbd_solver;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.log;

namespace gse::vbd {
	class struct_writer {
	public:
		struct_writer(std::byte* base) : m_base(base) {
		}

		template<typename T>
		auto write(
			const std::size_t offset,
			const T& value
		) -> void {
			gse::memcpy(m_base + offset, value);
		}

		auto write_index(
			const std::size_t offset,
			const std::uint32_t index,
			std::uint32_t max,
			const std::source_location& loc = std::source_location::current()
		) -> void {
			assert(index < max, loc, "Buffer index {} out of bounds (max {})", index, max);
			write(offset, index);
		}

	private:
		std::byte* m_base;
	};

	class struct_reader {
	public:
		struct_reader(const std::byte* base) : m_base(base) {
		}

		template<typename T>
		auto read(
			const std::size_t offset,
			T& dest
		) const -> void {
			gse::memcpy(dest, m_base + offset);
		}

		auto read_raw(
			const std::size_t src_offset,
			std::byte* dst,
			const std::size_t size
		) const -> void {
			gse::memcpy(dst, m_base + src_offset, size);
		}

	private:
		const std::byte* m_base;
	};
}

gse::vbd::gpu_solver::~gpu_solver() {
	if (m_compute.initialized) {
		for (auto& f : m_frames) {
			f.queue.wait();
		}
	}
}

auto gse::vbd::gpu_solver::create_buffers(gpu::context& ctx) -> void {
	constexpr auto storage_src = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_src;
	constexpr auto storage_dst = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst;
	constexpr auto storage_src_dst = storage_src | gpu::buffer_flag::transfer_dst;
	constexpr std::size_t color_buffer_size = max_colors * sizeof(std::uint32_t) * 2 + max_bodies * sizeof(std::uint32_t);
	constexpr std::size_t collision_pair_size = sizeof(std::uint32_t) + max_collision_pairs * 2 * sizeof(std::uint32_t);
	constexpr std::size_t collision_state_size = collision_state_uints * sizeof(std::uint32_t);
	const std::size_t warm_start_size = max_contacts * m_warm_start_layout.stride;
	const std::size_t joint_buffer_size = max_joints * m_joint_layout.stride;
	const std::size_t readback_size =
		max_bodies * m_body_layout.stride +
		max_contacts * m_contact_layout.stride +
		collision_state_uints * sizeof(std::uint32_t) +
		max_joints * m_joint_layout.stride;

	for (auto& f : m_frames) {
		f.body_buffer = gpu::create_buffer(ctx, {
			.size = max_bodies * m_body_layout.stride,
			.usage = storage_src_dst
		});

		f.contact_buffer = gpu::create_buffer(ctx, {
			.size = max_contacts * m_contact_layout.stride,
			.usage = storage_src
		});

		f.motor_buffer = gpu::create_buffer(ctx, {
			.size = max_motors * m_motor_layout.stride,
			.usage = gpu::buffer_flag::storage
		});

		f.color_buffer = gpu::create_buffer(ctx, {
			.size = color_buffer_size,
			.usage = gpu::buffer_flag::storage
		});

		f.contact_offsets_buffer = gpu::create_buffer(ctx, {
			.size = max_bodies * sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage
		});
		f.contact_counts_buffer = gpu::create_buffer(ctx, {
			.size = max_bodies * sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage
		});
		f.contact_adjacency_buffer = gpu::create_buffer(ctx, {
			.size = max_contacts * 2 * sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage
		});
		f.motor_map_buffer = gpu::create_buffer(ctx, {
			.size = max_bodies * sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage
		});
		f.joint_offsets_buffer = gpu::create_buffer(ctx, {
			.size = max_bodies * sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage
		});
		f.joint_counts_buffer = gpu::create_buffer(ctx, {
			.size = max_bodies * sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage
		});
		f.joint_adjacency_buffer = gpu::create_buffer(ctx, {
			.size = max_joints * 2 * sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage
		});

		f.solve_state_buffer = gpu::create_buffer(ctx, {
			.size = max_bodies * solve_state_float4s_per_body * sizeof(float) * 4,
			.usage = gpu::buffer_flag::storage
		});

		f.collision_pair_buffer = gpu::create_buffer(ctx, {
			.size = collision_pair_size,
			.usage = gpu::buffer_flag::storage
		});

		f.collision_state_buffer = gpu::create_buffer(ctx, {
			.size = collision_state_size,
			.usage = storage_src
		});

		f.warm_start_buffer = gpu::create_buffer(ctx, {
			.size = std::max<std::size_t>(warm_start_size, 16),
			.usage = gpu::buffer_flag::storage
		});

		f.joint_buffer = gpu::create_buffer(ctx, {
			.size = std::max<std::size_t>(joint_buffer_size, 16),
			.usage = storage_src
		});

		constexpr std::size_t grid_buffer_size = (1 + grid_table_size + max_bodies * 8 * 2) * sizeof(std::uint32_t);
		f.grid_buffer = gpu::create_buffer(ctx, {
			.size = grid_buffer_size,
			.usage = gpu::buffer_flag::storage
		});

		f.readback_buffer = gpu::create_buffer(ctx, {
			.size = readback_size,
			.usage = storage_dst
		});
		std::memset(f.readback_buffer.mapped(), 0, f.readback_buffer.size());

		f.physics_snapshot_buffer = gpu::create_buffer(ctx, {
			.size = max_bodies * m_body_layout.stride,
			.usage = storage_dst
		});
		std::memset(f.physics_snapshot_buffer.mapped(), 0, f.physics_snapshot_buffer.size());

		f.indirect_dispatch_buffer = gpu::create_buffer(ctx, {
			.size = (2 + max_colors) * 3 * sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage | gpu::buffer_flag::indirect
		});
		std::memset(f.indirect_dispatch_buffer.mapped(), 0, f.indirect_dispatch_buffer.size());

		f.frozen_jacobian_buffer = gpu::create_buffer(ctx, {
			.size = max_contacts * m_frozen_jacobian_layout.stride,
			.usage = gpu::buffer_flag::storage
		});

		f.solve_deltas_buffer = gpu::create_buffer(ctx, {
			.size = max_bodies * 2 * sizeof(float) * 4,
			.usage = gpu::buffer_flag::storage
		});
	}

	m_upload_body_data.reserve(max_bodies * m_body_layout.stride);
	m_upload_motor_data.reserve(max_motors * m_motor_layout.stride);
	m_upload_warm_start_data.reserve(max_contacts * m_warm_start_layout.stride);
	m_upload_joint_data.reserve(max_joints * m_joint_layout.stride);
	m_upload_motor_map.reserve(max_bodies);
	m_upload_collision_state.reserve(collision_state_uints);
	m_upload_authoritative_bodies.reserve(max_bodies);
	m_staged_contact_data.reserve(max_contacts * m_contact_layout.stride);
	m_staged_joint_data.reserve(max_joints * m_joint_layout.stride);
	m_latest_gpu_body_data.reserve(max_bodies * m_body_layout.stride);
	m_readback_staging.reserve(readback_size);

	m_buffers_created = true;
}

auto gse::vbd::gpu_solver::upload(const std::span<const body_state> bodies, const std::span<const collision_body_data> collision_data, const std::span<const float> accel_weights, const std::span<const velocity_motor_constraint> motors, const std::span<const joint_constraint> joints, const std::span<const warm_start_entry> prev_contacts, const std::span<const std::uint32_t> authoritative_body_indices, const solver_config& solver_cfg, const time_step dt, const int steps) -> void {
	m_body_count = static_cast<std::uint32_t>(std::min(bodies.size(), static_cast<std::size_t>(max_bodies)));
	m_contact_count = 0;
	m_motor_count = static_cast<std::uint32_t>(std::min(motors.size(), static_cast<std::size_t>(max_motors)));
	m_steps = static_cast<std::uint32_t>(std::max(steps, 1));
	m_solver_cfg = solver_cfg;
	m_dt = dt;

	if (m_body_count == 0) {
		m_pending_dispatch = false;
		return;
	}


	m_upload_authoritative_bodies.assign(m_body_count, 0);
	for (const auto body_index : authoritative_body_indices) {
		if (body_index < m_upload_authoritative_bodies.size()) {
			m_upload_authoritative_bodies[body_index] = 1;
		}
	}

	auto* gpu_body_base = m_frames[m_dispatch_slot].body_buffer.mapped();

	for (std::uint32_t i = 0; i < m_body_count; ++i) {
		const auto& [
			position,
			predicted_position,
			inertia_target,
			initial_position,
			body_velocity,
			orientation,
			predicted_orientation,
			angular_inertia_target,
			initial_orientation,
			body_angular_velocity,
			motor_target,
			mass_value,
			inv_inertia,
			locked,
			update_orientation,
			affected_by_gravity,
			sleep_counter
		] = bodies[i];

		struct_writer elem(gpu_body_base + i * m_body_layout.stride);

		const bool authoritative = i < m_upload_authoritative_bodies.size() && m_upload_authoritative_bodies[i] != 0;
		const bool full_upload = m_frame_count < 2 || authoritative || locked;

		if (full_upload) {
			elem.write(m_body_layout["position"], position);
			elem.write(m_body_layout["predicted_position"], predicted_position);
			elem.write(m_body_layout["inertia_target"], inertia_target);
			elem.write(m_body_layout["old_position"], initial_position);
			elem.write(m_body_layout["velocity"], body_velocity);
			elem.write(m_body_layout["orientation"], orientation);
			elem.write(m_body_layout["predicted_orientation"], predicted_orientation);
			elem.write(m_body_layout["angular_inertia_target"], angular_inertia_target);
			elem.write(m_body_layout["old_orientation"], initial_orientation);
			elem.write(m_body_layout["angular_velocity"], body_angular_velocity);
			elem.write(m_body_layout["sleep_counter"], sleep_counter);
			if (i < collision_data.size()) {
				const auto& [half_extents, aabb_min, aabb_max] = collision_data[i];
				elem.write(m_body_layout["aabb_min"], aabb_min);
				elem.write(m_body_layout["aabb_max"], aabb_max);
			}
		}

		std::uint32_t flags = 0;
		if (locked) {
			flags |= flag_locked;
		}
		if (update_orientation) {
			flags |= flag_update_orientation;
		}
		if (affected_by_gravity) {
			flags |= flag_affected_by_gravity;
		}
		elem.write(m_body_layout["flags"], flags);
		elem.write(m_body_layout["mass"], mass_value);

		const float accel_weight =
			i < accel_weights.size()
				? accel_weights[i]
				: 0.f;
		elem.write(m_body_layout["accel_weight"], accel_weight);

		const auto inertia = inv_inertia.inverse();
		elem.write(m_body_layout["inertia"], inertia);

		if (i < collision_data.size()) {
			elem.write(m_body_layout["half_extents"], collision_data[i].half_extents);
		}
	}

	displacement max_extent = meters(0.5f);
	for (std::uint32_t i = 0; i < std::min(m_body_count, static_cast<std::uint32_t>(collision_data.size())); ++i) {
		const auto& he = collision_data[i].half_extents;
		max_extent = std::max({max_extent, he.x(), he.y(), he.z()});
	}
	m_grid_cell_size = max_extent * 4.0f;

	m_upload_motor_data.assign(m_motor_count * m_motor_layout.stride, std::byte{0});
	for (std::uint32_t i = 0; i < m_motor_count; ++i) {
		const auto& [
			body_index, target_velocity,
			compliance, max_force, horizontal_only
		] = motors[i];
		struct_writer elem(m_upload_motor_data.data() + i * m_motor_layout.stride);

		elem.write_index(m_motor_layout["body_index"], body_index, m_body_count);
		elem.write(m_motor_layout["compliance"], compliance);
		elem.write(m_motor_layout["max_force"], max_force);

		const std::uint32_t horiz = horizontal_only ? 1u : 0u;
		elem.write(m_motor_layout["horizontal_only"], horiz);

		elem.write(m_motor_layout["target_velocity"], target_velocity);
	}

	m_upload_motor_map.assign(max_bodies, 0xFFFFFFFFu);
	for (std::uint32_t mi = 0; mi < m_motor_count; ++mi) {
		if (motors[mi].body_index < max_bodies) {
			m_upload_motor_map[motors[mi].body_index] = mi;
		}
	}

	m_upload_collision_state.resize(collision_state_uints, 0);
	m_upload_collision_state[0] = 0;
	m_upload_collision_state[1] = 0;

	m_warm_start_count = static_cast<std::uint32_t>(std::min(prev_contacts.size(), static_cast<std::size_t>(max_contacts)));

	m_upload_warm_start_data.assign(m_warm_start_count * m_warm_start_layout.stride, std::byte{0});
	std::uint32_t ws_written = 0;
	for (std::uint32_t i = 0; i < m_warm_start_count; ++i) {
		const auto& [
			ws_body_a, ws_body_b, ws_feature_key, ws_sticking,
			ws_normal, ws_tangent_u, ws_tangent_v,
			ws_local_anchor_a, ws_local_anchor_b,
			ws_lambda, ws_penalty
		] = prev_contacts[i];

		if (ws_body_a >= m_body_count || ws_body_b >= m_body_count) {
			continue;
		}

		struct_writer elem(m_upload_warm_start_data.data() + ws_written * m_warm_start_layout.stride);

		elem.write_index(m_warm_start_layout["body_a"], ws_body_a, m_body_count);
		elem.write_index(m_warm_start_layout["body_b"], ws_body_b, m_body_count);
		const auto feature_hi = static_cast<std::uint32_t>(ws_feature_key >> 32);
		const auto feature_lo = static_cast<std::uint32_t>(ws_feature_key & 0xFFFFFFFFu);
		elem.write(m_warm_start_layout["feature_key_hi"], feature_hi);
		elem.write(m_warm_start_layout["feature_key_lo"], feature_lo);

		const std::uint32_t sticking = ws_sticking ? 1u : 0u;
		elem.write(m_warm_start_layout["sticking"], sticking);

		elem.write(m_warm_start_layout["normal"], ws_normal);
		elem.write(m_warm_start_layout["tangent_u"], ws_tangent_u);
		elem.write(m_warm_start_layout["tangent_v"], ws_tangent_v);

		elem.write(m_warm_start_layout["local_anchor_a"], ws_local_anchor_a);
		elem.write(m_warm_start_layout["local_anchor_b"], ws_local_anchor_b);
		elem.write(m_warm_start_layout["lambda"], ws_lambda);

		elem.write(m_warm_start_layout["penalty"], ws_penalty);
		++ws_written;
	}
	m_warm_start_count = ws_written;

	m_joint_count = 0;
	m_upload_joint_data.assign(std::min(joints.size(), static_cast<std::size_t>(max_joints)) * m_joint_layout.stride, std::byte{0});

	const time_step sub_dt = dt / static_cast<float>(std::max(m_steps, 1u));
	const time_squared h_squared = sub_dt * sub_dt;

	for (std::size_t i = 0; i < joints.size() && m_joint_count < max_joints; ++i) {
		const auto& j = joints[i];
		if (j.body_a >= m_body_count || j.body_b >= m_body_count) {
			continue;
		}
		struct_writer elem(m_upload_joint_data.data() + m_joint_count * m_joint_layout.stride);

		elem.write_index(m_joint_layout["body_a"], j.body_a, m_body_count);
		elem.write_index(m_joint_layout["body_b"], j.body_b, m_body_count);

		const auto type_val = static_cast<std::uint32_t>(j.type);
		elem.write(m_joint_layout["type"], type_val);

		const std::uint32_t limits = j.limits_enabled ? 1u : 0u;
		elem.write(m_joint_layout["limits_enabled"], limits);

		elem.write(m_joint_layout["local_anchor_a"], j.local_anchor_a);
		elem.write(m_joint_layout["local_anchor_b"], j.local_anchor_b);

		elem.write(m_joint_layout["local_axis_a"], j.local_axis_a);
		elem.write(m_joint_layout["local_axis_b"], j.local_axis_b);

		elem.write(m_joint_layout["target_distance"], j.target_distance);
		elem.write(m_joint_layout["compliance"], j.compliance);
		elem.write(m_joint_layout["damping"], j.damping);

		elem.write(m_joint_layout["limit_lower"], j.limit_lower);
		elem.write(m_joint_layout["limit_upper"], j.limit_upper);

		elem.write(m_joint_layout["rest_orientation"], j.rest_orientation);

		const auto& body_a_state = bodies[j.body_a];
		const auto& body_b_state = bodies[j.body_b];

		auto pos_lambda = j.pos_lambda;
		auto pos_penalty = j.pos_penalty;
		auto ang_lambda = j.ang_lambda;
		auto ang_penalty = j.ang_penalty;
		auto limit_lambda_val = j.limit_lambda;
		auto limit_penalty_val = j.limit_penalty;

		for (int k = 0; k < 3; ++k) {
			pos_lambda[k] *= solver_cfg.gamma;
		}
		for (int k = 0; k < 3; ++k) {
			ang_lambda[k] *= solver_cfg.gamma;
		}
		if (j.limits_enabled) {
			limit_lambda_val *= solver_cfg.gamma;
		}

		const auto r_aw = rotate_vector(body_a_state.orientation, j.local_anchor_a);
		const auto r_bw = rotate_vector(body_b_state.orientation, j.local_anchor_b);

		auto contact_eff_mass = [&](const vec3f& dir) -> mass {
			inverse_mass inv_mass_sum{};
			if (!body_a_state.locked && body_a_state.mass_value > mass{}) {
				inv_mass_sum += 1.f / body_a_state.mass_value;
			}
			if (!body_b_state.locked && body_b_state.mass_value > mass{}) {
				inv_mass_sum += 1.f / body_b_state.mass_value;
			}
			if (body_a_state.update_orientation && !body_a_state.locked) {
				const auto ang_j = cross(r_aw, dir);
				inv_mass_sum += dot(cross(body_a_state.inv_inertia * ang_j, r_aw), dir);
			}
			if (body_b_state.update_orientation && !body_b_state.locked) {
				const auto ang_j = cross(r_bw, dir);
				inv_mass_sum += dot(cross(body_b_state.inv_inertia * ang_j, r_bw), dir);
			}
			if (inv_mass_sum <= per_kilograms(1e-10f)) {
				return mass{};
			}
			return 1.f / inv_mass_sum;
		};

		auto angular_inv_i = [&](const vec3f& ang_dir) -> inverse_inertia {
			inverse_inertia inv_i_sum{};
			if (!body_a_state.locked) {
				inv_i_sum += dot(ang_dir, body_a_state.inv_inertia * ang_dir);
			}
			if (!body_b_state.locked) {
				inv_i_sum += dot(ang_dir, body_b_state.inv_inertia * ang_dir);
			}
			return inv_i_sum;
		};

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
				const auto d = (body_a_state.position + r_aw) - (body_b_state.position + r_bw);
				dir = magnitude(d) > meters(1e-7f) ? normalize(d) : axis_y;
			}
			else if (j.type == joint_type::slider) {
				const auto axis_w = rotate_vector(body_a_state.orientation, j.local_axis_a);
				vec3f perp0 = cross(axis_w, axis_y);
				if (magnitude(perp0) < 1e-6f) {
					perp0 = cross(axis_w, axis_x);
				}
				perp0 = normalize(perp0);
				dir = k == 0 ? perp0 : normalize(cross(axis_w, perp0));
			}
			else {
				dir = dirs[k];
			}
			const stiffness eff = contact_eff_mass(dir) / h_squared;
			pos_penalty[k] = std::clamp(
				std::max(pos_penalty[k] * solver_cfg.gamma, eff),
				solver_cfg.penalty_min, solver_cfg.penalty_max
			);
		}

		int num_ang_rows = 3;
		if (j.type == joint_type::distance) {
			num_ang_rows = 0;
		}
		else if (j.type == joint_type::hinge) {
			num_ang_rows = 2;
		}

		for (int k = 0; k < num_ang_rows; ++k) {
			vec3f ang_dir;
			if (j.type == joint_type::hinge) {
				const auto axis_a = rotate_vector(body_a_state.orientation, j.local_axis_a);
				vec3f perp_u = cross(axis_a, axis_y);
				if (magnitude(perp_u) < 1e-6f) {
					perp_u = cross(axis_a, axis_x);
				}
				perp_u = normalize(perp_u);
				ang_dir = k == 0 ? perp_u : normalize(cross(axis_a, perp_u));
			}
			else {
				ang_dir = dirs[k];
			}
			const auto inv_i_sum = angular_inv_i(ang_dir);
			const angular_stiffness eff_ang = inv_i_sum > per_kilogram_meter_squared(1e-10f) ? 1.f / inv_i_sum / h_squared / rad : solver_cfg.ang_penalty_min;
			ang_penalty[k] = std::clamp(
				std::max(ang_penalty[k] * solver_cfg.gamma, eff_ang),
				solver_cfg.ang_penalty_min, solver_cfg.ang_penalty_max
			);
		}

		if (j.limits_enabled) {
			const auto limit_axis = rotate_vector(body_a_state.orientation, j.local_axis_a);
			const auto inv_i_sum = angular_inv_i(limit_axis);
			const angular_stiffness eff_limit = inv_i_sum > per_kilogram_meter_squared(1e-10f) ? 1.f / inv_i_sum / h_squared / rad : solver_cfg.ang_penalty_min;
			limit_penalty_val = std::clamp(
				std::max(limit_penalty_val * solver_cfg.gamma, eff_limit),
				solver_cfg.ang_penalty_min, solver_cfg.ang_penalty_max
			);
		}

		const auto d_vec = body_a_state.position + r_aw - (body_b_state.position + r_bw);
		vec3<gap> pos_c0_val;
		vec3<angle> ang_c0_val;
		angle limit_c0_val = {};

		if (j.type == joint_type::distance) {
			pos_c0_val[0] = magnitude(d_vec) - j.target_distance;
		}
		else if (j.type == joint_type::fixed || j.type == joint_type::hinge) {
			for (int k = 0; k < 3; ++k) {
				pos_c0_val[k] = dot(dirs[k], d_vec);
			}

			const quat q_error = body_b_state.orientation * conjugate(body_a_state.orientation) * conjugate(j.rest_orientation);
			const vec3<angle> theta = to_axis_angle(q_error);

			if (j.type == joint_type::fixed) {
				for (int k = 0; k < 3; ++k) {
					ang_c0_val[k] = theta[k];
				}
			}
			else {
				const auto axis_a = rotate_vector(body_a_state.orientation, j.local_axis_a);
				const auto axis_b = rotate_vector(body_b_state.orientation, j.local_axis_b);
				const auto swing_error = cross(axis_a, axis_b);
				vec3f perp_u = cross(axis_a, axis_y);
				if (magnitude(perp_u) < 1e-6f) {
					perp_u = cross(axis_a, axis_x);
				}
				perp_u = normalize(perp_u);
				const auto perp_v = normalize(cross(axis_a, perp_u));
				ang_c0_val[0] = radians(dot(perp_u, swing_error));
				ang_c0_val[1] = radians(dot(perp_v, swing_error));

				if (j.limits_enabled) {
					if (const angle twist_angle = dot(axis_a, theta); twist_angle < j.limit_lower) {
						limit_c0_val = twist_angle - j.limit_lower;
					}
					else if (twist_angle > j.limit_upper) {
						limit_c0_val = twist_angle - j.limit_upper;
					}
				}
			}
		}
		else if (j.type == joint_type::slider) {
			const auto axis_w = normalize(rotate_vector(body_a_state.orientation, j.local_axis_a));
			vec3f perp0 = cross(axis_w, axis_y);
			if (magnitude(perp0) < 1e-6f) {
				perp0 = cross(axis_w, axis_x);
			}
			perp0 = normalize(perp0);
			const auto perp1 = normalize(cross(axis_w, perp0));
			pos_c0_val[0] = dot(perp0, d_vec);
			pos_c0_val[1] = dot(perp1, d_vec);

			const auto slider_theta = to_axis_angle(body_b_state.orientation * conjugate(body_a_state.orientation) * conjugate(j.rest_orientation));
			for (int k = 0; k < 3; ++k) {
				ang_c0_val[k] = slider_theta[k];
			}
		}

		elem.write(m_joint_layout["pos_lambda"], pos_lambda);
		elem.write(m_joint_layout["pos_penalty"], pos_penalty);

		elem.write(m_joint_layout["ang_lambda"], ang_lambda);
		elem.write(m_joint_layout["ang_penalty"], ang_penalty);

		elem.write(m_joint_layout["limit_lambda"], limit_lambda_val);
		elem.write(m_joint_layout["limit_penalty"], limit_penalty_val);

		elem.write(m_joint_layout["pos_c0"], pos_c0_val);
		elem.write(m_joint_layout["ang_c0"], ang_c0_val);
		elem.write(m_joint_layout["limit_c0"], limit_c0_val);
		++m_joint_count;
	}

	m_pending_dispatch = true;
}

auto gse::vbd::gpu_solver::total_substeps() const -> std::uint32_t {
	return 1 * m_steps;
}

auto gse::vbd::gpu_solver::commit_upload() -> void {
	if (!m_buffers_created || !m_pending_dispatch || m_body_count == 0) {
		if (m_frame_count < 5) {
			log::println(
				log::level::warning, log::category::physics, "commit_upload skipped: created={} pending={} bodies={}",
				m_buffers_created, m_pending_dispatch, m_body_count
			);
		}
		return;
	}
	if (m_frame_count < 5) {
		log::println(
			log::category::physics, "commit_upload: bodies={} motors={} joints={} warm_starts={}",
			m_body_count, m_motor_count, m_joint_count, m_warm_start_count
		);
	}

	auto& f = m_frames[m_dispatch_slot];
	gse::memcpy(f.motor_buffer.mapped(), m_upload_motor_data);
	gse::memcpy(f.motor_map_buffer.mapped(), m_upload_motor_map);
	gse::memcpy(f.collision_state_buffer.mapped(), m_upload_collision_state);

	if (!m_upload_warm_start_data.empty()) {
		gse::memcpy(f.warm_start_buffer.mapped(), m_upload_warm_start_data);
	}

	if (!m_upload_joint_data.empty()) {
		gse::memcpy(f.joint_buffer.mapped(), m_upload_joint_data);
	}
}

auto gse::vbd::gpu_solver::stage_readback() -> void {
	auto& f = m_frames[1 - m_dispatch_slot];
	if (!f.readback_pending) {
		return;
	}

	if (!f.first_submit && !f.queue.is_complete()) {
		return;
	}

	auto& info = f.readback_info;

	if (info.body_count == 0) {
		info = {};
		f.readback_pending = false;
		return;
	}

	const auto* rb = f.readback_buffer.mapped();
	const std::size_t contact_region_offset = max_bodies * m_body_layout.stride;
	const std::size_t state_offset = contact_region_offset + max_contacts * m_contact_layout.stride;
	const std::size_t state_size = collision_state_uints * sizeof(std::uint32_t);

	std::array<std::uint32_t, collision_state_uints> staged_collision_state{};
	gse::memcpy(
		reinterpret_cast<std::byte*>(staged_collision_state.data()),
		rb + state_offset,
		state_size
	);

	const std::uint32_t gpu_contact_count = staged_collision_state[0];
	m_staged_contact_count = std::min(gpu_contact_count, max_contacts);
	m_staged_color_count = std::min(staged_collision_state[1], max_colors);
	m_contact_count = m_staged_contact_count;

	const std::size_t body_size = info.body_count * m_body_layout.stride;
	const std::size_t contact_size = m_staged_contact_count * m_contact_layout.stride;
	const std::size_t joint_region_offset = state_offset + state_size;
	const std::size_t joint_size = info.joint_count * m_joint_layout.stride;
	const std::size_t needed = body_size + contact_size + joint_size;

	if (m_readback_staging.size() < needed) {
		m_readback_staging.resize(needed);
	}

	auto* out = m_readback_staging.data();
	gse::memcpy(out, rb, body_size);
	gse::memcpy(out + body_size, rb + contact_region_offset, contact_size);
	gse::memcpy(out + body_size + contact_size, rb + joint_region_offset, joint_size);

	m_staged_body_count = info.body_count;
	m_latest_gpu_body_data.assign(out, out + body_size);
	m_staged_contact_data.assign(out + body_size, out + body_size + contact_size);

	m_staged_joint_count = info.joint_count;
	if (m_staged_joint_count > 0) {
		m_staged_joint_data.assign(
			out + body_size + contact_size,
			out + body_size + contact_size + joint_size
		);
	}
	else {
		m_staged_joint_data.clear();
	}

	info = {};

	m_staged_valid = true;
	f.readback_pending = false;

	m_latest_gpu_body_count = m_staged_body_count;
}

auto gse::vbd::gpu_solver::readback(const std::span<body_state> bodies, std::vector<contact_readback_entry>& contacts_out, const std::span<joint_constraint> joints_out) -> void {
	if (!m_staged_valid) {
		return;
	}

	const std::uint32_t count = std::min(m_staged_body_count, static_cast<std::uint32_t>(bodies.size()));

	for (std::uint32_t i = 0; i < count; ++i) {
		struct_reader elem(m_latest_gpu_body_data.data() + i * m_body_layout.stride);
		auto& b = bodies[i];

		elem.read(m_body_layout["orientation"], b.orientation);
		if (dot(b.orientation, b.orientation) < 0.5f) {
			continue;
		}

		elem.read(m_body_layout["position"], b.position);
		if (!gse::isfinite(b.position)) {
			continue;
		}

		elem.read(m_body_layout["velocity"], b.body_velocity);
		if (!gse::isfinite(b.body_velocity)) {
			b.body_velocity = {};
		}

		elem.read(m_body_layout["angular_velocity"], b.body_angular_velocity);
		if (!gse::isfinite(b.body_angular_velocity)) {
			b.body_angular_velocity = {};
		}

		elem.read(m_body_layout["sleep_counter"], b.sleep_counter);
	}

	contacts_out.clear();
	contacts_out.reserve(m_staged_contact_count);
	for (std::uint32_t i = 0; i < m_staged_contact_count; ++i) {
		struct_reader elem(m_staged_contact_data.data() + i * m_contact_layout.stride);

		contact_readback_entry entry;
		elem.read(m_contact_layout["body_a"], entry.body_a);
		elem.read(m_contact_layout["body_b"], entry.body_b);
		std::uint32_t feature_hi = 0;
		std::uint32_t feature_lo = 0;
		elem.read(m_contact_layout["feature_key_hi"], feature_hi);
		elem.read(m_contact_layout["feature_key_lo"], feature_lo);
		entry.feature_key = (static_cast<std::uint64_t>(feature_hi) << 32) | static_cast<std::uint64_t>(feature_lo);

		std::uint32_t sticking = 0;
		elem.read(m_contact_layout["sticking"], sticking);
		entry.sticking = sticking != 0;

		elem.read(m_contact_layout["normal"], entry.normal);
		elem.read(m_contact_layout["tangent_u"], entry.tangent_u);
		elem.read(m_contact_layout["tangent_v"], entry.tangent_v);

		elem.read(m_contact_layout["r_a"], entry.local_anchor_a);
		elem.read(m_contact_layout["r_b"], entry.local_anchor_b);

		elem.read(m_contact_layout["c0_friction"], entry.c0);
		gse::memcpy(entry.friction_coeff, m_staged_contact_data.data() + i * m_contact_layout.stride + m_contact_layout["c0_friction"] + 3 * sizeof(float));

		elem.read(m_contact_layout["lambda"], entry.lambda);

		elem.read(m_contact_layout["penalty"], entry.penalty);

		contacts_out.push_back(entry);
	}

	const std::uint32_t jcount = std::min(m_staged_joint_count, static_cast<std::uint32_t>(joints_out.size()));
	for (std::uint32_t i = 0; i < jcount; ++i) {
		struct_reader elem(m_staged_joint_data.data() + i * m_joint_layout.stride);
		auto& jout = joints_out[i];

		elem.read(m_joint_layout["pos_lambda"], jout.pos_lambda);
		elem.read(m_joint_layout["pos_penalty"], jout.pos_penalty);
		elem.read(m_joint_layout["ang_lambda"], jout.ang_lambda);
		elem.read(m_joint_layout["ang_penalty"], jout.ang_penalty);
		elem.read(m_joint_layout["limit_lambda"], jout.limit_lambda);
		elem.read(m_joint_layout["limit_penalty"], jout.limit_penalty);
	}

	m_staged_valid = false;

}

auto gse::vbd::gpu_solver::has_readback_data() const -> bool {
	return m_staged_valid;
}

auto gse::vbd::gpu_solver::pending_dispatch() const -> bool {
	return m_pending_dispatch;
}

auto gse::vbd::gpu_solver::ready_to_dispatch() const -> bool {
	const auto& f = m_frames[m_dispatch_slot];
	const auto& other = m_frames[1 - m_dispatch_slot];
	return (f.first_submit || f.queue.is_complete()) && !other.readback_pending;
}

auto gse::vbd::gpu_solver::mark_dispatched() -> void {
	auto& f = m_frames[m_dispatch_slot];
	f.readback_info = { m_body_count, m_contact_count, m_joint_count };
	f.readback_pending = true;
	m_pending_dispatch = false;
	m_frame_count++;
}

auto gse::vbd::gpu_solver::body_count() const -> std::uint32_t {
	return m_body_count;
}

auto gse::vbd::gpu_solver::contact_count() const -> std::uint32_t {
	return m_contact_count;
}

auto gse::vbd::gpu_solver::motor_count() const -> std::uint32_t {
	return m_motor_count;
}

auto gse::vbd::gpu_solver::joint_count() const -> std::uint32_t {
	return m_joint_count;
}

auto gse::vbd::gpu_solver::solver_cfg() const -> const solver_config& {
	return m_solver_cfg;
}

auto gse::vbd::gpu_solver::dt() const -> time_step {
	return m_dt;
}

auto gse::vbd::gpu_solver::frame_count() const -> std::uint32_t {
	return m_frame_count;
}

auto gse::vbd::gpu_solver::solve_time() const -> time_step {
	return milliseconds(m_compute.solve_ms);
}

auto gse::vbd::gpu_solver::snapshot_buffer(const std::uint32_t slot) const -> const gpu::buffer& {
	return m_frames[slot].physics_snapshot_buffer;
}

auto gse::vbd::gpu_solver::latest_snapshot_slot() const -> std::uint32_t {
	return 1 - m_dispatch_slot;
}

auto gse::vbd::gpu_solver::compute_semaphore() const -> gpu::compute_semaphore_state {
	const auto slot = latest_snapshot_slot();
	return m_frames[slot].queue.semaphore_state();
}

auto gse::vbd::gpu_solver::body_layout() const -> const buffer_layout& {
	return m_body_layout;
}

auto gse::vbd::gpu_solver::initialize_compute(gpu::context& ctx, asset_registry<gpu::context>& assets) -> void {
	m_compute.predict = assets.get<shader>("Shaders/VBDPhysics/vbd_predict");
	m_compute.solve_color = assets.get<shader>("Shaders/VBDPhysics/vbd_solve_color");
	m_compute.update_lambda = assets.get<shader>("Shaders/VBDPhysics/vbd_update_lambda");
	m_compute.derive_velocities = assets.get<shader>("Shaders/VBDPhysics/vbd_derive_velocities");
	m_compute.finalize = assets.get<shader>("Shaders/VBDPhysics/vbd_finalize");
	m_compute.collision_reset = assets.get<shader>("Shaders/VBDPhysics/collision_reset");
	m_compute.collision_broad_phase = assets.get<shader>("Shaders/VBDPhysics/collision_broad_phase");
	m_compute.collision_narrow_phase = assets.get<shader>("Shaders/VBDPhysics/collision_narrow_phase");
	m_compute.collision_build_adjacency = assets.get<shader>("Shaders/VBDPhysics/collision_build_adjacency");
	m_compute.collision_grid_build = assets.get<shader>("Shaders/VBDPhysics/collision_grid_build");
	m_compute.update_joint_lambda = assets.get<shader>("Shaders/VBDPhysics/vbd_update_joint_lambda");
	m_compute.prepare_indirect = assets.get<shader>("Shaders/VBDPhysics/vbd_prepare_indirect");
	m_compute.prepare_contact_indirect = assets.get<shader>("Shaders/VBDPhysics/vbd_prepare_contact_indirect");
	m_compute.prepare_color_indirect = assets.get<shader>("Shaders/VBDPhysics/vbd_prepare_color_indirect");
	m_compute.freeze_jacobians = assets.get<shader>("Shaders/VBDPhysics/vbd_freeze_jacobians");
	m_compute.apply_jacobi = assets.get<shader>("Shaders/VBDPhysics/vbd_apply_jacobi");

	assets.instantly_load(m_compute.predict);
	assets.instantly_load(m_compute.solve_color);
	assets.instantly_load(m_compute.update_lambda);
	assets.instantly_load(m_compute.derive_velocities);
	assets.instantly_load(m_compute.finalize);
	assets.instantly_load(m_compute.collision_reset);
	assets.instantly_load(m_compute.collision_broad_phase);
	assets.instantly_load(m_compute.collision_narrow_phase);
	assets.instantly_load(m_compute.collision_build_adjacency);
	assets.instantly_load(m_compute.collision_grid_build);
	assets.instantly_load(m_compute.update_joint_lambda);
	assets.instantly_load(m_compute.prepare_indirect);
	assets.instantly_load(m_compute.prepare_contact_indirect);
	assets.instantly_load(m_compute.prepare_color_indirect);
	assets.instantly_load(m_compute.freeze_jacobians);
	assets.instantly_load(m_compute.apply_jacobi);

	m_compute.predict_pipeline = gpu::create_compute_pipeline(ctx, *m_compute.predict, "vbd_push_constants");
	m_compute.solve_color_pipeline = gpu::create_compute_pipeline(ctx, *m_compute.solve_color, "vbd_push_constants");
	m_compute.update_lambda_pipeline = gpu::create_compute_pipeline(ctx, *m_compute.update_lambda, "vbd_push_constants");
	m_compute.derive_velocities_pipeline = gpu::create_compute_pipeline(ctx, *m_compute.derive_velocities, "vbd_push_constants");
	m_compute.finalize_pipeline = gpu::create_compute_pipeline(ctx, *m_compute.finalize, "vbd_push_constants");
	m_compute.collision_reset_pipeline = gpu::create_compute_pipeline(ctx, *m_compute.collision_reset, "vbd_push_constants");
	m_compute.collision_broad_phase_pipeline = gpu::create_compute_pipeline(ctx, *m_compute.collision_broad_phase, "vbd_push_constants");
	m_compute.collision_narrow_phase_pipeline = gpu::create_compute_pipeline(ctx, *m_compute.collision_narrow_phase, "vbd_push_constants");
	m_compute.collision_grid_build_pipeline = gpu::create_compute_pipeline(ctx, *m_compute.collision_grid_build, "vbd_push_constants");
	m_compute.collision_build_adjacency_pipeline = gpu::create_compute_pipeline(ctx, *m_compute.collision_build_adjacency, "vbd_push_constants");
	m_compute.update_joint_lambda_pipeline = gpu::create_compute_pipeline(ctx, *m_compute.update_joint_lambda, "vbd_push_constants");
	m_compute.prepare_indirect_pipeline = gpu::create_compute_pipeline(ctx, *m_compute.prepare_indirect, "vbd_push_constants");
	m_compute.prepare_contact_indirect_pipeline = gpu::create_compute_pipeline(ctx, *m_compute.prepare_contact_indirect, "vbd_push_constants");
	m_compute.prepare_color_indirect_pipeline = gpu::create_compute_pipeline(ctx, *m_compute.prepare_color_indirect, "vbd_push_constants");
	m_compute.freeze_jacobians_pipeline = gpu::create_compute_pipeline(ctx, *m_compute.freeze_jacobians, "vbd_push_constants");
	m_compute.apply_jacobi_pipeline = gpu::create_compute_pipeline(ctx, *m_compute.apply_jacobi, "vbd_push_constants");

	auto extract_layout = [](const resource::handle<shader>& sh, const std::string& name) {
		const auto block = sh->uniform_block(name);
		buffer_layout layout;
		layout.stride = block.size;
		for (const auto& [n, member] : block.members) {
			layout.offsets[n] = member.offset;
		}
		return layout;
	};

	m_body_layout = extract_layout(m_compute.predict, "body_data");
	m_contact_layout = extract_layout(m_compute.predict, "contact_data");
	m_motor_layout = extract_layout(m_compute.predict, "motor_data");
	m_warm_start_layout = extract_layout(m_compute.predict, "warm_starts");
	m_joint_layout = extract_layout(m_compute.predict, "joint_data");
	m_frozen_jacobian_layout = extract_layout(m_compute.freeze_jacobians, "frozen_jacobians");

	create_buffers(ctx);

	for (auto& f : m_frames) {
		f.queue = gpu::create_compute_queue(ctx);
		f.descriptors = gpu::allocate_descriptors(ctx, *m_compute.predict);

		gpu::descriptor_writer(ctx, m_compute.predict, f.descriptors)
			.buffer("body_data", f.body_buffer)
			.buffer("contact_data", f.contact_buffer)
			.buffer("motor_data", f.motor_buffer)
			.buffer("color_data", f.color_buffer)
			.buffer("contact_offsets", f.contact_offsets_buffer)
			.buffer("solve_state", f.solve_state_buffer)
			.buffer("collision_pairs", f.collision_pair_buffer)
			.buffer("collision_state", f.collision_state_buffer)
			.buffer("warm_starts", f.warm_start_buffer)
			.buffer("joint_data", f.joint_buffer)
			.buffer("contact_counts", f.contact_counts_buffer)
			.buffer("contact_adjacency", f.contact_adjacency_buffer)
			.buffer("motor_map", f.motor_map_buffer)
			.buffer("joint_offsets", f.joint_offsets_buffer)
			.buffer("joint_counts", f.joint_counts_buffer)
			.buffer("joint_adjacency", f.joint_adjacency_buffer)
			.buffer("grid_data", f.grid_buffer)
			.buffer("indirect_args", f.indirect_dispatch_buffer)
			.buffer("frozen_jacobians", f.frozen_jacobian_buffer)
			.buffer("solve_deltas", f.solve_deltas_buffer)
			.commit();
	}

	m_compute.initialized = true;
}

auto gse::vbd::gpu_solver::dispatch_compute() -> void {
	if (!m_buffers_created || m_body_count == 0) {
		return;
	}

	auto& f = m_frames[m_dispatch_slot];

	m_compute.solve_ms = f.queue.read_timing();

	auto ingest_stage = [&](const std::string_view tag, const std::uint32_t start_slot, const std::uint32_t end_slot) {
		const float ms = f.queue.read_timing(start_slot, end_slot);
		if (ms > 0.f) {
			profile::ingest_gpu_sample(find_or_generate_id(tag), milliseconds(ms));
		}
	};

	ingest_stage("gpu:vbd_collision",  timing_slot::begin,           timing_slot::after_collision);
	ingest_stage("gpu:vbd_predict",    timing_slot::after_collision, timing_slot::after_predict);
	ingest_stage("gpu:vbd_solve",      timing_slot::after_predict,   timing_slot::after_solve);
	ingest_stage("gpu:vbd_velocity",   timing_slot::after_solve,     timing_slot::after_velocity);
	ingest_stage("gpu:vbd_finalize",   timing_slot::after_velocity,  timing_slot::after_finalize);

	f.queue.begin();

	constexpr std::array init_scopes = { gpu::barrier_scope::host_to_compute, gpu::barrier_scope::transfer_to_transfer };
	f.queue.barriers(init_scopes);

	const auto& cfg = m_solver_cfg;
	const std::uint32_t total = total_substeps();
	const time_step sub_dt = m_dt / static_cast<float>(total);
	const auto h_squared = sub_dt * sub_dt;

	auto ceil_div = [](const std::uint32_t a, const std::uint32_t b) {
		return (a + b - 1) / b;
	};

	f.queue.begin_timing();

	auto bind_and_push = [&](const resource::handle<shader>& sh, const gpu::pipeline& pipeline, const std::uint32_t color_offset, const std::uint32_t color_count, const std::uint32_t substep, const std::uint32_t iteration, const float current_alpha, const std::uint32_t warm_start_count) {
		f.queue.bind_pipeline(pipeline);
		f.queue.bind_descriptors(pipeline, f.descriptors);
		auto pc = sh->cache_push_block("vbd_push_constants");
		pc.set("body_count", m_body_count);
		pc.set("contact_count", max_contacts);
		pc.set("motor_count", m_motor_count);
		pc.set("color_offset", color_offset);
		pc.set("color_count", color_count);
		pc.set("warm_start_count", warm_start_count);
		pc.set("post_stabilize", cfg.post_stabilize ? 1u : 0u);
		pc.set("joint_count", m_joint_count);
		pc.set("h_squared", h_squared);
		pc.set("dt", sub_dt);
		pc.set("beta", cfg.beta);
		pc.set("ang_beta", cfg.ang_beta);
		pc.set("linear_damping", 0.0f);
		pc.set("velocity_sleep_threshold", cfg.velocity_sleep_threshold);
		pc.set("angular_sleep_threshold", cfg.angular_sleep_threshold);
		pc.set("current_alpha", current_alpha);
		pc.set("collision_margin", cfg.collision_margin);
		pc.set("friction_coefficient", cfg.friction_coefficient);
		pc.set("penalty_min", cfg.penalty_min);
		pc.set("penalty_max", cfg.penalty_max);
		pc.set("gamma", cfg.gamma);
		pc.set("solver_alpha", cfg.alpha);
		pc.set("speculative_margin", cfg.speculative_margin);
		pc.set("stick_threshold", cfg.stick_threshold);
		pc.set("substep", substep);
		pc.set("iteration", iteration);
		pc.set("convergence_threshold", cfg.convergence_threshold.linear);
		pc.set("min_iterations", cfg.min_iterations);
		pc.set("grid_cell_size", m_grid_cell_size);
		pc.set("use_jacobi", cfg.use_jacobi ? 1u : 0u);
		pc.set("jacobi_omega", cfg.jacobi_omega);
		f.queue.push(pipeline, pc);
	};

	const std::uint32_t body_workgroups = ceil_div(m_body_count, workgroup_size);
	constexpr std::uint32_t num_colors = max_colors;
	const std::uint32_t num_iterations = cfg.iterations;
	const float solve_alpha = cfg.post_stabilize ? 1.f : cfg.alpha;

	for (std::uint32_t sub = 0; sub < total; ++sub) {
		const std::uint32_t substep_warm_start_count = (sub == 0) ? m_warm_start_count : 0u;

		bind_and_push(m_compute.collision_reset, m_compute.collision_reset_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch(ceil_div(std::max({m_body_count, max_contacts, grid_table_size}), workgroup_size), 1, 1);
		f.queue.barrier(gpu::barrier_scope::compute_to_compute);

		bind_and_push(m_compute.collision_grid_build, m_compute.collision_grid_build_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch(body_workgroups, 1, 1);
		f.queue.barrier(gpu::barrier_scope::compute_to_compute);

		bind_and_push(m_compute.collision_broad_phase, m_compute.collision_broad_phase_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch(body_workgroups, 1, 1);
		f.queue.barrier(gpu::barrier_scope::compute_to_compute);

		bind_and_push(m_compute.prepare_indirect, m_compute.prepare_indirect_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch(1, 1, 1);
		f.queue.barrier(gpu::barrier_scope::compute_to_indirect);

		bind_and_push(m_compute.collision_narrow_phase, m_compute.collision_narrow_phase_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch_indirect(f.indirect_dispatch_buffer, 0);
		f.queue.barrier(gpu::barrier_scope::compute_to_compute);

		bind_and_push(m_compute.prepare_contact_indirect, m_compute.prepare_contact_indirect_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch(1, 1, 1);
		f.queue.barrier(gpu::barrier_scope::compute_to_indirect);

		bind_and_push(m_compute.collision_build_adjacency, m_compute.collision_build_adjacency_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch(1, 1, 1);
		f.queue.barrier(gpu::barrier_scope::compute_to_indirect);

		f.queue.mark_timing(timing_slot::after_collision);

		bind_and_push(m_compute.predict, m_compute.predict_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch(body_workgroups, 1, 1);
		f.queue.barrier(gpu::barrier_scope::compute_to_compute);

		bind_and_push(m_compute.freeze_jacobians, m_compute.freeze_jacobians_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch_indirect(f.indirect_dispatch_buffer, 3 * sizeof(std::uint32_t));
		f.queue.barrier(gpu::barrier_scope::compute_to_compute);

		f.queue.mark_timing(timing_slot::after_predict);

		for (std::uint32_t iterations = 0; iterations < num_iterations; ++iterations) {
			bind_and_push(m_compute.solve_color, m_compute.solve_color_pipeline, 0u, num_colors, sub, iterations, solve_alpha, substep_warm_start_count);
			auto color_pc = m_compute.solve_color->cache_push_block("vbd_push_constants");
			color_pc.set("body_count", m_body_count);
			color_pc.set("contact_count", max_contacts);
			color_pc.set("motor_count", m_motor_count);
			color_pc.set("color_offset", 0u);
			color_pc.set("color_count", num_colors);
			color_pc.set("warm_start_count", substep_warm_start_count);
			color_pc.set("post_stabilize", cfg.post_stabilize ? 1u : 0u);
			color_pc.set("joint_count", m_joint_count);
			color_pc.set("h_squared", h_squared);
			color_pc.set("dt", sub_dt);
			color_pc.set("beta", cfg.beta);
			color_pc.set("ang_beta", cfg.ang_beta);
			color_pc.set("linear_damping", 0.0f);
			color_pc.set("velocity_sleep_threshold", cfg.velocity_sleep_threshold);
			color_pc.set("angular_sleep_threshold", cfg.angular_sleep_threshold);
			color_pc.set("current_alpha", solve_alpha);
			color_pc.set("collision_margin", cfg.collision_margin);
			color_pc.set("friction_coefficient", cfg.friction_coefficient);
			color_pc.set("penalty_min", cfg.penalty_min);
			color_pc.set("penalty_max", cfg.penalty_max);
			color_pc.set("gamma", cfg.gamma);
			color_pc.set("solver_alpha", cfg.alpha);
			color_pc.set("speculative_margin", cfg.speculative_margin);
			color_pc.set("stick_threshold", cfg.stick_threshold);
			color_pc.set("substep", sub);
			color_pc.set("iteration", iterations);
			color_pc.set("convergence_threshold", cfg.convergence_threshold.linear);
			color_pc.set("min_iterations", cfg.min_iterations);
			color_pc.set("grid_cell_size", m_grid_cell_size);
			color_pc.set("use_jacobi", cfg.use_jacobi ? 1u : 0u);
			color_pc.set("jacobi_omega", cfg.jacobi_omega);

			if (cfg.use_jacobi) {
				f.queue.push(m_compute.solve_color_pipeline, color_pc);
				f.queue.dispatch(body_workgroups, 1, 1);
				f.queue.barrier(gpu::barrier_scope::compute_to_compute);

				bind_and_push(m_compute.apply_jacobi, m_compute.apply_jacobi_pipeline, 0u, 0u, sub, iterations, solve_alpha, substep_warm_start_count);
				f.queue.dispatch(body_workgroups, 1, 1);
				f.queue.barrier(gpu::barrier_scope::compute_to_compute);
			} else {
				for (std::uint32_t color = 0; color < num_colors; ++color) {
					color_pc.set("color_offset", color);
					f.queue.push(m_compute.solve_color_pipeline, color_pc);
					f.queue.dispatch_indirect(f.indirect_dispatch_buffer, (2 + color) * 3 * sizeof(std::uint32_t));
					if (color + 1 < num_colors) {
						f.queue.barrier(gpu::barrier_scope::compute_to_compute);
					}
				}

				f.queue.barrier(gpu::barrier_scope::compute_to_compute);
			}

			bind_and_push(m_compute.update_lambda, m_compute.update_lambda_pipeline, 0u, 0u, sub, iterations, solve_alpha, substep_warm_start_count);
			f.queue.dispatch_indirect(f.indirect_dispatch_buffer, 3 * sizeof(std::uint32_t));
			if (m_joint_count > 0) {
				bind_and_push(m_compute.update_joint_lambda, m_compute.update_joint_lambda_pipeline, 0u, 0u, sub, iterations, solve_alpha, substep_warm_start_count);
				f.queue.dispatch(ceil_div(m_joint_count, workgroup_size), 1, 1);
			}
			f.queue.barrier(gpu::barrier_scope::compute_to_indirect);
		}

		f.queue.mark_timing(timing_slot::after_solve);

		bind_and_push(m_compute.derive_velocities, m_compute.derive_velocities_pipeline, 0u, 0u, sub, num_iterations, solve_alpha, substep_warm_start_count);
		f.queue.dispatch(body_workgroups, 1, 1);
		f.queue.barrier(gpu::barrier_scope::compute_to_compute);

		if (cfg.post_stabilize) {
			bind_and_push(m_compute.prepare_color_indirect, m_compute.prepare_color_indirect_pipeline, 0u, 0u, sub, num_iterations, 0.f, substep_warm_start_count);
			f.queue.dispatch(1, 1, 1);
			f.queue.barrier(gpu::barrier_scope::compute_to_indirect);

			auto color_pc = m_compute.solve_color->cache_push_block("vbd_push_constants");
			color_pc.set("body_count", m_body_count);
			color_pc.set("contact_count", max_contacts);
			color_pc.set("motor_count", m_motor_count);
			color_pc.set("color_offset", 0u);
			color_pc.set("color_count", num_colors);
			color_pc.set("warm_start_count", substep_warm_start_count);
			color_pc.set("post_stabilize", cfg.post_stabilize ? 1u : 0u);
			color_pc.set("joint_count", m_joint_count);
			color_pc.set("h_squared", h_squared);
			color_pc.set("dt", sub_dt);
			color_pc.set("beta", cfg.beta);
			color_pc.set("ang_beta", cfg.ang_beta);
			color_pc.set("linear_damping", 0.0f);
			color_pc.set("velocity_sleep_threshold", cfg.velocity_sleep_threshold);
			color_pc.set("angular_sleep_threshold", cfg.angular_sleep_threshold);
			color_pc.set("current_alpha", 0.f);
			color_pc.set("collision_margin", cfg.collision_margin);
			color_pc.set("friction_coefficient", cfg.friction_coefficient);
			color_pc.set("penalty_min", cfg.penalty_min);
			color_pc.set("penalty_max", cfg.penalty_max);
			color_pc.set("gamma", cfg.gamma);
			color_pc.set("solver_alpha", cfg.alpha);
			color_pc.set("speculative_margin", cfg.speculative_margin);
			color_pc.set("stick_threshold", cfg.stick_threshold);
			color_pc.set("substep", sub);
			color_pc.set("iteration", num_iterations);
			color_pc.set("convergence_threshold", cfg.convergence_threshold.linear);
			color_pc.set("min_iterations", cfg.min_iterations);
			color_pc.set("grid_cell_size", m_grid_cell_size);
			color_pc.set("use_jacobi", cfg.use_jacobi ? 1u : 0u);
			color_pc.set("jacobi_omega", cfg.jacobi_omega);

			f.queue.bind_pipeline(m_compute.solve_color_pipeline);
			f.queue.bind_descriptors(m_compute.solve_color_pipeline, f.descriptors);

			if (cfg.use_jacobi) {
				f.queue.push(m_compute.solve_color_pipeline, color_pc);
				f.queue.dispatch(body_workgroups, 1, 1);
				f.queue.barrier(gpu::barrier_scope::compute_to_compute);

				bind_and_push(m_compute.apply_jacobi, m_compute.apply_jacobi_pipeline, 0u, 0u, sub, num_iterations, 0.f, substep_warm_start_count);
				f.queue.dispatch(body_workgroups, 1, 1);
				f.queue.barrier(gpu::barrier_scope::compute_to_compute);
			} else {
				for (std::uint32_t color = 0; color < num_colors; ++color) {
					color_pc.set("color_offset", color);
					f.queue.push(m_compute.solve_color_pipeline, color_pc);
					f.queue.dispatch_indirect(f.indirect_dispatch_buffer, (2 + color) * 3 * sizeof(std::uint32_t));
					if (color + 1 < num_colors) {
						f.queue.barrier(gpu::barrier_scope::compute_to_compute);
					}
				}
				f.queue.barrier(gpu::barrier_scope::compute_to_compute);
			}
		}

		f.queue.mark_timing(timing_slot::after_velocity);

		bind_and_push(m_compute.finalize, m_compute.finalize_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch(body_workgroups, 1, 1);
		f.queue.barrier(gpu::barrier_scope::compute_to_compute);

		f.queue.mark_timing(timing_slot::after_finalize);
	}

	f.queue.end_timing();

	f.queue.barrier(gpu::barrier_scope::compute_to_transfer);

	const std::size_t body_copy_size = m_body_count * m_body_layout.stride;
	f.queue.copy_buffer({
		.src = f.body_buffer,
		.dst = f.readback_buffer,
		.size = body_copy_size
	});

	const std::size_t contact_dst_base = max_bodies * m_body_layout.stride;
	const std::size_t contact_copy_size = max_contacts * m_contact_layout.stride;
	f.queue.copy_buffer({
		.src = f.contact_buffer,
		.dst = f.readback_buffer,
		.dst_offset = contact_dst_base,
		.size = contact_copy_size
	});

	const std::size_t count_dst = contact_dst_base + contact_copy_size;
	f.queue.copy_buffer({
		.src = f.collision_state_buffer,
		.dst = f.readback_buffer,
		.dst_offset = count_dst,
		.size = collision_state_uints * sizeof(std::uint32_t)
	});

	if (m_joint_count > 0) {
		const std::size_t joint_dst = count_dst + collision_state_uints * sizeof(std::uint32_t);
		const std::size_t joint_copy_size = m_joint_count * m_joint_layout.stride;
		f.queue.copy_buffer({
			.src = f.joint_buffer,
			.dst = f.readback_buffer,
			.dst_offset = joint_dst,
			.size = joint_copy_size
		});
	}

	auto& other = m_frames[1 - m_dispatch_slot];
	f.queue.copy_buffer({
		.src = f.body_buffer,
		.dst = other.body_buffer,
		.size = body_copy_size
	});

	f.queue.copy_buffer({
		.src = f.body_buffer,
		.dst = f.physics_snapshot_buffer,
		.size = body_copy_size
	});

	f.queue.barrier(gpu::barrier_scope::transfer_to_host);

	f.queue.submit();

	mark_dispatched();
	f.first_submit = false;
	m_dispatch_slot = 1 - m_dispatch_slot;
}

auto gse::vbd::gpu_solver::compute_initialized() const -> bool {
	return m_compute.initialized;
}

auto gse::vbd::gpu_solver::buffers_created() const -> bool {
	return m_buffers_created;
}

auto gse::vbd::gpu_solver::body_stride() const -> std::uint32_t {
	return m_body_layout.stride;
}

auto gse::vbd::gpu_solver::contact_stride() const -> std::uint32_t {
	return m_contact_layout.stride;
}

auto gse::vbd::gpu_solver::contact_lambda_offset() const -> std::uint32_t {
	return m_contact_layout["lambda"];
}
