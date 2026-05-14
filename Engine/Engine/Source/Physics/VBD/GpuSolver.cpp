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
		solve_deltas
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

gse::vbd::gpu_solver::~gpu_solver() {
	if (m_compute.initialized) {
		for (auto& f : m_frames) {
			f.queue.wait();
		}
	}
}

auto gse::vbd::gpu_solver::create_buffers(const gpu::context::data& ctx) -> void {
	constexpr auto storage_src = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_src;
	constexpr auto storage_dst = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst;
	constexpr auto storage_src_dst = storage_src | gpu::buffer_flag::transfer_dst;
	constexpr std::size_t color_buffer_size = max_colors * sizeof(std::uint32_t) * 2 + max_bodies * sizeof(std::uint32_t);
	constexpr std::size_t collision_pair_size = sizeof(std::uint32_t) + max_collision_pairs * 2 * sizeof(std::uint32_t);
	constexpr std::size_t collision_state_size = collision_state_uints * sizeof(std::uint32_t);
	constexpr std::size_t warm_start_size = max_contacts * sizeof(contact_constraint);
	constexpr std::size_t joint_buffer_size = max_joints * sizeof(joint_constraint);
	constexpr std::size_t readback_size = max_bodies * sizeof(body_state) + max_contacts * sizeof(contact_constraint) + collision_state_uints * sizeof(std::uint32_t) + max_joints * sizeof(joint_constraint);

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
			.size = std::max<std::size_t>(warm_start_size, 16),
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

		f.readback_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = readback_size,
			.usage = storage_dst
		});
		std::memset(f.readback_buffer.mapped(), 0, f.readback_buffer.size());

		f.physics_snapshot_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = max_bodies * sizeof(body_state),
			.usage = storage_dst
		});
		std::memset(f.physics_snapshot_buffer.mapped(), 0, f.physics_snapshot_buffer.size());

		f.indirect_dispatch_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = (2 + max_colors) * 3 * sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage | gpu::buffer_flag::indirect
		});
		std::memset(f.indirect_dispatch_buffer.mapped(), 0, f.indirect_dispatch_buffer.size());

		f.frozen_jacobian_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = max_contacts * sizeof(frozen_jacobian),
			.usage = gpu::buffer_flag::storage
		});

		f.solve_deltas_buffer = gpu::buffer::create(ctx.device->allocator(), {
			.size = max_bodies * 2 * sizeof(float) * 4,
			.usage = gpu::buffer_flag::storage
		});
	}

	m_upload_motors.reserve(max_motors);
	m_upload_warm_starts.reserve(max_contacts);
	m_upload_joints.reserve(max_joints);
	m_upload_motor_map.reserve(max_bodies);
	m_upload_collision_state.reserve(collision_state_uints);
	m_staged_contacts.reserve(max_contacts);
	m_staged_joints.reserve(max_joints);
	m_staged_bodies.reserve(max_bodies);

	m_buffers_created = true;
}

auto gse::vbd::gpu_solver::upload(const std::span<const body_state> bodies, const std::span<const velocity_motor_constraint> motors, const std::span<const joint_constraint> joints, const std::span<const contact_constraint> prev_contacts, const solver_config& solver_cfg, const time_step dt, const int steps) -> void {
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

	auto* gpu_bodies = reinterpret_cast<body_state*>(m_frames[m_dispatch_slot].body_buffer.mapped());
	std::ranges::copy_n(bodies.begin(), m_body_count, gpu_bodies);

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

	const auto warm_start_limit = std::min(prev_contacts.size(), static_cast<std::size_t>(max_contacts));
	m_upload_warm_starts.clear();
	m_upload_warm_starts.reserve(warm_start_limit);
	std::ranges::copy_if(
		prev_contacts.first(warm_start_limit),
		std::back_inserter(m_upload_warm_starts),
		[this](const contact_constraint& ws) {
			return ws.body_a < m_body_count && ws.body_b < m_body_count;
		}
	);
	m_warm_start_count = static_cast<std::uint32_t>(m_upload_warm_starts.size());

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

	m_pending_dispatch = true;
}

auto gse::vbd::gpu_solver::total_substeps() const -> std::uint32_t {
	return 1 * m_steps;
}

