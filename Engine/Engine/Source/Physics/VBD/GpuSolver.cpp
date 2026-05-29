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
	struct [[= shaders::shader_struct]] vbd_push_constants {
		solver_config cfg;
		std::uint32_t body_count;
		std::uint32_t contact_count;
		std::uint32_t motor_count;
		std::uint32_t joint_count;
		std::uint32_t impulse_count;
		std::uint32_t color_offset;
		std::uint32_t color_count;
		std::uint32_t warm_start_count;
		std::uint32_t substep;
		std::uint32_t iteration;
		time_squared h_squared;
		time_step dt;
		float current_alpha;
		gap grid_cell_size;
	};

	struct [[
		= shaders::binding<0, 0>{},
		= shaders::ssbo_readwrite
	]] body_data {
		using element = body_state;
	};
	struct [[
		= shaders::binding<0, 1>{},
		= shaders::ssbo_readwrite
	]] contact_data {
		using element = contact_constraint;
	};
	struct [[
		= shaders::binding<0, 2>{},
		= shaders::ssbo_readonly
	]] motor_data {
		using element = velocity_motor_constraint;
	};
	struct [[
		= shaders::binding<0, 3>{},
		= shaders::ssbo_readwrite
	]] color_data {
		using element = std::uint32_t;
	};
	struct [[
		= shaders::binding<0, 4>{},
		= shaders::ssbo_readwrite
	]] contact_offsets {
		using element = std::uint32_t;
	};
	struct [[
		= shaders::binding<0, 5>{},
		= shaders::ssbo_readwrite
	]] solve_state {
		using element = vec4f;
	};
	struct [[
		= shaders::binding<0, 6>{},
		= shaders::ssbo_readwrite
	]] collision_pairs {
		using element = std::uint32_t;
	};
	struct [[
		= shaders::binding<0, 7>{},
		= shaders::ssbo_readwrite
	]] collision_state {
		using element = std::uint32_t;
	};
	struct [[
		= shaders::binding<0, 8>{},
		= shaders::ssbo_readonly
	]] warm_starts {
		using element = contact_constraint;
	};
	struct [[
		= shaders::binding<0, 9>{},
		= shaders::ssbo_readwrite
	]] joint_data {
		using element = joint_constraint;
	};
	struct [[
		= shaders::binding<0, 10>{},
		= shaders::ssbo_readwrite
	]] contact_counts {
		using element = std::uint32_t;
	};
	struct [[
		= shaders::binding<0, 11>{},
		= shaders::ssbo_readwrite
	]] contact_adjacency {
		using element = std::uint32_t;
	};
	struct [[
		= shaders::binding<0, 12>{},
		= shaders::ssbo_readwrite
	]] motor_map {
		using element = std::uint32_t;
	};
	struct [[
		= shaders::binding<0, 13>{},
		= shaders::ssbo_readwrite
	]] joint_offsets {
		using element = std::uint32_t;
	};
	struct [[
		= shaders::binding<0, 14>{},
		= shaders::ssbo_readwrite
	]] joint_counts {
		using element = std::uint32_t;
	};
	struct [[
		= shaders::binding<0, 15>{},
		= shaders::ssbo_readwrite
	]] joint_adjacency {
		using element = std::uint32_t;
	};
	struct [[
		= shaders::binding<0, 16>{},
		= shaders::ssbo_readwrite
	]] grid_data {
		using element = std::uint32_t;
	};
	struct [[
		= shaders::binding<0, 17>{},
		= shaders::ssbo_readwrite
	]] indirect_args {
		using element = dispatch_args;
	};
	struct [[
		= shaders::binding<0, 18>{},
		= shaders::ssbo_readwrite
	]] frozen_jacobians {
		using element = frozen_jacobian;
	};
	struct [[
		= shaders::binding<0, 19>{},
		= shaders::ssbo_readwrite
	]] solve_deltas {
		using element = vec4f;
	};
	struct [[
		= shaders::binding<0, 20>{},
		= shaders::ssbo_readwrite
	]] grounded_bits {
		using element = std::uint32_t;
	};
	struct [[
		= shaders::binding<0, 21>{},
		= shaders::ssbo_readonly
	]] impulse_data {
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
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<limits.workgroup_size>,
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
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<limits.adjacency_workgroup_size>,
		gpu::push_constant<vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id, gpu::group_thread_id>
	>;

	using collision_build_coloring_entry = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/collision_build_coloring">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<limits.adjacency_workgroup_size>,
		gpu::push_constant<vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id, gpu::group_thread_id>
	>;

	using prepare_indirect_entry = gpu::compute_entry<gpu::body_path<"VBDPhysics/vbd_prepare_indirect">, gpu::types<shader_types>, gpu::bindings<shader_binding_types>, gpu::helpers<"VBDPhysics/vbd_shared">, gpu::threads<1>, gpu::push_constant<vbd_push_constants>>;

	using prepare_contact_indirect_entry = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_prepare_contact_indirect">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<1>,
		gpu::push_constant<vbd_push_constants>
	>;

	using prepare_color_indirect_entry = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_prepare_color_indirect">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<16>,
		gpu::push_constant<vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using vbd_bindings = gpu::binding_args<shader_binding_types>;
}

