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

namespace gse::vbd {
	using time_step = time_t<float, gse::seconds>;

	struct [[= shaders::shader_struct]] vbd_push_constants {
		std::uint32_t body_count;
		std::uint32_t contact_count;
		std::uint32_t motor_count;
		std::uint32_t color_offset;
		std::uint32_t color_count;
		std::uint32_t warm_start_count;
		std::uint32_t post_stabilize;
		std::uint32_t joint_count;
		std::uint32_t impulse_count;
		time_squared h_squared;
		time_step dt;
		stiffness beta;
		angular_stiffness ang_beta;
		float linear_damping;
		velocity velocity_sleep_threshold;
		angular_velocity angular_sleep_threshold;
		float current_alpha;
		gap collision_margin;
		float friction_coefficient;
		stiffness penalty_min;
		stiffness penalty_max;
		float gamma;
		float solver_alpha;
		gap speculative_margin;
		gap stick_threshold;
		std::uint32_t substep;
		std::uint32_t iteration;
		length convergence_threshold;
		std::uint32_t min_iterations;
		gap grid_cell_size;
		std::uint32_t use_jacobi;
		float jacobi_omega;
		velocity restitution_threshold;
	};

	struct [[= shaders::binding<0, 0>{}, = shaders::ssbo_readwrite]] body_data {
		using element = body_state;
	};
	struct [[= shaders::binding<0, 1>{}, = shaders::ssbo_readwrite]] contact_data {
		using element = contact_constraint;
	};
	struct [[= shaders::binding<0, 2>{}, = shaders::ssbo_readonly]] motor_data {
		using element = velocity_motor_constraint;
	};
	struct [[= shaders::binding<0, 3>{}, = shaders::ssbo_readwrite]] color_data {
		using element = std::uint32_t;
	};
	struct [[= shaders::binding<0, 4>{}, = shaders::ssbo_readwrite]] contact_offsets {
		using element = std::uint32_t;
	};
	struct [[= shaders::binding<0, 5>{}, = shaders::ssbo_readwrite]] solve_state {
		using element = vec4f;
	};
	struct [[= shaders::binding<0, 6>{}, = shaders::ssbo_readwrite]] collision_pairs {
		using element = std::uint32_t;
	};
	struct [[= shaders::binding<0, 7>{}, = shaders::ssbo_readwrite]] collision_state {
		using element = std::uint32_t;
	};
	struct [[= shaders::binding<0, 8>{}, = shaders::ssbo_readonly]] warm_starts {
		using element = contact_constraint;
	};
	struct [[= shaders::binding<0, 9>{}, = shaders::ssbo_readwrite]] joint_data {
		using element = joint_constraint;
	};
	struct [[= shaders::binding<0, 10>{}, = shaders::ssbo_readwrite]] contact_counts {
		using element = std::uint32_t;
	};
	struct [[= shaders::binding<0, 11>{}, = shaders::ssbo_readwrite]] contact_adjacency {
		using element = std::uint32_t;
	};
	struct [[= shaders::binding<0, 12>{}, = shaders::ssbo_readwrite]] motor_map {
		using element = std::uint32_t;
	};
	struct [[= shaders::binding<0, 13>{}, = shaders::ssbo_readwrite]] joint_offsets {
		using element = std::uint32_t;
	};
	struct [[= shaders::binding<0, 14>{}, = shaders::ssbo_readwrite]] joint_counts {
		using element = std::uint32_t;
	};
	struct [[= shaders::binding<0, 15>{}, = shaders::ssbo_readwrite]] joint_adjacency {
		using element = std::uint32_t;
	};
	struct [[= shaders::binding<0, 16>{}, = shaders::ssbo_readwrite]] grid_data {
		using element = std::uint32_t;
	};
	struct [[= shaders::binding<0, 17>{}, = shaders::ssbo_readwrite]] indirect_args {
		using element = dispatch_args;
	};
	struct [[= shaders::binding<0, 18>{}, = shaders::ssbo_readwrite]] frozen_jacobians {
		using element = frozen_jacobian;
	};
	struct [[= shaders::binding<0, 19>{}, = shaders::ssbo_readwrite]] solve_deltas {
		using element = vec4f;
	};
	struct [[= shaders::binding<0, 20>{}, = shaders::ssbo_readwrite]] grounded_bits {
		using element = std::uint32_t;
	};
	struct [[= shaders::binding<0, 21>{}, = shaders::ssbo_readonly]] impulse_data {
		using element = impulse_constraint;
	};

	using shader_binding_types = type_pack<
		body_data,
		contact_data,
		motor_data,
		color_data,
		contact_offsets,
		solve_state,
		collision_pairs,
		collision_state,
		warm_starts,
		joint_data,
		contact_counts,
		contact_adjacency,
		motor_map,
		joint_offsets,
		joint_counts,
		joint_adjacency,
		grid_data,
		indirect_args,
		frozen_jacobians,
		solve_deltas,
		grounded_bits,
		impulse_data
	>;