auto gse::vbd::gpu_solver::commit_upload() -> void {
	if (!m_buffers_created || !m_pending_dispatch || m_body_count == 0) {
		if (m_frame_count < 5) {
			log::println(
				log::level::warning,
				log::category::physics,
				"commit_upload skipped: created={} pending={} bodies={}",
				m_buffers_created,
				m_pending_dispatch,
				m_body_count
			);
		}
		return;
	}
	if (m_frame_count < 5) {
		log::println(
			log::category::physics,
			"commit_upload: bodies={} motors={} joints={} warm_starts={}",
			m_body_count,
			m_motor_count,
			m_joint_count,
			m_warm_start_count
		);
	}

	auto& f = m_frames[m_dispatch_slot];
	gse::memcpy(f.motor_buffer.mapped(), m_upload_motors);
	gse::memcpy(f.motor_map_buffer.mapped(), m_upload_motor_map);
	gse::memcpy(f.collision_state_buffer.mapped(), m_upload_collision_state);

	if (!m_upload_warm_starts.empty()) {
		gse::memcpy(f.warm_start_buffer.mapped(), m_upload_warm_starts);
	}

	if (!m_upload_joints.empty()) {
		gse::memcpy(f.joint_buffer.mapped(), m_upload_joints);
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
	const auto* bodies_src = reinterpret_cast<const body_state*>(rb);
	const auto* contacts_src = reinterpret_cast<const contact_constraint*>(rb + max_bodies * sizeof(body_state));
	const auto* state_src = reinterpret_cast<const std::uint32_t*>(rb + max_bodies * sizeof(body_state) + max_contacts * sizeof(contact_constraint));
	const auto* joints_src = reinterpret_cast<const joint_constraint*>(rb + max_bodies * sizeof(body_state) + max_contacts * sizeof(contact_constraint) + collision_state_uints * sizeof(std::uint32_t));

	const std::uint32_t gpu_contact_count = state_src[0];
	m_staged_contact_count = std::min(gpu_contact_count, max_contacts);
	m_contact_count = m_staged_contact_count;

	m_staged_bodies.assign(bodies_src, bodies_src + info.body_count);
	m_staged_contacts.assign(contacts_src, contacts_src + m_staged_contact_count);

	m_staged_body_count = info.body_count;
	m_staged_joint_count = info.joint_count;
	if (m_staged_joint_count > 0) {
		m_staged_joints.assign(joints_src, joints_src + m_staged_joint_count);
	}
		else {
		m_staged_joints.clear();
	}

	info = {};
	m_staged_valid = true;
	f.readback_pending = false;
}

auto gse::vbd::gpu_solver::readback(const std::span<body_state> bodies, std::vector<contact_constraint>& contacts_out, const std::span<joint_constraint> joints_out) -> void {
	if (!m_staged_valid) {
		return;
	}

	const std::uint32_t count = std::min(m_staged_body_count, static_cast<std::uint32_t>(bodies.size()));
	std::ranges::copy_n(m_staged_bodies.begin(), count, bodies.begin());

	contacts_out.assign(m_staged_contacts.begin(), m_staged_contacts.begin() + m_staged_contact_count);

	const std::uint32_t jcount = std::min(m_staged_joint_count, static_cast<std::uint32_t>(joints_out.size()));
	std::ranges::copy_n(m_staged_joints.begin(), jcount, joints_out.begin());

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

	create_buffers(gpu_s);

	for (auto& f : m_frames) {
		f.queue = gpu::compute_queue::create(*gpu_s.device, gpu_s.bindless_textures.get());
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
			.commit();
	}

	m_compute.initialized = true;
	co_return;
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

	ingest_stage("gpu:vbd_collision", timing_slot::begin, timing_slot::after_collision);
	ingest_stage("gpu:vbd_predict", timing_slot::after_collision, timing_slot::after_predict);
	ingest_stage("gpu:vbd_solve", timing_slot::after_predict, timing_slot::after_solve);
	ingest_stage("gpu:vbd_velocity", timing_slot::after_solve, timing_slot::after_velocity);
	ingest_stage("gpu:vbd_finalize", timing_slot::after_velocity, timing_slot::after_finalize);

	f.queue.begin("compute_queue.vbd_solver");

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

	auto make_pc = [&](const std::uint32_t color_offset, const std::uint32_t color_count, const std::uint32_t substep, const std::uint32_t iteration, const float current_alpha, const std::uint32_t warm_start_count) {
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

	auto bind_and_push = [&](const gpu::pipeline& pipeline, const std::uint32_t color_offset, const std::uint32_t color_count, const std::uint32_t substep, const std::uint32_t iteration, const float current_alpha, const std::uint32_t warm_start_count) {
		f.queue.bind_pipeline(pipeline);
		f.queue.bind_descriptors(pipeline, f.descriptors);
		const auto pc = make_pc(color_offset, color_count, substep, iteration, current_alpha, warm_start_count);
		f.queue.push(pipeline, pc);
	};

	const std::uint32_t body_workgroups = ceil_div(m_body_count, workgroup_size);
	constexpr std::uint32_t num_colors = max_colors;
	const std::uint32_t num_iterations = cfg.iterations;
	const float solve_alpha = cfg.post_stabilize ? 1.f : cfg.alpha;

	for (std::uint32_t sub = 0; sub < total; ++sub) {
		const std::uint32_t substep_warm_start_count = (sub == 0) ? m_warm_start_count : 0u;

		bind_and_push(m_compute.collision_reset_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch(ceil_div(std::max({m_body_count, max_contacts, grid_table_size}), workgroup_size), 1, 1);
		f.queue.barrier(gpu::barrier_scope::compute_to_compute);

		bind_and_push(m_compute.collision_grid_build_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch(body_workgroups, 1, 1);
		f.queue.barrier(gpu::barrier_scope::compute_to_compute);

		bind_and_push(m_compute.collision_broad_phase_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch(body_workgroups, 1, 1);
		f.queue.barrier(gpu::barrier_scope::compute_to_compute);

		bind_and_push(m_compute.prepare_indirect_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch(1, 1, 1);
		f.queue.barrier(gpu::barrier_scope::compute_to_indirect);

		bind_and_push(m_compute.collision_narrow_phase_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch_indirect(f.indirect_dispatch_buffer, 0);
		f.queue.barrier(gpu::barrier_scope::compute_to_compute);

		bind_and_push(m_compute.prepare_contact_indirect_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch(1, 1, 1);
		f.queue.barrier(gpu::barrier_scope::compute_to_indirect);

		bind_and_push(m_compute.collision_build_adjacency_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch(1, 1, 1);
		f.queue.barrier(gpu::barrier_scope::compute_to_indirect);

		f.queue.mark_timing(timing_slot::after_collision);

		bind_and_push(m_compute.predict_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch(body_workgroups, 1, 1);
		f.queue.barrier(gpu::barrier_scope::compute_to_compute);

		bind_and_push(m_compute.freeze_jacobians_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch_indirect(f.indirect_dispatch_buffer, 3 * sizeof(std::uint32_t));
		f.queue.barrier(gpu::barrier_scope::compute_to_compute);

		f.queue.mark_timing(timing_slot::after_predict);

		for (std::uint32_t iterations = 0; iterations < num_iterations; ++iterations) {
			bind_and_push(m_compute.solve_color_pipeline, 0u, num_colors, sub, iterations, solve_alpha, substep_warm_start_count);
			auto color_pc = make_pc(0u, num_colors, sub, iterations, solve_alpha, substep_warm_start_count);

			if (cfg.use_jacobi) {
				f.queue.push(m_compute.solve_color_pipeline, color_pc);
				f.queue.dispatch(body_workgroups, 1, 1);
				f.queue.barrier(gpu::barrier_scope::compute_to_compute);

				bind_and_push(m_compute.apply_jacobi_pipeline, 0u, 0u, sub, iterations, solve_alpha, substep_warm_start_count);
				f.queue.dispatch(body_workgroups, 1, 1);
				f.queue.barrier(gpu::barrier_scope::compute_to_compute);
			}
		else {
				for (std::uint32_t color = 0; color < num_colors; ++color) {
					color_pc.data.color_offset = color;
					f.queue.push(m_compute.solve_color_pipeline, color_pc);
					f.queue.dispatch_indirect(f.indirect_dispatch_buffer, (2 + color) * 3 * sizeof(std::uint32_t));
					if (color + 1 < num_colors) {
						f.queue.barrier(gpu::barrier_scope::compute_to_compute);
					}
				}

				f.queue.barrier(gpu::barrier_scope::compute_to_compute);
			}

			bind_and_push(m_compute.update_lambda_pipeline, 0u, 0u, sub, iterations, solve_alpha, substep_warm_start_count);
			f.queue.dispatch_indirect(f.indirect_dispatch_buffer, 3 * sizeof(std::uint32_t));
			if (m_joint_count > 0) {
				bind_and_push(m_compute.update_joint_lambda_pipeline, 0u, 0u, sub, iterations, solve_alpha, substep_warm_start_count);
				f.queue.dispatch(ceil_div(m_joint_count, workgroup_size), 1, 1);
			}
			f.queue.barrier(gpu::barrier_scope::compute_to_indirect);
		}

		f.queue.mark_timing(timing_slot::after_solve);

		bind_and_push(m_compute.derive_velocities_pipeline, 0u, 0u, sub, num_iterations, solve_alpha, substep_warm_start_count);
		f.queue.dispatch(body_workgroups, 1, 1);
		f.queue.barrier(gpu::barrier_scope::compute_to_compute);

		auto restitution_pc = make_pc(0u, num_colors, sub, num_iterations, 0.f, substep_warm_start_count);
		f.queue.bind_pipeline(m_compute.apply_restitution_pipeline);
		f.queue.bind_descriptors(m_compute.apply_restitution_pipeline, f.descriptors);
		for (std::uint32_t color = 0; color < num_colors; ++color) {
			restitution_pc.data.color_offset = color;
			f.queue.push(m_compute.apply_restitution_pipeline, restitution_pc);
			f.queue.dispatch_indirect(f.indirect_dispatch_buffer, (2 + color) * 3 * sizeof(std::uint32_t));
			if (color + 1 < num_colors) {
				f.queue.barrier(gpu::barrier_scope::compute_to_compute);
			}
		}
		f.queue.barrier(gpu::barrier_scope::compute_to_compute);

		if (cfg.post_stabilize) {
			bind_and_push(m_compute.prepare_color_indirect_pipeline, 0u, 0u, sub, num_iterations, 0.f, substep_warm_start_count);
			f.queue.dispatch(1, 1, 1);
			f.queue.barrier(gpu::barrier_scope::compute_to_indirect);

			auto color_pc = make_pc(0u, num_colors, sub, num_iterations, 0.f, substep_warm_start_count);

			f.queue.bind_pipeline(m_compute.solve_color_pipeline);
			f.queue.bind_descriptors(m_compute.solve_color_pipeline, f.descriptors);

			if (cfg.use_jacobi) {
				f.queue.push(m_compute.solve_color_pipeline, color_pc);
				f.queue.dispatch(body_workgroups, 1, 1);
				f.queue.barrier(gpu::barrier_scope::compute_to_compute);

				bind_and_push(m_compute.apply_jacobi_pipeline, 0u, 0u, sub, num_iterations, 0.f, substep_warm_start_count);
				f.queue.dispatch(body_workgroups, 1, 1);
				f.queue.barrier(gpu::barrier_scope::compute_to_compute);
			}
			else {
				for (std::uint32_t color = 0; color < num_colors; ++color) {
					color_pc.data.color_offset = color;
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

		bind_and_push(m_compute.finalize_pipeline, 0u, 0u, sub, 0u, 0.f, substep_warm_start_count);
		f.queue.dispatch(body_workgroups, 1, 1);
		f.queue.barrier(gpu::barrier_scope::compute_to_compute);

		f.queue.mark_timing(timing_slot::after_finalize);
	}

	f.queue.end_timing();

	f.queue.barrier(gpu::barrier_scope::compute_to_transfer);

	const std::size_t body_copy_size = m_body_count * sizeof(body_state);
	f.queue.copy_buffer({
		.src = f.body_buffer,
		.dst = f.readback_buffer,
		.size = body_copy_size
	});

	constexpr std::size_t contact_dst_base = max_bodies * sizeof(body_state);
	constexpr std::size_t contact_copy_size = max_contacts * sizeof(contact_constraint);
	f.queue.copy_buffer({
		.src = f.contact_buffer,
		.dst = f.readback_buffer,
		.dst_offset = contact_dst_base,
		.size = contact_copy_size
	});

	constexpr std::size_t count_dst = contact_dst_base + contact_copy_size;
	f.queue.copy_buffer({
		.src = f.collision_state_buffer,
		.dst = f.readback_buffer,
		.dst_offset = count_dst,
		.size = collision_state_uints * sizeof(std::uint32_t)
	});

	if (m_joint_count > 0) {
		constexpr std::size_t joint_dst = count_dst + collision_state_uints * sizeof(std::uint32_t);
		const std::size_t joint_copy_size = m_joint_count * sizeof(joint_constraint);
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