auto gse::vbd::gpu_solver::create_buffers(const gpu::context::data& ctx) -> void {
	constexpr auto storage_src = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_src;
	constexpr auto storage_dst = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst;
	constexpr auto storage_src_dst = storage_src | gpu::buffer_flag::transfer_dst;
	constexpr std::size_t color_buffer_size =
		limits.max_colors * sizeof(std::uint32_t) * 2 + limits.max_bodies * sizeof(std::uint32_t);
	constexpr std::size_t collision_pair_size =
		sizeof(std::uint32_t) + limits.max_collision_pairs * 2 * sizeof(std::uint32_t);
	constexpr std::size_t collision_state_size =
		(limits.collision_state_header_uints +
		 limits.max_narrow_phase_debug_records * limits.narrow_phase_debug_record_uints) *
		sizeof(std::uint32_t);
	constexpr std::size_t joint_buffer_size = limits.max_joints * sizeof(joint_constraint);

	for (auto& f : m_frames) {
		f.body_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = limits.max_bodies * sizeof(body_state),
				.usage = storage_src_dst
			},
			"vbd.body"
		);

		f.contact_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = limits.max_contacts * sizeof(contact_constraint),
				.usage = storage_src
			},
			"vbd.contact"
		);

		f.motor_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = limits.max_motors * sizeof(velocity_motor_constraint),
				.usage = gpu::buffer_flag::storage
			},
			"vbd.motor"
		);

		f.color_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = color_buffer_size,
				.usage = gpu::buffer_flag::storage
			},
			"vbd.color"
		);

		f.contact_offsets_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = limits.max_bodies * sizeof(std::uint32_t),
				.usage = gpu::buffer_flag::storage
			},
			"vbd.contact_offsets"
		);
		f.contact_counts_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = limits.max_bodies * sizeof(std::uint32_t),
				.usage = gpu::buffer_flag::storage
			},
			"vbd.contact_counts"
		);
		f.contact_adjacency_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = limits.max_contacts * 2 * sizeof(std::uint32_t),
				.usage = gpu::buffer_flag::storage
			},
			"vbd.contact_adjacency"
		);
		f.motor_map_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = limits.max_bodies * sizeof(std::uint32_t),
				.usage = gpu::buffer_flag::storage
			},
			"vbd.motor_map"
		);
		f.joint_offsets_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = limits.max_bodies * sizeof(std::uint32_t),
				.usage = gpu::buffer_flag::storage
			},
			"vbd.joint_offsets"
		);
		f.joint_counts_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = limits.max_bodies * sizeof(std::uint32_t),
				.usage = gpu::buffer_flag::storage
			},
			"vbd.joint_counts"
		);
		f.joint_adjacency_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = limits.max_joints * 2 * sizeof(std::uint32_t),
				.usage = gpu::buffer_flag::storage
			},
			"vbd.joint_adjacency"
		);

		f.solve_state_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = limits.max_bodies * limits.solve_state_float4s_per_body * sizeof(float) * 4,
				.usage = gpu::buffer_flag::storage
			},
			"vbd.solve_state"
		);

		f.collision_pair_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = collision_pair_size,
				.usage = gpu::buffer_flag::storage
			},
			"vbd.collision_pair"
		);

		f.collision_state_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = collision_state_size,
				.usage = storage_src
			},
			"vbd.collision_state"
		);
		f.collision_state_buffer.buffer().host_zero();
		f.collision_state_buffer.buffer().clear_host_dirty();

		f.warm_start_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = 16,
				.usage = gpu::buffer_flag::storage
			},
			"vbd.warm_start"
		);

		f.joint_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = std::max<std::size_t>(joint_buffer_size, 16),
				.usage = storage_src
			},
			"vbd.joint"
		);

		constexpr std::size_t grid_buffer_size = (1 + limits.grid_table_size + limits.max_bodies * 8 * 2) * sizeof(std::uint32_t);
		f.grid_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = grid_buffer_size,
				.usage = gpu::buffer_flag::storage
			},
			"vbd.grid"
		);

		f.physics_snapshot_buffer = ctx.device->allocator().create_buffer(
			{
				.size = limits.max_bodies * sizeof(body_state),
				.usage = storage_dst
			}
		);
		f.physics_snapshot_buffer.host_zero();
		f.physics_snapshot_buffer.clear_host_dirty();

		f.indirect_dispatch_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = (2 + limits.max_colors) * 3 * sizeof(std::uint32_t),
				.usage = gpu::buffer_flag::storage | gpu::buffer_flag::indirect
			},
			"vbd.indirect_dispatch"
		);
		f.indirect_dispatch_buffer.buffer().host_zero();
		f.indirect_dispatch_buffer.buffer().clear_host_dirty();

		f.frozen_jacobian_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = limits.max_contacts * sizeof(frozen_jacobian),
				.usage = gpu::buffer_flag::storage
			},
			"vbd.frozen_jacobian"
		);

		f.solve_deltas_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = limits.max_bodies * 2 * sizeof(float) * 4,
				.usage = gpu::buffer_flag::storage
			},
			"vbd.solve_deltas"
		);

		f.grounded_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = limits.max_grounded_uints * sizeof(std::uint32_t),
				.usage = storage_src
			},
			"vbd.grounded"
		);
		f.grounded_buffer.buffer().host_zero();
		f.grounded_buffer.buffer().clear_host_dirty();

		f.grounded_readback_buffer = ctx.device->allocator().create_buffer(
			{
				.size = limits.max_grounded_uints * sizeof(std::uint32_t),
				.usage = storage_dst
			}
		);
		f.grounded_readback_buffer.host_zero();
		f.grounded_readback_buffer.clear_host_dirty();

		f.impulse_buffer = gpu::bindless_buffer::create(
			ctx.device->allocator(),
			*ctx.bindless_heaps,
			{
				.size = limits.max_impulses * sizeof(impulse_constraint),
				.usage = gpu::buffer_flag::storage
			},
			"vbd.impulse"
		);
	}

	m_upload_motors.reserve(limits.max_motors);
	m_upload_joints.reserve(limits.max_joints);
	m_upload_impulses.reserve(limits.max_impulses);
	m_upload_motor_map.reserve(limits.max_bodies);
	m_upload_collision_state.reserve(limits.collision_state_header_uints);

	m_buffers_created = true;
}