	template <fixed_string BodyPath>
	using vbd_compute = gpu::compute_entry<
		gpu::body_path<BodyPath>,
		gpu::layout<"vbd_physics">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<64>,
		gpu::push_constant<vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using predict_entry = vbd_compute<"VBDPhysics/vbd_predict">;
	using solve_color_entry = vbd_compute<"VBDPhysics/vbd_solve_color">;
	using update_lambda_entry = vbd_compute<"VBDPhysics/vbd_update_lambda">;
	using derive_velocities_entry = vbd_compute<"VBDPhysics/vbd_derive_velocities">;
	using finalize_entry = vbd_compute<"VBDPhysics/vbd_finalize">;
	using collision_reset_entry = vbd_compute<"VBDPhysics/collision_reset">;
	using collision_grid_build_entry = vbd_compute<"VBDPhysics/collision_grid_build">;
	using collision_broad_phase_entry = vbd_compute<"VBDPhysics/collision_broad_phase">;
	using collision_narrow_phase_entry = vbd_compute<"VBDPhysics/collision_narrow_phase">;
	using update_joint_lambda_entry = vbd_compute<"VBDPhysics/vbd_update_joint_lambda">;
	using freeze_jacobians_entry = vbd_compute<"VBDPhysics/vbd_freeze_jacobians">;
	using apply_jacobi_entry = vbd_compute<"VBDPhysics/vbd_apply_jacobi">;
	using apply_restitution_entry = vbd_compute<"VBDPhysics/vbd_apply_restitution">;
	using apply_impulses_entry = vbd_compute<"VBDPhysics/vbd_apply_impulses">;

	using collision_build_adjacency_entry = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/collision_build_adjacency">,
		gpu::layout<"vbd_physics">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<64>,
		gpu::push_constant<vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id, gpu::group_thread_id>
	>;

	using prepare_indirect_entry = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_prepare_indirect">,
		gpu::layout<"vbd_physics">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<1>,
		gpu::push_constant<vbd_push_constants>
	>;

	using prepare_contact_indirect_entry = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_prepare_contact_indirect">,
		gpu::layout<"vbd_physics">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<1>,
		gpu::push_constant<vbd_push_constants>
	>;

	using prepare_color_indirect_entry = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_prepare_color_indirect">,
		gpu::layout<"vbd_physics">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<16>,
		gpu::push_constant<vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	auto prepare_joint(
		const joint_constraint& j,
		const body_state& body_a_state,
		const body_state& body_b_state,
		time_squared h_squared,
		const solver_config& solver_cfg
	) -> joint_constraint;
}

auto gse::vbd::prepare_joint(const joint_constraint& j, const body_state& body_a_state, const body_state& body_b_state, const time_squared h_squared, const solver_config& solver_cfg) -> joint_constraint {
	joint_constraint g = j;

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
		if (!body_a_state.locked && body_a_state.mass > mass{}) {
			inv_mass_sum += 1.f / body_a_state.mass;
		}
		if (!body_b_state.locked && body_b_state.mass > mass{}) {
			inv_mass_sum += 1.f / body_b_state.mass;
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
			solver_cfg.penalty_min,
			solver_cfg.penalty_max
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
			solver_cfg.ang_penalty_min,
			solver_cfg.ang_penalty_max
		);
	}

	if (j.limits_enabled) {
		const auto limit_axis = rotate_vector(body_a_state.orientation, j.local_axis_a);
		const auto inv_i_sum = angular_inv_i(limit_axis);
		const angular_stiffness eff_limit = inv_i_sum > per_kilogram_meter_squared(1e-10f) ? 1.f / inv_i_sum / h_squared / rad : solver_cfg.ang_penalty_min;
		limit_penalty_val = std::clamp(
			std::max(limit_penalty_val * solver_cfg.gamma, eff_limit),
			solver_cfg.ang_penalty_min,
			solver_cfg.ang_penalty_max
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

	g.pos_lambda = pos_lambda;
	g.pos_penalty = pos_penalty;
	g.ang_lambda = ang_lambda;
	g.ang_penalty = ang_penalty;
	g.limit_lambda = limit_lambda_val;
	g.limit_penalty = limit_penalty_val;
	g.pos_c0 = pos_c0_val;
	g.ang_c0 = ang_c0_val;
	g.limit_c0 = limit_c0_val;

	return g;
}

auto gse::vbd::gpu_solver::create_buffers(const gpu::context::data& ctx) -> void {
	constexpr auto storage_src = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_src;
	constexpr auto storage_dst = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst;
	constexpr auto storage_src_dst = storage_src | gpu::buffer_flag::transfer_dst;
	constexpr std::size_t color_buffer_size = max_colors * sizeof(std::uint32_t) * 2 + max_bodies * sizeof(std::uint32_t);
	constexpr std::size_t collision_pair_size = sizeof(std::uint32_t) + max_collision_pairs * 2 * sizeof(std::uint32_t);
	constexpr std::size_t collision_state_size = collision_state_uints * sizeof(std::uint32_t);
	constexpr std::size_t joint_buffer_size = max_joints * sizeof(joint_constraint);

	for (auto& f : m_frames) {
		f.body_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = max_bodies * sizeof(body_state),
			.usage = storage_src_dst
		});

		f.contact_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = max_contacts * sizeof(contact_constraint),
			.usage = storage_src
		});

		f.motor_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = max_motors * sizeof(velocity_motor_constraint),
			.usage = gpu::buffer_flag::storage
		});

		f.color_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = color_buffer_size,
			.usage = gpu::buffer_flag::storage
		});

		f.contact_offsets_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = max_bodies * sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage
		});
		f.contact_counts_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = max_bodies * sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage
		});
		f.contact_adjacency_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = max_contacts * 2 * sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage
		});
		f.motor_map_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = max_bodies * sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage
		});
		f.joint_offsets_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = max_bodies * sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage
		});
		f.joint_counts_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = max_bodies * sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage
		});
		f.joint_adjacency_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = max_joints * 2 * sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage
		});

		f.solve_state_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = max_bodies * solve_state_float4s_per_body * sizeof(float) * 4,
			.usage = gpu::buffer_flag::storage
		});

		f.collision_pair_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = collision_pair_size,
			.usage = gpu::buffer_flag::storage
		});

		f.collision_state_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = collision_state_size,
			.usage = storage_src
		});

		f.warm_start_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = 16,
			.usage = gpu::buffer_flag::storage
		});

		f.joint_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = std::max<std::size_t>(joint_buffer_size, 16),
			.usage = storage_src
		});

		constexpr std::size_t grid_buffer_size = (1 + grid_table_size + max_bodies * 8 * 2) * sizeof(std::uint32_t);
		f.grid_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = grid_buffer_size,
			.usage = gpu::buffer_flag::storage
		});

		f.physics_snapshot_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = max_bodies * sizeof(body_state),
			.usage = storage_dst
		});
		f.physics_snapshot_buffer.host_zero();

		f.indirect_dispatch_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = (2 + max_colors) * 3 * sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage | gpu::buffer_flag::indirect
		});
		f.indirect_dispatch_buffer.host_zero();

		f.frozen_jacobian_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = max_contacts * sizeof(frozen_jacobian),
			.usage = gpu::buffer_flag::storage
		});

		f.solve_deltas_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = max_bodies * 2 * sizeof(float) * 4,
			.usage = gpu::buffer_flag::storage
		});

		f.grounded_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = grounded_bits_uints * sizeof(std::uint32_t),
			.usage = storage_src
		});
		f.grounded_buffer.host_zero();

		f.grounded_readback_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = grounded_bits_uints * sizeof(std::uint32_t),
			.usage = storage_dst
		});
		f.grounded_readback_buffer.host_zero();

		f.impulse_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = max_impulses * sizeof(impulse_constraint),
			.usage = gpu::buffer_flag::storage
		});
	}

	m_upload_motors.reserve(max_motors);
	m_upload_joints.reserve(max_joints);
	m_upload_impulses.reserve(max_impulses);
	m_upload_motor_map.reserve(max_bodies);
	m_upload_collision_state.reserve(collision_state_uints);

	m_buffers_created = true;
}