auto gse::vbd::gpu_solver::upload(const std::span<const body_state> bodies, const std::span<const velocity_motor_constraint> motors, const std::span<const joint_constraint> joints, const std::span<const impulse_constraint> impulses, const solver_config& solver_cfg, const time_step dt, const int steps, const bool refresh_joints) -> void {
	assert(bodies.size() <= limits.max_bodies, "body count {} exceeds max_bodies {}", bodies.size(), limits.max_bodies);
	assert(motors.size() <= limits.max_motors, "motor count {} exceeds max_motors {}", motors.size(),
		   limits.max_motors);
	assert(
		impulses.size() <= limits.max_impulses,
		"impulse count {} exceeds max_impulses {}",
		impulses.size(),
		limits.max_impulses
	);
	assert(joints.size() <= limits.max_joints, "joint count {} exceeds max_joints {}", joints.size(),
		   limits.max_joints);

	m_body_count = static_cast<std::uint32_t>(bodies.size());
	m_motor_count = static_cast<std::uint32_t>(motors.size());
	m_impulse_count = static_cast<std::uint32_t>(impulses.size());
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
		frame_data.body_buffer.buffer().host_write(bodies.first(m_body_count));
	}
	else {
		for (std::uint32_t i = 0; i < m_body_count; ++i) {
			if (bodies[i].locked) {
				frame_data.body_buffer.buffer().host_write(bodies[i], i * sizeof(body_state));
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

	m_upload_motor_map.assign(limits.max_bodies, 0xFFFFFFFFu);
	for (std::uint32_t mi = 0; mi < m_motor_count; ++mi) {
		assert(
			motors[mi].body_index < m_body_count,
			"Motor body index {} out of bounds (max {})",
			motors[mi].body_index,
			m_body_count
		);
		if (motors[mi].body_index < limits.max_bodies) {
			m_upload_motor_map[motors[mi].body_index] = mi;
		}
	}

	m_upload_collision_state.assign(limits.collision_state_header_uints, 0);

	m_warm_start_count = 0;
	m_upload_joints.clear();
	m_upload_joints_dirty = false;

	if (upload_joint_buffer) {
		m_joint_count = 0;
		m_upload_joints.assign(
			joints.size(),
			joint_constraint{}
		);

		const time_step sub_dt = dt / static_cast<float>(std::max(m_steps, 1u));
		const time_squared h_squared = sub_dt * sub_dt;

		for (std::size_t i = 0; i < joints.size(); ++i) {
			const auto& j = joints[i];
			if (j.body_a >= m_body_count || j.body_b >= m_body_count) {
				continue;
			}
			auto& g = m_upload_joints[m_joint_count];
			g = j;
			warm_start_joint(g, bodies[j.body_a], bodies[j.body_b], h_squared, solver_cfg);
			compute_joint_c0(g, bodies[j.body_a], bodies[j.body_b]);
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
	f.motor_buffer.buffer().host_write(m_upload_motors);
	f.motor_map_buffer.buffer().host_write(m_upload_motor_map);
	f.collision_state_buffer.buffer().host_write(m_upload_collision_state);

	if (m_upload_joints_dirty && !m_upload_joints.empty()) {
		f.joint_buffer.buffer().host_write(m_upload_joints);
	}

	if (!m_upload_impulses.empty()) {
		f.impulse_buffer.buffer().host_write(m_upload_impulses);
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
	while (!gpu_s.device || !gpu_s.bindless_heaps) {
		co_await ctx.next_tick();
	}

	const auto build = [&](const auto& pod) {
		return gpu::build_compute_program(*gpu_s.device, *gpu_s.bindless_heaps, pod);
	};

	m_compute.predict_pipeline = build(predict_entry::pod);
	m_compute.solve_color_pipeline = build(solve_color_entry::pod);
	m_compute.update_lambda_pipeline = build(update_lambda_entry::pod);
	m_compute.derive_velocities_pipeline = build(derive_velocities_entry::pod);
	m_compute.finalize_pipeline = build(finalize_entry::pod);
	m_compute.collision_reset_pipeline = build(collision_reset_entry::pod);
	m_compute.collision_broad_phase_pipeline = build(collision_broad_phase_entry::pod);
	m_compute.collision_narrow_phase_pipeline = build(collision_narrow_phase_entry::pod);
	m_compute.collision_grid_build_pipeline = build(collision_grid_build_entry::pod);
	m_compute.collision_build_adjacency_pipeline = build(collision_build_adjacency_entry::pod);
	m_compute.collision_build_coloring_pipeline = build(collision_build_coloring_entry::pod);
	m_compute.update_joint_lambda_pipeline = build(update_joint_lambda_entry::pod);
	m_compute.prepare_indirect_pipeline = build(prepare_indirect_entry::pod);
	m_compute.prepare_contact_indirect_pipeline = build(prepare_contact_indirect_entry::pod);
	m_compute.prepare_color_indirect_pipeline = build(prepare_color_indirect_entry::pod);
	m_compute.freeze_jacobians_pipeline = build(freeze_jacobians_entry::pod);
	m_compute.apply_jacobi_pipeline = build(apply_jacobi_entry::pod);
	m_compute.apply_restitution_pipeline = build(apply_restitution_entry::pod);
	m_compute.apply_impulses_pipeline = build(apply_impulses_entry::pod);

	create_buffers(gpu_s);

	m_compute.initialized = true;
	co_return;
}

auto gse::vbd::gpu_solver::dispatch_compute(frame_context& ctx) -> async::task<> {
	if (!m_buffers_created || m_body_count == 0) {
		co_return;
	}

	auto& f = m_frames[m_dispatch_slot];
	auto& other = m_frames[1 - m_dispatch_slot];

	const vbd_bindings bindings{
		.body_data = f.body_buffer.slot(),
		.contact_data = f.contact_buffer.slot(),
		.motor_data = f.motor_buffer.slot(),
		.color_data = f.color_buffer.slot(),
		.contact_offsets = f.contact_offsets_buffer.slot(),
		.solve_state = f.solve_state_buffer.slot(),
		.collision_pairs = f.collision_pair_buffer.slot(),
		.collision_state = f.collision_state_buffer.slot(),
		.warm_starts = f.warm_start_buffer.slot(),
		.joint_data = f.joint_buffer.slot(),
		.contact_counts = f.contact_counts_buffer.slot(),
		.contact_adjacency = f.contact_adjacency_buffer.slot(),
		.motor_map = f.motor_map_buffer.slot(),
		.joint_offsets = f.joint_offsets_buffer.slot(),
		.joint_counts = f.joint_counts_buffer.slot(),
		.joint_adjacency = f.joint_adjacency_buffer.slot(),
		.grid_data = f.grid_buffer.slot(),
		.indirect_args = f.indirect_dispatch_buffer.slot(),
		.frozen_jacobians = f.frozen_jacobian_buffer.slot(),
		.solve_deltas = f.solve_deltas_buffer.slot(),
		.grounded_bits = f.grounded_buffer.slot(),
		.impulse_data = f.impulse_buffer.slot(),
	};

	const std::uint32_t total = total_substeps();
	const time_step sub_dt = m_dt / static_cast<float>(total);
	const auto h_squared = sub_dt * sub_dt;

	auto ceil_div = [](const std::uint32_t a, const std::uint32_t b) {
		return (a + b - 1) / b;
	};

	auto make_pc = [this, sub_dt, h_squared](
		const std::uint32_t color_offset,
		const std::uint32_t color_count,
		const std::uint32_t substep,
		const std::uint32_t iteration,
		const float current_alpha,
		const std::uint32_t warm_start_count
	) {
		return vbd_push_constants{
			.cfg = m_solver_cfg,
			.body_count = m_body_count,
			.contact_count = limits.max_contacts,
			.motor_count = m_motor_count,
			.joint_count = m_joint_count,
			.impulse_count = m_impulse_count,
			.color_offset = color_offset,
			.color_count = color_count,
			.warm_start_count = warm_start_count,
			.substep = substep,
			.iteration = iteration,
			.h_squared = h_squared,
			.dt = sub_dt,
			.current_alpha = current_alpha,
			.grid_cell_size = m_grid_cell_size,
		};
	};

	const std::uint32_t body_workgroups = ceil_div(m_body_count, limits.workgroup_size);
	const std::uint32_t reset_workgroups = ceil_div(
		std::max({ m_body_count, limits.max_contacts, limits.grid_table_size }),
		limits.workgroup_size
	);
	const std::uint32_t joint_workgroups = ceil_div(std::max(m_joint_count, 1u), limits.workgroup_size);
	constexpr std::uint32_t num_colors = limits.max_colors;
	const std::uint32_t num_iterations = m_solver_cfg.iterations;
	const float solve_alpha = m_solver_cfg.post_stabilize ? 1.f : m_solver_cfg.alpha;
	const bool use_jacobi = m_solver_cfg.use_jacobi;
	const bool post_stabilize = m_solver_cfg.post_stabilize;
	const std::uint32_t joint_count = m_joint_count;

	constexpr std::size_t frozen_jacobian_clear_size = limits.max_contacts * sizeof(frozen_jacobian);
	constexpr std::size_t solve_state_clear_size = limits.max_bodies * limits.solve_state_float4s_per_body * sizeof(float) * 4;
	constexpr std::size_t solve_deltas_clear_size = limits.max_bodies * 2 * sizeof(float) * 4;

	auto rec = co_await gpu::pass<vbd_clear_state_buffers_stage>(ctx).on(gpu::queue_type::compute).in_chain<vbd_solve_chain>();
	rec.fill_buffer(f.frozen_jacobian_buffer.buffer(), 0, frozen_jacobian_clear_size);
	rec.fill_buffer(f.solve_state_buffer.buffer(), 0, solve_state_clear_size);
	rec.fill_buffer(f.solve_deltas_buffer.buffer(), 0, solve_deltas_clear_size);

	for (std::uint32_t sub = 0; sub < total; ++sub) {
		const std::uint32_t warm = (sub == 0) ? m_warm_start_count : 0u;

		rec = co_await gpu::pass<vbd_collision_reset_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.collision_reset_pipeline);

		rec.dispatch<collision_reset_entry>(
			make_pc(0u, 0u, sub, 0u, 0.f, warm),
			bindings,
			vec3u{ reset_workgroups, 1u, 1u }
		);

		rec = co_await gpu::pass<vbd_grid_build_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.collision_grid_build_pipeline);

		rec.dispatch<collision_grid_build_entry>(
			make_pc(0u, 0u, sub, 0u, 0.f, warm),
			bindings,
			vec3u{ body_workgroups, 1u, 1u }
		);

		rec = co_await gpu::pass<vbd_broad_phase_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.collision_broad_phase_pipeline);

		rec.dispatch<collision_broad_phase_entry>(
			make_pc(0u, 0u, sub, 0u, 0.f, warm),
			bindings,
			vec3u{ body_workgroups, 1u, 1u }
		);

		rec = co_await gpu::pass<vbd_prepare_indirect_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.prepare_indirect_pipeline);

		rec.dispatch<prepare_indirect_entry>(
			make_pc(0u, 0u, sub, 0u, 0.f, warm),
			bindings,
			vec3u{ 1u, 1u, 1u }
		);

		rec = co_await gpu::pass<vbd_narrow_phase_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.collision_narrow_phase_pipeline);

		rec.push_bindings<collision_narrow_phase_entry>(make_pc(0u, 0u, sub, 0u, 0.f, warm), bindings);
		rec.dispatch_indirect(f.indirect_dispatch_buffer.buffer(), 0);

		rec = co_await gpu::pass<vbd_prepare_contact_indirect_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.prepare_contact_indirect_pipeline);

		rec.dispatch<prepare_contact_indirect_entry>(
			make_pc(0u, 0u, sub, 0u, 0.f, warm),
			bindings,
			vec3u{ 1u, 1u, 1u }
		);

		rec = co_await gpu::pass<vbd_build_adjacency_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.collision_build_adjacency_pipeline);

		rec.dispatch<collision_build_adjacency_entry>(
			make_pc(0u, 0u, sub, 0u, 0.f, warm),
			bindings,
			vec3u{ 1u, 1u, 1u }
		);

		if (sub == 0) {
			rec = co_await gpu::pass<vbd_build_coloring_stage>(ctx)
				.on(gpu::queue_type::compute)
				.in_chain<vbd_solve_chain>()
				.pipeline(m_compute.collision_build_coloring_pipeline);

			rec.dispatch<collision_build_coloring_entry>(
				make_pc(0u, 0u, sub, 0u, 0.f, warm),
				bindings,
				vec3u{ 1u, 1u, 1u }
			);
		}
		else {
			rec = co_await gpu::pass<vbd_build_coloring_stage>(ctx)
				.on(gpu::queue_type::compute)
				.in_chain<vbd_solve_chain>()
				.pipeline(m_compute.prepare_color_indirect_pipeline);

			rec.dispatch<prepare_color_indirect_entry>(
				make_pc(0u, 0u, sub, 0u, 0.f, warm),
				bindings,
				vec3u{ 1u, 1u, 1u }
			);
		}

		if (sub == 0 && m_impulse_count > 0) {
			const std::uint32_t impulse_workgroups = ceil_div(m_impulse_count, limits.workgroup_size);

			rec = co_await gpu::pass<vbd_apply_impulses_stage>(ctx)
				.on(gpu::queue_type::compute)
				.in_chain<vbd_solve_chain>()
				.pipeline(m_compute.apply_impulses_pipeline);

			rec.dispatch<apply_impulses_entry>(
				make_pc(0u, 0u, sub, 0u, 0.f, warm),
				bindings,
				vec3u{ impulse_workgroups, 1u, 1u }
			);
		}

		rec = co_await gpu::pass<vbd_predict_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.predict_pipeline);

		rec.dispatch<predict_entry>(
			make_pc(0u, 0u, sub, 0u, 0.f, warm),
			bindings,
			vec3u{ body_workgroups, 1u, 1u }
		);

		rec = co_await gpu::pass<vbd_freeze_jacobians_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.freeze_jacobians_pipeline);

		rec.push_bindings<freeze_jacobians_entry>(make_pc(0u, 0u, sub, 0u, 0.f, warm), bindings);
		rec.dispatch_indirect(f.indirect_dispatch_buffer.buffer(), 3 * sizeof(std::uint32_t));

		rec = co_await gpu::pass<vbd_solve_iterations_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.solve_color_pipeline);

		for (std::uint32_t it = 0; it < num_iterations; ++it) {
			rec.bind(m_compute.solve_color_pipeline);
			auto color_pc = make_pc(0u, num_colors, sub, it, solve_alpha, warm);

			if (use_jacobi) {
				rec.push_bindings<solve_color_entry>(color_pc, bindings);
				rec.dispatch(body_workgroups, 1, 1);
				rec.barrier(gpu::barrier_scope::compute_to_compute);

				rec.bind(m_compute.apply_jacobi_pipeline);
				rec.push_bindings<apply_jacobi_entry>(make_pc(0u, 0u, sub, it, solve_alpha, warm), bindings);
				rec.dispatch(body_workgroups, 1, 1);
				rec.barrier(gpu::barrier_scope::compute_to_compute);
			}
			else {
				for (std::uint32_t color = 0; color < num_colors; ++color) {
					color_pc.color_offset = color;
					rec.push_bindings<solve_color_entry>(color_pc, bindings);
					rec.dispatch_indirect(f.indirect_dispatch_buffer.buffer(), (2 + color) * 3 * sizeof(std::uint32_t));
					if (color + 1 < num_colors) {
						rec.barrier(gpu::barrier_scope::compute_to_compute);
					}
				}
				rec.barrier(gpu::barrier_scope::compute_to_compute);
			}

			rec.bind(m_compute.update_lambda_pipeline);
			rec.push_bindings<update_lambda_entry>(make_pc(0u, 0u, sub, it, solve_alpha, warm), bindings);
			rec.dispatch_indirect(f.indirect_dispatch_buffer.buffer(), 3 * sizeof(std::uint32_t));
			if (joint_count > 0) {
				rec.bind(m_compute.update_joint_lambda_pipeline);
				rec.push_bindings<update_joint_lambda_entry>(make_pc(0u, 0u, sub, it, solve_alpha, warm), bindings);
				rec.dispatch(joint_workgroups, 1, 1);
			}
			rec.barrier(gpu::barrier_scope::compute_to_indirect);
		}

		rec = co_await gpu::pass<vbd_derive_velocities_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.derive_velocities_pipeline);

		rec.dispatch<derive_velocities_entry>(
			make_pc(0u, 0u, sub, num_iterations, solve_alpha, warm),
			bindings,
			vec3u{ body_workgroups, 1u, 1u }
		);

		rec = co_await gpu::pass<vbd_apply_restitution_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.apply_restitution_pipeline);

		auto restitution_pc = make_pc(0u, num_colors, sub, num_iterations, 0.f, warm);

		for (std::uint32_t color = 0; color < num_colors; ++color) {
			restitution_pc.color_offset = color;
			rec.push_bindings<apply_restitution_entry>(restitution_pc, bindings);
			rec.dispatch_indirect(f.indirect_dispatch_buffer.buffer(), (2 + color) * 3 * sizeof(std::uint32_t));

			if (color + 1 < num_colors) {
				rec.barrier(gpu::barrier_scope::compute_to_compute);
			}
		}

		if (post_stabilize) {
			rec = co_await gpu::pass<vbd_post_stabilize_stage>(ctx)
				.on(gpu::queue_type::compute)
				.in_chain<vbd_solve_chain>()
				.pipeline(m_compute.prepare_color_indirect_pipeline);

			rec.dispatch<prepare_color_indirect_entry>(
				make_pc(0u, 0u, sub, num_iterations, 0.f, warm),
				bindings,
				vec3u{ 1u, 1u, 1u }
			);
			rec.barrier(gpu::barrier_scope::compute_to_indirect);

			auto color_pc = make_pc(0u, num_colors, sub, num_iterations, 0.f, warm);

			rec.bind(m_compute.solve_color_pipeline);

			if (use_jacobi) {
				rec.push_bindings<solve_color_entry>(color_pc, bindings);
				rec.dispatch(body_workgroups, 1, 1);
				rec.barrier(gpu::barrier_scope::compute_to_compute);

				rec.bind(m_compute.apply_jacobi_pipeline);
				rec.push_bindings<apply_jacobi_entry>(make_pc(0u, 0u, sub, num_iterations, 0.f, warm), bindings);
				rec.dispatch(body_workgroups, 1, 1);
			}
			else {
				for (std::uint32_t color = 0; color < num_colors; ++color) {
					color_pc.color_offset = color;
					rec.push_bindings<solve_color_entry>(color_pc, bindings);
					rec.dispatch_indirect(f.indirect_dispatch_buffer.buffer(), (2 + color) * 3 * sizeof(std::uint32_t));
					if (color + 1 < num_colors) {
						rec.barrier(gpu::barrier_scope::compute_to_compute);
					}
				}
			}
		}

		rec = co_await gpu::pass<vbd_finalize_stage>(ctx)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>()
			.pipeline(m_compute.finalize_pipeline);

		rec.dispatch<finalize_entry>(
			make_pc(0u, 0u, sub, 0u, 0.f, warm),
			bindings,
			vec3u{ body_workgroups, 1u, 1u }
		);
	}

	const std::size_t body_copy_size = m_body_count * sizeof(body_state);
	const std::size_t joint_copy_size = joint_count * sizeof(joint_constraint);
	constexpr std::size_t grounded_copy_size = limits.max_grounded_uints * sizeof(std::uint32_t);

	rec = co_await gpu::pass<vbd_state_copy_stage>(ctx).on(gpu::queue_type::compute).in_chain<vbd_solve_chain>();
	if (joint_count > 0) {
		rec.copy_buffer(f.joint_buffer.buffer(), other.joint_buffer.buffer(), joint_copy_size);
	}

	rec.copy_buffer(f.body_buffer.buffer(), other.body_buffer.buffer(), body_copy_size);
	rec.copy_buffer(f.body_buffer.buffer(), f.physics_snapshot_buffer, body_copy_size);
	rec.copy_buffer(f.grounded_buffer.buffer(), f.grounded_readback_buffer, grounded_copy_size);

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