auto gse::vbd::gpu_solver::upload(const std::span<const body_state> bodies, const std::span<const velocity_motor_constraint> motors, const std::span<const joint_constraint> joints, const std::span<const impulse_constraint> impulses, const solver_config& solver_cfg, const time_step dt, const int steps, const bool refresh_joints) -> void {
	m_body_count = static_cast<std::uint32_t>(std::min(bodies.size(), static_cast<std::size_t>(max_bodies)));
	m_motor_count = static_cast<std::uint32_t>(std::min(motors.size(), static_cast<std::size_t>(max_motors)));
	m_impulse_count = static_cast<std::uint32_t>(std::min(impulses.size(), static_cast<std::size_t>(max_impulses)));
	m_steps = static_cast<std::uint32_t>(std::max(steps, 1));
	m_solver_cfg = solver_cfg;
	m_dt = dt;

	if (m_body_count == 0) {
		m_pending_dispatch = false;
		m_body_buffers_seeded = false;
		m_joint_buffers_seeded = false;
		m_seeded_body_count = 0;
		m_joint_count = 0;
		m_impulse_count = 0;
		return;
	}

	m_upload_impulses.assign(impulses.begin(), impulses.begin() + m_impulse_count);

	const bool upload_body_buffer = !m_body_buffers_seeded || m_seeded_body_count != m_body_count;
	const bool upload_joint_buffer = refresh_joints || !m_joint_buffers_seeded || m_seeded_body_count != m_body_count;

	auto& frame_data = m_frames[m_dispatch_slot];
	if (upload_body_buffer) {
		frame_data.body_buffer.host_write(bodies.first(m_body_count));
	}
	else {
		for (std::uint32_t i = 0; i < m_body_count; ++i) {
			if (bodies[i].locked) {
				frame_data.body_buffer.host_write(bodies[i], i * sizeof(body_state));
			}
		}
	}

	displacement max_extent = meters(0.5f);
	for (std::uint32_t i = 0; i < m_body_count; ++i) {
		const auto& he = bodies[i].half_extents;
		max_extent = std::max({ max_extent, he.x(), he.y(), he.z() });
	}
	m_grid_cell_size = max_extent * 4.0f;

	m_upload_motors.assign(motors.begin(), motors.begin() + m_motor_count);

	m_upload_motor_map.assign(max_bodies, 0xFFFFFFFFu);
	for (std::uint32_t mi = 0; mi < m_motor_count; ++mi) {
		assert(motors[mi].body_index < m_body_count, "Motor body index {} out of bounds (max {})", motors[mi].body_index, m_body_count);
		if (motors[mi].body_index < max_bodies) {
			m_upload_motor_map[motors[mi].body_index] = mi;
		}
	}

	m_upload_collision_state.assign(collision_state_uints, 0);

	m_warm_start_count = 0;
	m_upload_joints.clear();
	m_upload_joints_dirty = false;

	if (upload_joint_buffer) {
		m_joint_count = 0;
		m_upload_joints.assign(std::min(joints.size(), static_cast<std::size_t>(max_joints)), joint_constraint{});

		const time_step sub_dt = dt / static_cast<float>(std::max(m_steps, 1u));
		const time_squared h_squared = sub_dt * sub_dt;

		for (std::size_t i = 0; i < joints.size() && m_joint_count < max_joints; ++i) {
			const auto& j = joints[i];
			if (j.body_a >= m_body_count || j.body_b >= m_body_count) {
				continue;
			}
			m_upload_joints[m_joint_count] = prepare_joint(j, bodies[j.body_a], bodies[j.body_b], h_squared, solver_cfg);
			++m_joint_count;
		}
		m_upload_joints_dirty = true;
		m_joint_buffers_seeded = true;
	}

	m_pending_dispatch = true;
}

auto gse::vbd::gpu_solver::total_substeps() const -> std::uint32_t {
	return 1 * m_steps;
}

auto gse::vbd::gpu_solver::commit_upload() -> void {
	if (!m_buffers_created || !m_pending_dispatch || m_body_count == 0) {
		return;
	}

	auto& f = m_frames[m_dispatch_slot];
	f.motor_buffer.host_write(m_upload_motors);
	f.motor_map_buffer.host_write(m_upload_motor_map);
	f.collision_state_buffer.host_write(m_upload_collision_state);

	if (m_upload_joints_dirty && !m_upload_joints.empty()) {
		f.joint_buffer.host_write(m_upload_joints);
	}

	if (!m_upload_impulses.empty()) {
		f.impulse_buffer.host_write(m_upload_impulses);
	}
}

auto gse::vbd::gpu_solver::read_grounded() const -> std::span<const std::uint32_t> {
	if (!m_buffers_created) {
		return {};
	}
	const auto& f = m_frames[m_dispatch_slot];
	if (!f.grounded_valid) {
		return {};
	}
	const auto bytes = f.grounded_readback_buffer.host_read();
	return std::span<const std::uint32_t>(
		reinterpret_cast<const std::uint32_t*>(bytes.data()),
		bytes.size() / sizeof(std::uint32_t)
	);
}

auto gse::vbd::gpu_solver::query_body_snapshot(const std::uint32_t body_index) const -> std::optional<body_state> {
	if (!m_buffers_created || body_index >= m_body_count) {
		return std::nullopt;
	}
	const auto& f = m_frames[m_dispatch_slot];
	if (!f.grounded_valid) {
		return std::nullopt;
	}
	const auto bytes = f.physics_snapshot_buffer.host_read();
	const auto* bodies = reinterpret_cast<const body_state*>(bytes.data());
	return bodies[body_index];
}

auto gse::vbd::gpu_solver::pending_dispatch() const -> bool {
	return m_pending_dispatch;
}

auto gse::vbd::gpu_solver::body_count() const -> std::uint32_t {
	return m_body_count;
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

auto gse::vbd::gpu_solver::snapshot_buffer(const std::uint32_t slot) const -> const gpu::buffer& {
	return m_frames[slot].physics_snapshot_buffer;
}

auto gse::vbd::gpu_solver::latest_snapshot_slot() const -> std::uint32_t {
	return 1 - m_dispatch_slot;
}

auto gse::vbd::gpu_solver::initialize_compute(run_context& ctx, const gpu::context::data& gpu_s) -> async::task<> {
	while (!gpu_s.device || !gpu_s.shader_registry || !gpu_s.bindless_textures) {
		co_await ctx.next_tick();
	}

	m_compute.predict_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, predict_entry::pod);
	m_compute.solve_color_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, solve_color_entry::pod);
	m_compute.update_lambda_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, update_lambda_entry::pod);
	m_compute.derive_velocities_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, derive_velocities_entry::pod);
	m_compute.finalize_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, finalize_entry::pod);
	m_compute.collision_reset_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, collision_reset_entry::pod);
	m_compute.collision_broad_phase_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, collision_broad_phase_entry::pod);
	m_compute.collision_narrow_phase_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, collision_narrow_phase_entry::pod);
	m_compute.collision_grid_build_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, collision_grid_build_entry::pod);
	m_compute.collision_build_adjacency_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, collision_build_adjacency_entry::pod);
	m_compute.update_joint_lambda_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, update_joint_lambda_entry::pod);
	m_compute.prepare_indirect_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, prepare_indirect_entry::pod);
	m_compute.prepare_contact_indirect_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, prepare_contact_indirect_entry::pod);
	m_compute.prepare_color_indirect_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, prepare_color_indirect_entry::pod);
	m_compute.freeze_jacobians_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, freeze_jacobians_entry::pod);
	m_compute.apply_jacobi_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, apply_jacobi_entry::pod);
	m_compute.apply_restitution_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, apply_restitution_entry::pod);
	m_compute.apply_impulses_pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, apply_impulses_entry::pod);

	create_buffers(gpu_s);

	for (auto& f : m_frames) {
		f.descriptors = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), predict_entry::pod);

		gpu::descriptor_writer(gpu::context::device_handle(*gpu_s.device), f.descriptors)
			.buffer<body_data>(f.body_buffer)
			.buffer<contact_data>(f.contact_buffer)
			.buffer<motor_data>(f.motor_buffer)
			.buffer<color_data>(f.color_buffer)
			.buffer<contact_offsets>(f.contact_offsets_buffer)
			.buffer<solve_state>(f.solve_state_buffer)
			.buffer<collision_pairs>(f.collision_pair_buffer)
			.buffer<collision_state>(f.collision_state_buffer)
			.buffer<warm_starts>(f.warm_start_buffer)
			.buffer<joint_data>(f.joint_buffer)
			.buffer<contact_counts>(f.contact_counts_buffer)
			.buffer<contact_adjacency>(f.contact_adjacency_buffer)
			.buffer<motor_map>(f.motor_map_buffer)
			.buffer<joint_offsets>(f.joint_offsets_buffer)
			.buffer<joint_counts>(f.joint_counts_buffer)
			.buffer<joint_adjacency>(f.joint_adjacency_buffer)
			.buffer<grid_data>(f.grid_buffer)
			.buffer<indirect_args>(f.indirect_dispatch_buffer)
			.buffer<frozen_jacobians>(f.frozen_jacobian_buffer)
			.buffer<solve_deltas>(f.solve_deltas_buffer)
			.buffer<grounded_bits>(f.grounded_buffer)
			.buffer<impulse_data>(f.impulse_buffer)
			.commit();
	}

	m_compute.initialized = true;
	co_return;
}

auto gse::vbd::gpu_solver::dispatch_compute(frame_context& ctx) -> async::task<> {
	if (!m_buffers_created || m_body_count == 0) {
		co_return;
	}

	auto& f = m_frames[m_dispatch_slot];
	auto& other = m_frames[1 - m_dispatch_slot];

	const std::uint32_t total = total_substeps();
	const time_step sub_dt = m_dt / static_cast<float>(total);
	const auto h_squared = sub_dt * sub_dt;

	auto ceil_div = [](const std::uint32_t a, const std::uint32_t b) {
		return (a + b - 1) / b;
	};

	auto make_pc = [this, sub_dt, h_squared](const std::uint32_t color_offset, const std::uint32_t color_count, const std::uint32_t substep, const std::uint32_t iteration, const float current_alpha, const std::uint32_t warm_start_count) {
		const auto& cfg = m_solver_cfg;
		return gpu::typed_push_constants<vbd_push_constants>{
			.data = {
				.body_count = m_body_count,
				.contact_count = max_contacts,
				.motor_count = m_motor_count,
				.color_offset = color_offset,
				.color_count = color_count,
				.warm_start_count = warm_start_count,
				.post_stabilize = cfg.post_stabilize ? 1u : 0u,
				.joint_count = m_joint_count,
				.impulse_count = m_impulse_count,
				.h_squared = h_squared,
				.dt = sub_dt,
				.beta = cfg.beta,
				.ang_beta = cfg.ang_beta,
				.linear_damping = 0.0f,
				.velocity_sleep_threshold = cfg.velocity_sleep_threshold,
				.angular_sleep_threshold = cfg.angular_sleep_threshold,
				.current_alpha = current_alpha,
				.collision_margin = cfg.collision_margin,
				.friction_coefficient = cfg.friction_coefficient,
				.penalty_min = cfg.penalty_min,
				.penalty_max = cfg.penalty_max,
				.gamma = cfg.gamma,
				.solver_alpha = cfg.alpha,
				.speculative_margin = cfg.speculative_margin,
				.stick_threshold = cfg.stick_threshold,
				.substep = substep,
				.iteration = iteration,
				.convergence_threshold = cfg.convergence_threshold.linear,
				.min_iterations = cfg.min_iterations,
				.grid_cell_size = m_grid_cell_size,
				.use_jacobi = cfg.use_jacobi ? 1u : 0u,
				.jacobi_omega = cfg.jacobi_omega,
				.restitution_threshold = cfg.restitution_threshold,
			},
			.stages = gpu::stage_flag::compute,
		};
	};

	const std::uint32_t body_workgroups = ceil_div(m_body_count, workgroup_size);
	const std::uint32_t reset_workgroups = ceil_div(std::max({ m_body_count, max_contacts, grid_table_size }), workgroup_size);
	const std::uint32_t joint_workgroups = ceil_div(std::max(m_joint_count, 1u), workgroup_size);
	constexpr std::uint32_t num_colors = max_colors;
	const std::uint32_t num_iterations = m_solver_cfg.iterations;
	const float solve_alpha = m_solver_cfg.post_stabilize ? 1.f : m_solver_cfg.alpha;
	const bool use_jacobi = m_solver_cfg.use_jacobi;
	const bool post_stabilize = m_solver_cfg.post_stabilize;
	const std::uint32_t joint_count = m_joint_count;

	constexpr std::size_t frozen_jacobian_clear_size = max_contacts * sizeof(frozen_jacobian);
	constexpr std::size_t solve_state_clear_size = max_bodies * solve_state_float4s_per_body * sizeof(float) * 4;
	constexpr std::size_t solve_deltas_clear_size = max_bodies * 2 * sizeof(float) * 4;
	gpu::pass<vbd_clear_state_buffers_stage>(ctx)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>()
		.record([&f](gpu::recording_context& rec) {
			rec.fill_buffer(f.frozen_jacobian_buffer, 0, frozen_jacobian_clear_size);
			rec.fill_buffer(f.solve_state_buffer, 0, solve_state_clear_size);
			rec.fill_buffer(f.solve_deltas_buffer, 0, solve_deltas_clear_size);
		});

	for (std::uint32_t sub = 0; sub < total; ++sub) {
		const std::uint32_t warm = (sub == 0) ? m_warm_start_count : 0u;

		gpu::pass<vbd_collision_reset_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.collision_reset_pipeline)
			.record([this, &f, sub, warm, make_pc, reset_workgroups](gpu::recording_context& rec) {
				rec.bind_descriptors(m_compute.collision_reset_pipeline, f.descriptors);
				rec.push(m_compute.collision_reset_pipeline, make_pc(0u, 0u, sub, 0u, 0.f, warm));
				rec.dispatch(reset_workgroups, 1, 1);
			});

		gpu::pass<vbd_grid_build_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.collision_grid_build_pipeline)
			.record([this, &f, sub, warm, make_pc, body_workgroups](gpu::recording_context& rec) {
				rec.bind_descriptors(m_compute.collision_grid_build_pipeline, f.descriptors);
				rec.push(m_compute.collision_grid_build_pipeline, make_pc(0u, 0u, sub, 0u, 0.f, warm));
				rec.dispatch(body_workgroups, 1, 1);
			});

		gpu::pass<vbd_broad_phase_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.collision_broad_phase_pipeline)
			.record([this, &f, sub, warm, make_pc, body_workgroups](gpu::recording_context& rec) {
				rec.bind_descriptors(m_compute.collision_broad_phase_pipeline, f.descriptors);
				rec.push(m_compute.collision_broad_phase_pipeline, make_pc(0u, 0u, sub, 0u, 0.f, warm));
				rec.dispatch(body_workgroups, 1, 1);
			});

		gpu::pass<vbd_prepare_indirect_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.prepare_indirect_pipeline)
			.record([this, &f, sub, warm, make_pc](gpu::recording_context& rec) {
				rec.bind_descriptors(m_compute.prepare_indirect_pipeline, f.descriptors);
				rec.push(m_compute.prepare_indirect_pipeline, make_pc(0u, 0u, sub, 0u, 0.f, warm));
				rec.dispatch(1, 1, 1);
			});

		gpu::pass<vbd_narrow_phase_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.collision_narrow_phase_pipeline)
			.record([this, &f, sub, warm, make_pc](gpu::recording_context& rec) {
				rec.bind_descriptors(m_compute.collision_narrow_phase_pipeline, f.descriptors);
				rec.push(m_compute.collision_narrow_phase_pipeline, make_pc(0u, 0u, sub, 0u, 0.f, warm));
				rec.dispatch_indirect(f.indirect_dispatch_buffer, 0);
			});

		gpu::pass<vbd_prepare_contact_indirect_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.prepare_contact_indirect_pipeline)
			.record([this, &f, sub, warm, make_pc](gpu::recording_context& rec) {
				rec.bind_descriptors(m_compute.prepare_contact_indirect_pipeline, f.descriptors);
				rec.push(m_compute.prepare_contact_indirect_pipeline, make_pc(0u, 0u, sub, 0u, 0.f, warm));
				rec.dispatch(1, 1, 1);
			});

		gpu::pass<vbd_build_adjacency_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.collision_build_adjacency_pipeline)
			.record([this, &f, sub, warm, make_pc](gpu::recording_context& rec) {
				rec.bind_descriptors(m_compute.collision_build_adjacency_pipeline, f.descriptors);
				rec.push(m_compute.collision_build_adjacency_pipeline, make_pc(0u, 0u, sub, 0u, 0.f, warm));
				rec.dispatch(1, 1, 1);
			});

		if (sub == 0 && m_impulse_count > 0) {
			const std::uint32_t impulse_workgroups = ceil_div(m_impulse_count, workgroup_size);
			gpu::pass<vbd_apply_impulses_stage>(ctx)
				.on(gpu::queue_type::compute)
				.in_chain<vbd_solve_chain>()
				.pipeline(m_compute.apply_impulses_pipeline)
				.record([this, &f, sub, warm, make_pc, impulse_workgroups](gpu::recording_context& rec) {
					rec.bind_descriptors(m_compute.apply_impulses_pipeline, f.descriptors);
					rec.push(m_compute.apply_impulses_pipeline, make_pc(0u, 0u, sub, 0u, 0.f, warm));
					rec.dispatch(impulse_workgroups, 1, 1);
				});
		}

		gpu::pass<vbd_predict_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.predict_pipeline)
			.record([this, &f, sub, warm, make_pc, body_workgroups](gpu::recording_context& rec) {
				rec.bind_descriptors(m_compute.predict_pipeline, f.descriptors);
				rec.push(m_compute.predict_pipeline, make_pc(0u, 0u, sub, 0u, 0.f, warm));
				rec.dispatch(body_workgroups, 1, 1);
			});

		gpu::pass<vbd_freeze_jacobians_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.freeze_jacobians_pipeline)
			.record([this, &f, sub, warm, make_pc](gpu::recording_context& rec) {
				rec.bind_descriptors(m_compute.freeze_jacobians_pipeline, f.descriptors);
				rec.push(m_compute.freeze_jacobians_pipeline, make_pc(0u, 0u, sub, 0u, 0.f, warm));
				rec.dispatch_indirect(f.indirect_dispatch_buffer, 3 * sizeof(std::uint32_t));
			});

		gpu::pass<vbd_solve_iterations_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.solve_color_pipeline)
			.record([this, &f, sub, warm, make_pc, body_workgroups, joint_workgroups, num_iterations, solve_alpha, use_jacobi, joint_count](gpu::recording_context& rec) {
				for (std::uint32_t it = 0; it < num_iterations; ++it) {
					rec.bind(m_compute.solve_color_pipeline);
					rec.bind_descriptors(m_compute.solve_color_pipeline, f.descriptors);
					auto color_pc = make_pc(0u, num_colors, sub, it, solve_alpha, warm);

					if (use_jacobi) {
						rec.push(m_compute.solve_color_pipeline, color_pc);
						rec.dispatch(body_workgroups, 1, 1);
						rec.barrier(gpu::barrier_scope::compute_to_compute);

						rec.bind(m_compute.apply_jacobi_pipeline);
						rec.bind_descriptors(m_compute.apply_jacobi_pipeline, f.descriptors);
						rec.push(m_compute.apply_jacobi_pipeline, make_pc(0u, 0u, sub, it, solve_alpha, warm));
						rec.dispatch(body_workgroups, 1, 1);
						rec.barrier(gpu::barrier_scope::compute_to_compute);
					}
					else {
						for (std::uint32_t color = 0; color < num_colors; ++color) {
							color_pc.data.color_offset = color;
							rec.push(m_compute.solve_color_pipeline, color_pc);
							rec.dispatch_indirect(f.indirect_dispatch_buffer, (2 + color) * 3 * sizeof(std::uint32_t));
							if (color + 1 < num_colors) {
								rec.barrier(gpu::barrier_scope::compute_to_compute);
							}
						}
						rec.barrier(gpu::barrier_scope::compute_to_compute);
					}

					rec.bind(m_compute.update_lambda_pipeline);
					rec.bind_descriptors(m_compute.update_lambda_pipeline, f.descriptors);
					rec.push(m_compute.update_lambda_pipeline, make_pc(0u, 0u, sub, it, solve_alpha, warm));
					rec.dispatch_indirect(f.indirect_dispatch_buffer, 3 * sizeof(std::uint32_t));
					if (joint_count > 0) {
						rec.bind(m_compute.update_joint_lambda_pipeline);
						rec.bind_descriptors(m_compute.update_joint_lambda_pipeline, f.descriptors);
						rec.push(m_compute.update_joint_lambda_pipeline, make_pc(0u, 0u, sub, it, solve_alpha, warm));
						rec.dispatch(joint_workgroups, 1, 1);
					}
					rec.barrier(gpu::barrier_scope::compute_to_indirect);
				}
			});

		gpu::pass<vbd_derive_velocities_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.derive_velocities_pipeline)
			.record([this, &f, sub, warm, make_pc, body_workgroups, num_iterations, solve_alpha](gpu::recording_context& rec) {
				rec.bind_descriptors(m_compute.derive_velocities_pipeline, f.descriptors);
				rec.push(m_compute.derive_velocities_pipeline, make_pc(0u, 0u, sub, num_iterations, solve_alpha, warm));
				rec.dispatch(body_workgroups, 1, 1);
			});

		gpu::pass<vbd_apply_restitution_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.apply_restitution_pipeline)
			.record([this, &f, sub, warm, make_pc, num_iterations](gpu::recording_context& rec) {
				auto restitution_pc = make_pc(0u, num_colors, sub, num_iterations, 0.f, warm);
				rec.bind_descriptors(m_compute.apply_restitution_pipeline, f.descriptors);
				for (std::uint32_t color = 0; color < num_colors; ++color) {
					restitution_pc.data.color_offset = color;
					rec.push(m_compute.apply_restitution_pipeline, restitution_pc);
					rec.dispatch_indirect(f.indirect_dispatch_buffer, (2 + color) * 3 * sizeof(std::uint32_t));
					if (color + 1 < num_colors) {
						rec.barrier(gpu::barrier_scope::compute_to_compute);
					}
				}
			});

		if (post_stabilize) {
			gpu::pass<vbd_post_stabilize_stage>(ctx)
				.on(gpu::queue_type::compute)
				.in_chain<vbd_solve_chain>()
				.pipeline(m_compute.prepare_color_indirect_pipeline)
				.record([this, &f, sub, warm, make_pc, body_workgroups, num_iterations, use_jacobi](gpu::recording_context& rec) {
					rec.bind_descriptors(m_compute.prepare_color_indirect_pipeline, f.descriptors);
					rec.push(m_compute.prepare_color_indirect_pipeline, make_pc(0u, 0u, sub, num_iterations, 0.f, warm));
					rec.dispatch(1, 1, 1);
					rec.barrier(gpu::barrier_scope::compute_to_indirect);

					auto color_pc = make_pc(0u, num_colors, sub, num_iterations, 0.f, warm);

					rec.bind(m_compute.solve_color_pipeline);
					rec.bind_descriptors(m_compute.solve_color_pipeline, f.descriptors);

					if (use_jacobi) {
						rec.push(m_compute.solve_color_pipeline, color_pc);
						rec.dispatch(body_workgroups, 1, 1);
						rec.barrier(gpu::barrier_scope::compute_to_compute);

						rec.bind(m_compute.apply_jacobi_pipeline);
						rec.bind_descriptors(m_compute.apply_jacobi_pipeline, f.descriptors);
						rec.push(m_compute.apply_jacobi_pipeline, make_pc(0u, 0u, sub, num_iterations, 0.f, warm));
						rec.dispatch(body_workgroups, 1, 1);
					}
					else {
						for (std::uint32_t color = 0; color < num_colors; ++color) {
							color_pc.data.color_offset = color;
							rec.push(m_compute.solve_color_pipeline, color_pc);
							rec.dispatch_indirect(f.indirect_dispatch_buffer, (2 + color) * 3 * sizeof(std::uint32_t));
							if (color + 1 < num_colors) {
								rec.barrier(gpu::barrier_scope::compute_to_compute);
							}
						}
					}
				});
		}

		gpu::pass<vbd_finalize_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.finalize_pipeline)
			.record([this, &f, sub, warm, make_pc, body_workgroups](gpu::recording_context& rec) {
				rec.bind_descriptors(m_compute.finalize_pipeline, f.descriptors);
				rec.push(m_compute.finalize_pipeline, make_pc(0u, 0u, sub, 0u, 0.f, warm));
				rec.dispatch(body_workgroups, 1, 1);
			});
	}

	{
		const std::size_t body_copy_size = m_body_count * sizeof(body_state);
		const std::size_t joint_copy_size = joint_count * sizeof(joint_constraint);
		constexpr std::size_t grounded_copy_size = grounded_bits_uints * sizeof(std::uint32_t);

		gpu::pass<vbd_state_copy_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.record([&f, &other, body_copy_size, joint_copy_size, joint_count](gpu::recording_context& rec) {
				if (joint_count > 0) {
					rec.copy_buffer(f.joint_buffer, other.joint_buffer, joint_copy_size);
				}

				rec.copy_buffer(f.body_buffer, other.body_buffer, body_copy_size);
				rec.copy_buffer(f.body_buffer, f.physics_snapshot_buffer, body_copy_size);
				rec.copy_buffer(f.grounded_buffer, f.grounded_readback_buffer, grounded_copy_size);
			});
	}

	f.grounded_valid = true;
	m_pending_dispatch = false;
	m_body_buffers_seeded = true;
	m_seeded_body_count = m_body_count;
	m_dispatch_slot = 1 - m_dispatch_slot;
	co_return;
}

auto gse::vbd::gpu_solver::compute_initialized() const -> bool {
	return m_compute.initialized;
}

auto gse::vbd::gpu_solver::buffers_created() const -> bool {
	return m_buffers_created;
}
