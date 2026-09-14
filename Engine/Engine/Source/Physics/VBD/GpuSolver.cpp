module gse.physics:vbd_gpu_solver_impl;

import gse.assert;
import gse.assets;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;
import gse.log;
import gse.os;
import gse.time;
import std;

import :vbd_constraints;
import :vbd_gpu_solver;
import :vbd_solver;

namespace gse::vbd {
	constexpr std::size_t host_write_chunk_bytes = 1u << 20;

	template <typename T>
	auto parallel_host_write(const gpu::buffer& target, const std::span<const T> src, const id label) -> void {
		const auto total = src.size_bytes();
		const auto chunks = (total + host_write_chunk_bytes - 1) / host_write_chunk_bytes;
		const auto* bytes = reinterpret_cast<const std::byte*>(src.data());
		task::coarse_parallel(
			chunks,
			1,
			[&target, bytes, total](const std::size_t c) {
				const auto offset = c * host_write_chunk_bytes;
				target.host_write(bytes + offset, std::min(host_write_chunk_bytes, total - offset), offset);
			},
			label
		);
	}

	constexpr std::size_t debug_contact_dump_count = 4096;
	constexpr std::size_t debug_adjacency_meta_count = 256;
	constexpr std::size_t debug_adjacency_dump_count = 1024;
	struct [[= shaders::shader_struct]] vbd_push_constants {
		std::uint32_t body_count;
		std::uint32_t contact_count;
		std::uint32_t motor_count;
		std::uint32_t joint_count;
		std::uint32_t impulse_count;
		std::uint32_t color_offset;
		std::uint32_t color_count;
		std::uint32_t substep;
		std::uint32_t iteration;
		time_squared h_squared;
		time_step dt;
		float current_alpha;
		gap grid_cell_size;
		std::uint32_t apply_all_body_inputs;
		std::uint32_t preserve_accel_weight;
		std::uint32_t sweep_workgroups;
		std::uint32_t substeps_per_tick;
		std::uint32_t impulse_offset;
		std::uint32_t fold_lambda;
		std::uint32_t lambda_pass;
	};

	struct [[= shaders::ssbo_readwrite]] body_data {
		using element = body_state;
	};
	struct [[= shaders::ssbo_readwrite]] contact_data {
		using element = contact_constraint;
	};
	struct [[= shaders::ssbo_readonly]] motor_data {
		using element = velocity_motor_constraint;
	};
	struct [[= shaders::ssbo_readwrite]] color_data {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readwrite]] contact_offsets {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readwrite]] solve_state {
		using element = vec4f;
	};
	struct [[= shaders::ssbo_readwrite]] collision_pairs {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readwrite]] collision_state {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readwrite]] warm_starts {
		using element = contact_constraint;
	};
	struct [[= shaders::ssbo_readwrite]] joint_data {
		using element = joint_constraint;
	};
	struct [[= shaders::ssbo_readwrite]] contact_counts {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readwrite]] contact_adjacency {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readwrite]] motor_map {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readwrite]] joint_offsets {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readwrite]] joint_counts {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readwrite]] joint_adjacency {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readwrite]] grid_data {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readwrite]] indirect_args {
		using element = dispatch_args;
	};
	struct [[= shaders::ssbo_readwrite]] frozen_jacobians {
		using element = frozen_jacobian;
	};
	struct [[= shaders::ssbo_readwrite]] solve_deltas {
		using element = vec4f;
	};
	struct [[= shaders::ssbo_readwrite]] grounded_bits {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readwrite]] unowned_contacts {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readonly]] impulse_data {
		using element = impulse_constraint;
	};
	struct [[= shaders::ssbo_readonly]] solver_config_data {
		using element = solver_config;
	};
	struct [[= shaders::ssbo_readonly]] jointed_pairs_data {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readonly]] body_input_data {
		using element = body_state;
	};
	struct [[= shaders::ssbo_readwrite]] jointless_color_data {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readwrite]] jointless_indirect_args {
		using element = dispatch_args;
	};
	struct [[= shaders::ssbo_readonly]] island_data {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readonly]] body_env_data {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readonly]] static_bodies_data {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readwrite]] coloring_scratch {
		using element = std::uint32_t;
	};
	struct [[= shaders::ssbo_readonly]] joint_input_data {
		using element = joint_constraint;
	};
	struct [[= shaders::ssbo_readonly]] joint_drive_input_data {
		using element = joint_drive_input;
	};
	struct [[= shaders::ssbo_readonly]] body_input_index_data {
		using element = std::uint32_t;
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
		impulse_data,
		solver_config_data,
		jointed_pairs_data,
		body_input_data,
		jointless_color_data,
		jointless_indirect_args,
		island_data,
		body_env_data,
		static_bodies_data,
		coloring_scratch,
		joint_input_data,
		joint_drive_input_data,
		unowned_contacts,
		body_input_index_data
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
	using solve_color_entry = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_solve_color">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared", "Bodies/VBDPhysics/vbd_update_lambda">,
		gpu::threads<limits.workgroup_size>,
		gpu::push_constant<vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using solve_island_entry = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_solve_island">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared", "Bodies/VBDPhysics/vbd_update_lambda", "Bodies/VBDPhysics/vbd_solve_color">,
		gpu::threads<limits.workgroup_size>,
		gpu::push_constant<vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using solve_sweep_entry = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_solve_sweep">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared", "Bodies/VBDPhysics/vbd_update_lambda", "Bodies/VBDPhysics/vbd_solve_color">,
		gpu::threads<limits.workgroup_size>,
		gpu::push_constant<vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

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
	using update_sticking_entry = vbd_compute<"VBDPhysics/vbd_update_sticking">;
	using apply_impulses_entry = vbd_compute<"VBDPhysics/vbd_apply_impulses">;
	using apply_body_inputs_entry = vbd_compute<"VBDPhysics/vbd_apply_body_inputs">;
	using apply_joint_inputs_entry = vbd_compute<"VBDPhysics/vbd_apply_joint_inputs">;
	using apply_joint_drive_inputs_entry = vbd_compute<"VBDPhysics/vbd_apply_joint_drive_inputs">;
	using collision_sort_adjacency_entry = vbd_compute<"VBDPhysics/collision_sort_adjacency">;
	using collision_color_round_entry = vbd_compute<"VBDPhysics/collision_color_round">;
	using collision_color_commit_entry = vbd_compute<"VBDPhysics/collision_color_commit">;

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

	using hash_state_entry = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_hash_state">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<limits.adjacency_workgroup_size>,
		gpu::push_constant<vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id, gpu::group_thread_id>
	>;

	using hash_warm_inputs_entry = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_hash_warm_inputs">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<limits.adjacency_workgroup_size>,
		gpu::push_constant<vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id, gpu::group_thread_id>
	>;

	using hash_adjacency_entry = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_hash_adjacency">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<limits.adjacency_workgroup_size>,
		gpu::push_constant<vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id, gpu::group_thread_id>
	>;

	using hash_colors_entry = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_hash_colors">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<limits.adjacency_workgroup_size>,
		gpu::push_constant<vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id, gpu::group_thread_id>
	>;

	using hash_bodies_entry = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_hash_bodies">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<limits.adjacency_workgroup_size>,
		gpu::push_constant<vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id, gpu::group_thread_id>
	>;

	using apply_restitution_serial_entry = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_apply_restitution">,
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

	using convergence_check_entry = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_convergence_check">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<1>,
		gpu::push_constant<vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
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

auto gse::vbd::gpu_solver::create_buffers(const shared_view<gpu::context::data> ctx) -> void {
	m_frame = ctx.frame;
	constexpr gpu::buffer_usage storage_src{ gpu::buffer_flag::storage, gpu::buffer_flag::transfer_src };
	constexpr gpu::buffer_usage storage_dst{ gpu::buffer_flag::storage, gpu::buffer_flag::transfer_dst };
	constexpr auto storage_src_dst = storage_src | gpu::buffer_flag::transfer_dst;
	const std::size_t color_buffer_size =
		limits.max_colors * sizeof(std::uint32_t) * 2 + m_capacities.max_bodies * sizeof(std::uint32_t);
	const std::size_t collision_pair_size =
		sizeof(std::uint32_t) + m_capacities.max_collision_pairs * 2 * sizeof(std::uint32_t);
	constexpr std::size_t collision_state_size =
		(limits.collision_state_header_uints +
		 limits.max_narrow_phase_debug_records * limits.narrow_phase_debug_record_uints) *
		sizeof(std::uint32_t);
	const std::size_t joint_buffer_size = m_capacities.max_joints * sizeof(joint_constraint);

	for (auto& f : m_frames) {
		f.body_buffer = ctx.device->create_buffer(
			{
				.size = m_capacities.max_bodies * sizeof(body_state),
				.stride = sizeof(body_state),
				.usage = storage_src_dst,
				.bindless = true,
				.writable = true,
				.device_local = true
			},
			"vbd.body"
		);
		f.body_alt_view = ctx.device->allocate_buffer_slot();
		ctx.device->write_storage_buffer(f.body_alt_view.slot(), f.body_buffer, m_capacities.max_bodies * sizeof(body_state));

		f.contact_buffer = ctx.device->create_buffer(
			{
				.size = m_capacities.max_contacts * sizeof(contact_constraint),
				.stride = sizeof(contact_constraint),
				.usage = storage_src_dst,
				.bindless = true,
				.writable = true,
				.device_local = true
			},
			"vbd.contact"
		);

		f.color_buffer = ctx.device->create_buffer(
			{
				.size = color_buffer_size,
				.stride = sizeof(std::uint32_t),
				.usage = storage_dst,
				.bindless = true,
				.writable = true
			},
			"vbd.color"
		);
		f.jointless_color_buffer = ctx.device->create_buffer(
			{
				.size = color_buffer_size,
				.stride = sizeof(std::uint32_t),
				.usage = storage_dst,
				.bindless = true,
				.writable = true
			},
			"vbd.jointless_color"
		);

		f.contact_offsets_buffer = ctx.device->create_buffer(
			{
				.size = m_capacities.max_bodies * sizeof(std::uint32_t),
				.stride = sizeof(std::uint32_t),
				.usage = storage_src_dst,
				.bindless = true,
				.writable = true,
				.device_local = true
			},
			"vbd.contact_offsets"
		);
		f.contact_counts_buffer = ctx.device->create_buffer(
			{
				.size = m_capacities.max_bodies * sizeof(std::uint32_t),
				.stride = sizeof(std::uint32_t),
				.usage = storage_src_dst,
				.bindless = true,
				.writable = true,
				.device_local = true
			},
			"vbd.contact_counts"
		);
		f.contact_adjacency_buffer = ctx.device->create_buffer(
			{
				.size = m_capacities.max_contacts * 2 * sizeof(std::uint32_t),
				.stride = sizeof(std::uint32_t),
				.usage = storage_src_dst,
				.bindless = true,
				.writable = true,
				.device_local = true
			},
			"vbd.contact_adjacency"
		);
		f.joint_offsets_buffer = ctx.device->create_buffer(
			{
				.size = m_capacities.max_bodies * sizeof(std::uint32_t),
				.stride = sizeof(std::uint32_t),
				.usage = gpu::buffer_flag::storage,
				.bindless = true,
				.writable = true
			},
			"vbd.joint_offsets"
		);
		f.joint_counts_buffer = ctx.device->create_buffer(
			{
				.size = m_capacities.max_bodies * sizeof(std::uint32_t),
				.stride = sizeof(std::uint32_t),
				.usage = gpu::buffer_flag::storage,
				.bindless = true,
				.writable = true
			},
			"vbd.joint_counts"
		);
		f.joint_adjacency_buffer = ctx.device->create_buffer(
			{
				.size = m_capacities.max_joints * 2 * sizeof(std::uint32_t),
				.stride = sizeof(std::uint32_t),
				.usage = gpu::buffer_flag::storage,
				.bindless = true,
				.writable = true
			},
			"vbd.joint_adjacency"
		);

		f.solve_state_buffer = ctx.device->create_buffer(
			{
				.size = m_capacities.max_bodies * limits.solve_state_float4s_per_body * sizeof(float) * 4,
				.stride = sizeof(vec4f),
				.usage = storage_dst,
				.bindless = true,
				.writable = true
			},
			"vbd.solve_state"
		);

		f.collision_pair_buffer = ctx.device->create_buffer(
			{
				.size = collision_pair_size,
				.stride = sizeof(std::uint32_t),
				.usage = storage_dst,
				.bindless = true,
				.writable = true
			},
			"vbd.collision_pair"
		);

		f.collision_state_buffer = ctx.device->create_buffer(
			{
				.size = collision_state_size,
				.stride = sizeof(std::uint32_t),
				.usage = storage_src_dst,
				.bindless = true,
				.writable = true
			},
			"vbd.collision_state"
		);
		f.collision_state_buffer.host_zero();
		f.collision_state_buffer.clear_host_dirty();

		f.warm_start_buffer = ctx.device->create_buffer(
			{
				.size = m_capacities.max_contacts * sizeof(contact_constraint),
				.stride = sizeof(contact_constraint),
				.usage = storage_dst,
				.bindless = true,
				.writable = true,
				.device_local = true
			},
			"vbd.warm_start"
		);

		f.joint_buffer = ctx.device->create_buffer(
			{
				.size = std::max<std::size_t>(joint_buffer_size, 16),
				.stride = sizeof(joint_constraint),
				.usage = storage_src_dst,
				.bindless = true,
				.writable = true
			},
			"vbd.joint"
		);

		const std::size_t grid_buffer_size = (1 + m_capacities.grid_table_size + m_capacities.max_bodies * 8 * 2) * sizeof(std::uint32_t);
		f.grid_buffer = ctx.device->create_buffer(
			{
				.size = grid_buffer_size,
				.stride = sizeof(std::uint32_t),
				.usage = storage_dst,
				.bindless = true,
				.writable = true
			},
			"vbd.grid"
		);
		f.grid_buffer.host_zero();
		f.grid_buffer.clear_host_dirty();

		f.indirect_dispatch_buffer = ctx.device->create_buffer(
			{
				.size = (3 + limits.max_colors) * 3 * sizeof(std::uint32_t),
				.stride = sizeof(dispatch_args),
				.usage = { gpu::buffer_flag::storage, gpu::buffer_flag::indirect, gpu::buffer_flag::transfer_dst },
				.bindless = true,
				.writable = true
			},
			"vbd.indirect_dispatch"
		);
		f.indirect_dispatch_buffer.host_zero();
		f.indirect_dispatch_buffer.clear_host_dirty();

		f.jointless_indirect_dispatch_buffer = ctx.device->create_buffer(
			{
				.size = (2 + limits.max_colors) * 3 * sizeof(std::uint32_t),
				.stride = sizeof(dispatch_args),
				.usage = { gpu::buffer_flag::storage, gpu::buffer_flag::indirect, gpu::buffer_flag::transfer_dst },
				.bindless = true,
				.writable = true
			},
			"vbd.jointless_indirect_dispatch"
		);
		f.jointless_indirect_dispatch_buffer.host_zero();
		f.jointless_indirect_dispatch_buffer.clear_host_dirty();

		f.frozen_jacobian_buffer = ctx.device->create_buffer(
			{
				.size = m_capacities.max_contacts * sizeof(frozen_jacobian),
				.stride = sizeof(frozen_jacobian),
				.usage = storage_dst,
				.bindless = true,
				.writable = true
			},
			"vbd.frozen_jacobian"
		);

		f.solve_deltas_buffer = ctx.device->create_buffer(
			{
				.size = m_capacities.max_bodies * 2 * sizeof(float) * 4,
				.stride = sizeof(vec4f),
				.usage = storage_dst,
				.bindless = true,
				.writable = true
			},
			"vbd.solve_deltas"
		);

		f.unowned_contact_buffer = ctx.device->create_buffer(
			{
				.size = (m_capacities.max_contacts + 1) * sizeof(std::uint32_t),
				.stride = sizeof(std::uint32_t),
				.usage = storage_dst,
				.bindless = true,
				.writable = true
			},
			"vbd.unowned_contacts"
		);

		f.grounded_buffer = ctx.device->create_buffer(
			{
				.size = m_capacities.max_grounded_uints * sizeof(std::uint32_t),
				.stride = sizeof(std::uint32_t),
				.usage = storage_src_dst,
				.bindless = true,
				.writable = true
			},
			"vbd.grounded"
		);
		f.grounded_buffer.host_zero();
		f.grounded_buffer.clear_host_dirty();

		f.coloring_scratch_buffer = ctx.device->create_buffer(
			{
				.size = m_capacities.max_bodies * 2 * sizeof(std::uint32_t),
				.stride = sizeof(std::uint32_t),
				.usage = gpu::buffer_flag::storage,
				.bindless = true,
				.writable = true
			},
			"vbd.coloring_scratch"
		);

		f.render_body_buffer = ctx.device->create_buffer(
			{
				.size = m_capacities.max_bodies * sizeof(body_state),
				.stride = sizeof(body_state),
				.usage = storage_src_dst,
				.bindless = true,
				.writable = true,
				.device_local = true
			},
			"vbd.render_body"
		);
	}

	m_body_input_channel = ctx.render_graph->create_upload_channel(
		{
			.size = m_capacities.max_bodies * sizeof(body_state),
			.stride = sizeof(body_state),
			.usage = gpu::buffer_flag::storage,
			.bindless = true
		},
		"vbd.body_input"
	);

	m_motor_channel = ctx.render_graph->create_upload_channel(
		{
			.size = m_capacities.max_motors * sizeof(velocity_motor_constraint),
			.stride = sizeof(velocity_motor_constraint),
			.usage = gpu::buffer_flag::storage,
			.bindless = true
		},
		"vbd.motor"
	);

	m_motor_map_channel = ctx.render_graph->create_upload_channel(
		{
			.size = m_capacities.max_bodies * sizeof(std::uint32_t),
			.stride = sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage,
			.bindless = true,
			.writable = true
		},
		"vbd.motor_map"
	);

	m_solver_config_channel = ctx.render_graph->create_upload_channel(
		{
			.size = sizeof(solver_config),
			.stride = sizeof(solver_config),
			.usage = gpu::buffer_flag::storage,
			.bindless = true
		},
		"vbd.solver_config"
	);

	m_impulse_channel = ctx.render_graph->create_upload_channel(
		{
			.size = m_capacities.max_impulses * sizeof(impulse_constraint),
			.stride = sizeof(impulse_constraint),
			.usage = gpu::buffer_flag::storage,
			.bindless = true
		},
		"vbd.impulse"
	);

	m_jointed_pairs_channel = ctx.render_graph->create_upload_channel(
		{
			.size = (1 + m_capacities.max_joints * 2) * sizeof(std::uint32_t),
			.stride = sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage,
			.bindless = true
		},
		"vbd.jointed_pairs"
	);

	m_island_channel = ctx.render_graph->create_upload_channel(
		{
			.size = (1 + 2 * m_capacities.max_islands + m_capacities.max_bodies) * sizeof(std::uint32_t),
			.stride = sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage,
			.bindless = true
		},
		"vbd.island"
	);

	m_body_env_channel = ctx.render_graph->create_upload_channel(
		{
			.size = m_capacities.max_bodies * sizeof(std::uint32_t),
			.stride = sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage,
			.bindless = true
		},
		"vbd.body_env"
	);

	m_static_bodies_channel = ctx.render_graph->create_upload_channel(
		{
			.size = (1 + m_capacities.max_bodies) * sizeof(std::uint32_t),
			.stride = sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage,
			.bindless = true
		},
		"vbd.static_bodies"
	);

	m_body_input_index_channel = ctx.render_graph->create_upload_channel(
		{
			.size = (1 + m_capacities.max_bodies) * sizeof(std::uint32_t),
			.stride = sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage,
			.bindless = true
		},
		"vbd.body_input_indices"
	);

	m_joint_upload_channel = ctx.render_graph->create_upload_channel(
		{
			.size = std::max<std::size_t>(joint_buffer_size, 16),
			.stride = sizeof(joint_constraint),
			.usage = storage_src,
			.bindless = true
		},
		"vbd.joint_upload"
	);

	m_joint_drive_input_channel = ctx.render_graph->create_upload_channel(
		{
			.size = std::max<std::size_t>(m_capacities.max_joints * sizeof(joint_drive_input), 16),
			.stride = sizeof(joint_drive_input),
			.usage = storage_src,
			.bindless = true
		},
		"vbd.joint_drive_inputs"
	);

	const auto gate = m_sync_readback ? gpu::readback_gate::queue_fence : gpu::readback_gate::frames_in_flight;
	m_snapshot_channel = ctx.render_graph->create_readback_channel(m_capacities.max_bodies * sizeof(body_state), "vbd.snapshot", gate, gpu::queue_type::compute);
	m_grounded_channel = ctx.render_graph->create_readback_channel(m_capacities.max_grounded_uints * sizeof(std::uint32_t), "vbd.grounded_readback", gate, gpu::queue_type::compute);
	m_collision_state_channel = ctx.render_graph->create_readback_channel(collision_state_size, "vbd.collision_state_readback", gate, gpu::queue_type::compute);
	m_contact_dump_channel = ctx.render_graph->create_readback_channel(debug_contact_dump_count * sizeof(contact_constraint), "vbd.contact_dump", gate, gpu::queue_type::compute);
	m_adjacency_meta_channel = ctx.render_graph->create_readback_channel(debug_adjacency_meta_count * 2 * sizeof(std::uint32_t), "vbd.adjacency_meta", gate, gpu::queue_type::compute);
	m_contact_adjacency_channel = ctx.render_graph->create_readback_channel(debug_adjacency_dump_count * sizeof(std::uint32_t), "vbd.contact_adjacency_dump", gate, gpu::queue_type::compute);

	m_upload_motors.reserve(m_capacities.max_motors);
	m_upload_joints.reserve(m_capacities.max_joints);
	m_upload_impulses.reserve(m_capacities.max_impulses);
	m_upload_motor_map.reserve(m_capacities.max_bodies);

	m_buffers_created = true;
	m_compute.device_local_seeded = false;
}

auto gse::vbd::gpu_solver::upload(const solver_upload& payload) -> void {
	const std::span<const body_state> bodies = payload.bodies;
	const std::span<const velocity_motor_constraint> motors = payload.motors;
	const std::span<const joint_constraint> joints = payload.joints;
	const std::span<const impulse_constraint> impulses = payload.impulses;

	assert(bodies.size() <= m_capacities.max_bodies, "body count {} exceeds max_bodies {}", bodies.size(), m_capacities.max_bodies);
	assert(motors.size() <= m_capacities.max_motors, "motor count {} exceeds max_motors {}", motors.size(),
		m_capacities.max_motors);
	assert(
		impulses.size() <= m_capacities.max_impulses,
		"impulse count {} exceeds max_impulses {}",
		impulses.size(),
		m_capacities.max_impulses
	);
	assert(joints.size() <= m_capacities.max_joints, "joint count {} exceeds max_joints {}", joints.size(),
		m_capacities.max_joints);

	m_body_count = static_cast<std::uint32_t>(bodies.size());
	m_steps = static_cast<std::uint32_t>(std::max(payload.steps, 1));
	m_ticks = static_cast<std::uint32_t>(std::max(payload.ticks, 1));
	m_motor_count = payload.motors_per_tick > 0 ? payload.motors_per_tick : static_cast<std::uint32_t>(motors.size());
	m_impulse_count = static_cast<std::uint32_t>(impulses.size());
	m_impulse_counts.assign(m_ticks, 0u);
	m_impulse_offsets.assign(m_ticks, 0u);
	if (payload.impulse_counts.size() == m_ticks) {
		std::uint32_t offset = 0;
		for (std::uint32_t t = 0; t < m_ticks; ++t) {
			m_impulse_counts[t] = payload.impulse_counts[t];
			m_impulse_offsets[t] = offset;
			offset += payload.impulse_counts[t];
		}
	}
	else {
		m_impulse_counts[0] = m_impulse_count;
	}
	m_solver_cfg = payload.solver_cfg;
	m_dt = payload.dt;
	m_first_tick = payload.first_tick;
	m_restore_tick = payload.restore_tick;

	if (m_body_count == 0) {
		m_pending_dispatch = false;
		m_body_buffers_seeded = false;
		m_joint_buffers_seeded = false;
		m_seeded_body_count = 0;
		m_joint_count = 0;
		m_jointless_body_count = 0;
		m_impulse_count = 0;
		m_apply_all_body_inputs = false;
		return;
	}

	m_upload_impulses.assign(impulses.begin(), impulses.begin() + m_impulse_count);

	if (payload.force_reseed) {
		m_body_buffers_seeded = false;
		m_joint_buffers_seeded = false;
	}

	m_apply_all_body_inputs = !m_body_buffers_seeded || m_seeded_body_count != m_body_count;
	const bool resident_joints = m_joint_buffers_seeded && m_seeded_body_count == m_body_count;
	const bool upload_joint_buffer = payload.refresh_joints || !resident_joints;
	const auto previous_joint_count = m_joint_count;

	{
		trace::scope_guard _{ trace_id<"vbd_gpu::upload::body_scan">() };
		constexpr std::uint32_t scan_chunk = 2048;
		const auto chunk_count = (m_body_count + scan_chunk - 1) / scan_chunk;
		const bool sparse = !m_apply_all_body_inputs;
		m_body_scan.resize(chunk_count);
		const auto* body_data = bodies.data();
		auto* scan_data = m_body_scan.data();
		const auto body_count = m_body_count;
		task::coarse_parallel(
			chunk_count,
			1,
			[body_data, scan_data, body_count, sparse](const std::size_t c) {
				auto& res = scan_data[c];
				res.statics.clear();
				res.inputs.clear();
				res.max_extent = meters(0.5f);
				const auto begin = static_cast<std::uint32_t>(c * scan_chunk);
				const auto end = std::min(begin + scan_chunk, body_count);
				for (std::uint32_t i = begin; i < end; ++i) {
					const auto& b = body_data[i];
					if (b.locked != 0) {
						res.statics.push_back(i);
						if (sparse) {
							res.inputs.push_back(i);
						}
						continue;
					}
					if (sparse && b.reset_pending != 0) {
						res.inputs.push_back(i);
					}
					const auto& he = b.half_extents;
					res.max_extent = std::max({ res.max_extent, he.x(), he.y(), he.z() });
				}
			},
			trace_id<"vbd_gpu::upload::body_scan::chunks">()
		);

		displacement max_extent = meters(0.5f);
		m_upload_static_bodies.clear();
		m_upload_static_bodies.push_back(0u);
		m_upload_body_input_indices.clear();
		m_upload_body_input_indices.push_back(0u);
		for (const auto& res : m_body_scan) {
			m_upload_static_bodies.insert(m_upload_static_bodies.end(), res.statics.begin(), res.statics.end());
			m_upload_body_input_indices.insert(m_upload_body_input_indices.end(), res.inputs.begin(), res.inputs.end());
			max_extent = std::max(max_extent, res.max_extent);
		}
		m_upload_static_bodies[0] = static_cast<std::uint32_t>(m_upload_static_bodies.size() - 1);
		m_upload_body_input_indices[0] = static_cast<std::uint32_t>(m_upload_body_input_indices.size() - 1);
		m_grid_cell_size = max_extent * 4.0f;
	}

	if (m_apply_all_body_inputs) {
		parallel_host_write<body_state>(m_body_input_channel.write_target(), bodies.first(m_body_count), trace_id<"vbd_gpu::upload::body_write">());
	}
	else {
		trace::scope_guard _{ trace_id<"vbd_gpu::upload::body_write_sparse">() };
		const auto& target = m_body_input_channel.write_target();
		for (std::size_t k = 1; k < m_upload_body_input_indices.size(); ++k) {
			const auto bi = m_upload_body_input_indices[k];
			target.host_write(reinterpret_cast<const std::byte*>(&bodies[bi]), sizeof(body_state), bi * sizeof(body_state));
		}
	}

	{
		trace::scope_guard _{ trace_id<"vbd_gpu::upload::motor_map">() };
		m_upload_motors.assign(motors.begin(), motors.end());

		m_upload_motor_map.assign(m_capacities.max_bodies, 0xFFFFFFFFu);
		for (std::uint32_t mi = 0; mi < m_motor_count; ++mi) {
			assert(
				motors[mi].body_index < m_body_count,
				"Motor body index {} out of bounds (max {})",
				motors[mi].body_index,
				m_body_count
			);
			if (motors[mi].body_index < m_capacities.max_bodies) {
				m_upload_motor_map[motors[mi].body_index] = mi;
			}
		}
	}

	m_upload_joints_dirty = false;
	m_upload_joint_inputs_dirty = false;
	m_merge_joint_inputs = false;
	constexpr auto unresolved = std::numeric_limits<std::uint32_t>::max();

	if (!upload_joint_buffer) {
		m_upload_joints.clear();
		if (!payload.joint_inputs.empty()) {
			trace::scope_guard _{ trace_id<"vbd_gpu::upload::joint_inputs">() };
			assert(
				resident_joints && payload.joint_inputs.size() == m_joint_slots.size(),
				"joint drive inputs ({}) arrived without resident joints ({} slots, resident {})",
				payload.joint_inputs.size(),
				m_joint_slots.size(),
				resident_joints
			);
			if (m_upload_joint_inputs.size() != m_joint_count) {
				m_upload_joint_inputs.resize(m_joint_count);
			}
			const auto* input_data = payload.joint_inputs.data();
			const auto* slot_data = m_joint_slots.data();
			auto* upload_data = m_upload_joint_inputs.data();
			task::coarse_parallel(
				payload.joint_inputs.size(),
				256,
				[input_data, slot_data, upload_data](const std::size_t i) {
					if (slot_data[i] != unresolved) {
						upload_data[slot_data[i]] = input_data[i];
					}
				},
				trace_id<"vbd_gpu::upload::joint_input_scatter">()
			);
			m_upload_joint_inputs_dirty = true;
		}
	}
	else {
		trace::scope_guard _{ trace_id<"vbd_gpu::upload::joint_rebuild">() };
		m_joint_count = 0;
		m_island_count = 0;
		m_jointed_body_mask.assign(m_body_count, 0);
		if (m_upload_joints.size() != joints.size()) {
			m_upload_joints.resize(joints.size());
		}

		const time_step sub_dt = payload.dt / static_cast<float>(std::max(m_steps, 1u));
		const time_squared h_squared = sub_dt * sub_dt;

		m_joint_slots.assign(joints.size(), unresolved);
		m_topology_key_next.clear();
		for (std::size_t i = 0; i < joints.size(); ++i) {
			const auto& j = joints[i];
			if (j.body_a >= m_body_count || j.body_b >= m_body_count) {
				continue;
			}
			m_joint_slots[i] = m_joint_count;
			m_jointed_body_mask[j.body_a] = 1;
			m_jointed_body_mask[j.body_b] = 1;
			m_topology_key_next.push_back((static_cast<std::uint64_t>(j.body_a) << 32) | j.body_b);
			++m_joint_count;
		}
		const auto* joint_data = joints.data();
		const auto* slot_data = m_joint_slots.data();
		auto* upload_data = m_upload_joints.data();
		task::coarse_parallel(
			joints.size(),
			256,
			[joint_data, slot_data, upload_data](const std::size_t i) {
				if (slot_data[i] != unresolved) {
					upload_data[slot_data[i]] = joint_data[i];
				}
			},
			trace_id<"vbd_gpu::upload::joint_copy">()
		);

		m_merge_joint_inputs = resident_joints && m_joint_count == previous_joint_count;
		if (!m_merge_joint_inputs) {
			task::coarse_parallel(
				m_joint_count,
				32,
				[&, this](const std::size_t ji) {
					auto& g = m_upload_joints[ji];
					warm_start_joint(g, bodies[g.body_a], bodies[g.body_b], h_squared, payload.solver_cfg);
					compute_joint_c0(g, bodies[g.body_a], bodies[g.body_b]);
				},
				trace_id<"vbd_gpu::upload::joint_math">()
			);
		}
		m_upload_joints_dirty = true;
		m_joint_buffers_seeded = true;

		const bool topology_changed = m_topology_body_count != m_body_count || m_topology_key != m_topology_key_next;
		if (topology_changed) {
			m_topology_key = m_topology_key_next;
			m_topology_body_count = m_body_count;
			m_topology_island_count = 0;
		}

		if (m_joint_count > 0 && topology_changed) {
			trace::scope_guard _{ trace_id<"vbd_gpu::upload::topology">() };
			std::vector<std::pair<std::uint32_t, std::uint32_t>> jointed_pairs;
			jointed_pairs.reserve(m_joint_count);
			for (std::uint32_t i = 0; i < m_joint_count; ++i) {
				const auto& g = m_upload_joints[i];
				jointed_pairs.emplace_back(std::min(g.body_a, g.body_b), std::max(g.body_a, g.body_b));
			}
			std::ranges::sort(jointed_pairs);
			const auto duplicates = std::ranges::unique(jointed_pairs);
			jointed_pairs.erase(duplicates.begin(), duplicates.end());

			m_upload_jointed_pairs.assign(1, static_cast<std::uint32_t>(jointed_pairs.size()));
			for (const auto& [body_lo, body_hi] : jointed_pairs) {
				m_upload_jointed_pairs.push_back(body_lo);
				m_upload_jointed_pairs.push_back(body_hi);
			}

			std::vector<std::uint32_t> parent(m_body_count);
			for (std::uint32_t i = 0; i < m_body_count; ++i) {
				parent[i] = i;
			}
			auto find_root = [&parent](std::uint32_t x) -> std::uint32_t {
				while (parent[x] != x) {
					parent[x] = parent[parent[x]];
					x = parent[x];
				}
				return x;
			};
			for (std::uint32_t i = 0; i < m_joint_count; ++i) {
				const auto ra = find_root(m_upload_joints[i].body_a);
				const auto rb = find_root(m_upload_joints[i].body_b);
				if (ra != rb) {
					parent[std::max(ra, rb)] = std::min(ra, rb);
				}
			}
			std::vector<std::uint32_t> root_to_island(m_body_count, 0xFFFFFFFFu);
			std::vector<std::vector<std::uint32_t>> island_bodies;
			m_upload_body_env.assign(m_capacities.max_bodies, 0xFFFFFFFFu);
			for (std::uint32_t bi = 0; bi < m_body_count; ++bi) {
				if (m_jointed_body_mask[bi] == 0) {
					continue;
				}
				const auto r = find_root(bi);
				if (root_to_island[r] == 0xFFFFFFFFu) {
					root_to_island[r] = static_cast<std::uint32_t>(island_bodies.size());
					island_bodies.emplace_back();
				}
				island_bodies[root_to_island[r]].push_back(bi);
				m_upload_body_env[bi] = root_to_island[r];
			}
			m_island_count = static_cast<std::uint32_t>(island_bodies.size());
			assert(
				m_island_count <= m_capacities.max_islands,
				"island count {} exceeds max_islands {}",
				m_island_count,
				m_capacities.max_islands
			);

			const std::uint32_t island_base = 1 + 2 * m_capacities.max_islands;
			m_upload_islands.assign(island_base + m_capacities.max_bodies, 0u);
			m_upload_islands[0] = m_island_count;
			std::uint32_t flat = 0;
			for (std::uint32_t i = 0; i < m_island_count; ++i) {
				m_upload_islands[1 + i] = flat;
				m_upload_islands[1 + m_capacities.max_islands + i] = static_cast<std::uint32_t>(island_bodies[i].size());
				for (const auto b : island_bodies[i]) {
					m_upload_islands[island_base + flat] = b;
					++flat;
				}
			}
			m_topology_island_count = m_island_count;
			std::uint32_t largest_island = 0;
			for (const auto& bodies : island_bodies) {
				largest_island = std::max(largest_island, static_cast<std::uint32_t>(bodies.size()));
			}
			log::println(
				log::category::physics,
				"vbd topology: {} islands over {} jointed bodies of {} bodies, {} joints, largest island {} bodies",
				m_island_count,
				flat,
				m_body_count,
				m_joint_count,
				largest_island
			);
		}
		else {
			m_island_count = m_topology_island_count;
		}
	}

	m_jointless_body_count = 0;
	for (std::uint32_t i = 0; i < m_body_count; ++i) {
		if (!bodies[i].locked && (i >= m_jointed_body_mask.size() || m_jointed_body_mask[i] == 0)) {
			++m_jointless_body_count;
		}
	}

	m_pending_dispatch = true;
}

auto gse::vbd::gpu_solver::commit_upload() -> void {
	if (!m_buffers_created || !m_pending_dispatch || m_body_count == 0) {
		return;
	}

	m_solver_config_channel.write_target().host_write(m_solver_cfg, 0);
	m_motor_map_channel.write_target().host_write(m_upload_motor_map);

	if (!m_upload_motors.empty()) {
		m_motor_channel.write_target().host_write(m_upload_motors);
	}

	if (!m_upload_jointed_pairs.empty()) {
		m_jointed_pairs_channel.write_target().host_write(m_upload_jointed_pairs);
	}

	if (!m_upload_islands.empty()) {
		m_island_channel.write_target().host_write(m_upload_islands);
	}

	if (!m_upload_body_env.empty()) {
		m_body_env_channel.write_target().host_write(m_upload_body_env);
	}

	if (!m_upload_static_bodies.empty()) {
		m_static_bodies_channel.write_target().host_write(m_upload_static_bodies);
	}

	if (!m_upload_body_input_indices.empty()) {
		m_body_input_index_channel.write_target().host_write(m_upload_body_input_indices);
	}

	if (m_upload_joints_dirty && !m_upload_joints.empty()) {
		parallel_host_write<joint_constraint>(m_joint_upload_channel.write_target(), std::span<const joint_constraint>(m_upload_joints), trace_id<"vbd_gpu::upload::joint_write">());
	}

	if (m_upload_joint_inputs_dirty && !m_upload_joint_inputs.empty()) {
		parallel_host_write<joint_drive_input>(m_joint_drive_input_channel.write_target(), std::span<const joint_drive_input>(m_upload_joint_inputs), trace_id<"vbd_gpu::upload::joint_input_write">());
	}

	if (!m_upload_impulses.empty()) {
		m_impulse_channel.write_target().host_write(m_upload_impulses);
	}
}

auto gse::vbd::gpu_solver::set_color_launch_hint(const std::uint32_t max_used_color) -> void {
	m_color_launch_hint = max_used_color;
}

auto gse::vbd::gpu_solver::set_preserve_warm_starts(const bool preserve) -> void {
	m_preserve_warm_starts = preserve;
}

auto gse::vbd::gpu_solver::read_grounded() const -> std::span<const std::uint32_t> {
	if (!m_buffers_created) {
		return {};
	}
	const auto view = m_grounded_channel.latest();
	return std::span<const std::uint32_t>(
		reinterpret_cast<const std::uint32_t*>(view.bytes.data()),
		view.bytes.size() / sizeof(std::uint32_t)
	);
}

auto gse::vbd::gpu_solver::read_narrow_phase_debug() const -> std::span<const std::uint32_t> {
	if (!m_buffers_created) {
		return {};
	}
	const auto view = m_collision_state_channel.latest();
	return std::span<const std::uint32_t>(
		reinterpret_cast<const std::uint32_t*>(view.bytes.data()),
		view.bytes.size() / sizeof(std::uint32_t)
	);
}

auto gse::vbd::gpu_solver::read_contact_dump() const -> std::span<const contact_constraint> {
	if (!m_buffers_created) {
		return {};
	}
	const auto view = m_contact_dump_channel.latest();
	return std::span<const contact_constraint>(
		reinterpret_cast<const contact_constraint*>(view.bytes.data()),
		view.bytes.size() / sizeof(contact_constraint)
	);
}

auto gse::vbd::gpu_solver::read_adjacency_meta() const -> std::span<const std::uint32_t> {
	if (!m_buffers_created) {
		return {};
	}
	const auto view = m_adjacency_meta_channel.latest();
	return std::span<const std::uint32_t>(
		reinterpret_cast<const std::uint32_t*>(view.bytes.data()),
		view.bytes.size() / sizeof(std::uint32_t)
	);
}

auto gse::vbd::gpu_solver::read_contact_adjacency_dump() const -> std::span<const std::uint32_t> {
	if (!m_buffers_created) {
		return {};
	}
	const auto view = m_contact_adjacency_channel.latest();
	return std::span<const std::uint32_t>(
		reinterpret_cast<const std::uint32_t*>(view.bytes.data()),
		view.bytes.size() / sizeof(std::uint32_t)
	);
}

auto gse::vbd::gpu_solver::read_body_states() const -> std::span<const body_state> {
	if (!m_buffers_created) {
		return {};
	}
	trace::scope_guard _{ trace_id<"vbd_gpu::readback::map">() };
	const auto view = m_snapshot_channel.latest();
	return std::span<const body_state>(
		reinterpret_cast<const body_state*>(view.bytes.data()),
		view.bytes.size() / sizeof(body_state)
	);
}

auto gse::vbd::gpu_solver::diagnostics() const -> solver_diagnostics {
	if (!m_buffers_created) {
		return {};
	}
	const auto view = m_collision_state_channel.latest();
	if (view.bytes.size() < limits.collision_state_header_uints * sizeof(std::uint32_t)) {
		return {};
	}
	const auto* header = reinterpret_cast<const std::uint32_t*>(view.bytes.data());
	std::array<std::uint32_t, limits.max_colors> populations{};
	for (std::uint32_t c = 0; c < limits.max_colors; ++c) {
		populations[c] = header[limits.state_color_population_base_index + c];
	}
	return {
		.attempted_contacts = header[limits.state_contact_count_index],
		.max_used_color = header[limits.state_max_used_color_index],
		.coloring_fallbacks = header[limits.state_coloring_fallback_index],
		.coloring_conflicts = header[limits.state_coloring_conflict_index],
		.contact_duplicates = header[limits.state_contact_duplicate_index],
		.warm_start_hits = header[limits.state_warm_start_hit_index],
		.stale_reads = header[limits.state_stale_read_count_index],
		.stale_checks = header[limits.state_stale_check_count_index],
		.sweep_bails = header[limits.state_sweep_bail_index],
		.color_populations = populations,
		.broad_nodes_walked = header[limits.state_broad_nodes_index],
		.broad_aabb_tests = header[limits.state_broad_tests_index],
		.broad_pairs = header[limits.state_pair_count_index],
		.joint_lambda_max = newtons(std::bit_cast<float>(header[limits.state_joint_lambda_index])),
		.joint_penalty_max = newtons_per_meter(std::bit_cast<float>(header[limits.state_joint_penalty_index])),
		.joint_c_max = meters(std::bit_cast<float>(header[limits.state_joint_c_index])),
		.joint_c0_max = meters(std::bit_cast<float>(header[limits.state_joint_c0_index])),
	};
}

auto gse::vbd::gpu_solver::query_body_snapshot(const std::uint32_t body_index) const -> std::optional<body_state> {
	if (!m_buffers_created || body_index >= m_body_count) {
		return std::nullopt;
	}
	const auto view = m_snapshot_channel.latest();
	if (body_index >= view.bytes.size() / sizeof(body_state)) {
		return std::nullopt;
	}
	const auto* bodies = reinterpret_cast<const body_state*>(view.bytes.data());
	return bodies[body_index];
}

auto gse::vbd::gpu_solver::pending_dispatch() const -> bool {
	return m_pending_dispatch;
}

auto gse::vbd::gpu_solver::reseeding() const -> bool {
	return m_apply_all_body_inputs;
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

auto gse::vbd::gpu_solver::render_body_buffer() const -> const gpu::buffer& {
	return m_frames[1 - m_dispatch_slot].render_body_buffer;
}

auto gse::vbd::gpu_solver::dispatch_generation() const -> std::uint64_t {
	return m_dispatch_generation;
}

auto gse::vbd::gpu_solver::retired_generation() const -> std::uint64_t {
	if (!m_buffers_created) {
		return 0;
	}
	return m_snapshot_channel.latest().generation;
}

auto gse::vbd::gpu_solver::readback_age_steps() const -> int {
	const auto served = retired_generation();
	if (served == 0 || m_dispatch_generation == 0 || m_dispatch_generation - served >= m_generation_ticks.size()) {
		return 0;
	}
	return static_cast<int>(m_ticks_dispatched - m_generation_ticks[served % m_generation_ticks.size()]);
}

auto gse::vbd::gpu_solver::readback_tick() const -> std::optional<std::uint64_t> {
	const auto served = retired_generation();
	if (served == 0 || m_dispatch_generation == 0 || m_dispatch_generation - served >= m_generation_end_tick.size()) {
		return std::nullopt;
	}
	return m_generation_end_tick[served % m_generation_end_tick.size()];
}

auto gse::vbd::gpu_solver::latest_dispatch_complete() const -> bool {
	if (m_dispatch_generation == 0 || retired_generation() == m_dispatch_generation) {
		return true;
	}
	if (m_frame->frame_count() == m_recorded_frame) {
		return false;
	}
	return m_frame->queue_fence_signaled(gpu::queue_type::compute, m_recorded_ring);
}

auto gse::vbd::gpu_solver::wait_for_latest_dispatch() const -> void {
	if (!m_sync_readback || !m_buffers_created || m_dispatch_generation == 0 || retired_generation() == m_dispatch_generation) {
		return;
	}
	trace::scope_guard _{ trace_id<"vbd_gpu::readback::wait">() };
	m_frame->wait_queue_fence(gpu::queue_type::compute, m_recorded_ring);
}

auto gse::vbd::gpu_solver::initialize_compute(context& ctx, const shared_view<gpu::context::data> gpu_s, const vbd_capacities& capacities, const bool sync_readback) -> async::task<> {
	assert(gpu_s.device != nullptr, "gpu_solver::initialize_compute requires gpu::context to be initialized first");
	m_sync_readback = sync_readback;
	m_capacities = capacities;
	assert(
		m_capacities.max_bodies <= (1u << 18) && m_capacities.max_contacts <= (1u << 18) && limits.adjacency_workgroup_size <= 1024u,
		"restitution sort key packs 18-bit body indices, 18-bit contact indices and a 10-bit island slot: max_bodies {} max_contacts {} adjacency_workgroup_size {}",
		m_capacities.max_bodies,
		m_capacities.max_contacts,
		limits.adjacency_workgroup_size
	);
	m_topology_key.clear();
	m_topology_body_count = 0;
	m_topology_island_count = 0;

	const auto capacity_constants = shaders::emit_slang_constants(m_capacities);
	const auto build = [&](const auto& pod) {
		return gpu::build_compute_program(*gpu_s.device, pod, {}, capacity_constants);
	};

	for (std::uint32_t color = 0; color < limits.max_colors; ++color) {
		m_solve_marks.color[color] = find_or_generate_id(std::format("vbd::solve::color[{}]", color));
	}
	m_solve_marks.sweep = find_or_generate_id("vbd::solve::sweep");
	m_solve_marks.island = find_or_generate_id("vbd::solve::island");
	m_solve_marks.jacobi = find_or_generate_id("vbd::solve::jacobi");
	m_solve_marks.apply_jacobi = find_or_generate_id("vbd::solve::apply_jacobi");
	m_solve_marks.update_lambda = find_or_generate_id("vbd::solve::update_lambda");
	m_solve_marks.joint_lambda = find_or_generate_id("vbd::solve::joint_lambda");
	m_solve_marks.convergence = find_or_generate_id("vbd::solve::convergence");

	m_compute.predict_pipeline = build(predict_entry::pod);
	m_compute.solve_color_pipeline = build(solve_color_entry::pod);
	m_compute.solve_island_pipeline = build(solve_island_entry::pod);
	m_compute.solve_sweep_pipeline = build(solve_sweep_entry::pod);
	m_compute.update_lambda_pipeline = build(update_lambda_entry::pod);
	m_compute.derive_velocities_pipeline = build(derive_velocities_entry::pod);
	m_compute.finalize_pipeline = build(finalize_entry::pod);
	m_compute.collision_reset_pipeline = build(collision_reset_entry::pod);
	m_compute.collision_broad_phase_pipeline = build(collision_broad_phase_entry::pod);
	m_compute.collision_narrow_phase_pipeline = build(collision_narrow_phase_entry::pod);
	m_compute.collision_grid_build_pipeline = build(collision_grid_build_entry::pod);
	m_compute.collision_build_adjacency_pipeline = build(collision_build_adjacency_entry::pod);
	m_compute.collision_sort_adjacency_pipeline = build(collision_sort_adjacency_entry::pod);
	m_compute.collision_build_coloring_pipeline = build(collision_build_coloring_entry::pod);
	m_compute.collision_color_round_pipeline = build(collision_color_round_entry::pod);
	m_compute.collision_color_commit_pipeline = build(collision_color_commit_entry::pod);
	m_compute.update_joint_lambda_pipeline = build(update_joint_lambda_entry::pod);
	m_compute.prepare_indirect_pipeline = build(prepare_indirect_entry::pod);
	m_compute.prepare_contact_indirect_pipeline = build(prepare_contact_indirect_entry::pod);
	m_compute.prepare_color_indirect_pipeline = build(prepare_color_indirect_entry::pod);
	m_compute.freeze_jacobians_pipeline = build(freeze_jacobians_entry::pod);
	m_compute.apply_jacobi_pipeline = build(apply_jacobi_entry::pod);
	m_compute.apply_restitution_pipeline = build(apply_restitution_serial_entry::pod);
	m_compute.update_sticking_pipeline = build(update_sticking_entry::pod);
	m_compute.apply_impulses_pipeline = build(apply_impulses_entry::pod);
	m_compute.apply_body_inputs_pipeline = build(apply_body_inputs_entry::pod);
	m_compute.apply_joint_inputs_pipeline = build(apply_joint_inputs_entry::pod);
	m_compute.apply_joint_drive_inputs_pipeline = build(apply_joint_drive_inputs_entry::pod);
	m_compute.convergence_check_pipeline = build(convergence_check_entry::pod);
	m_compute.hash_state_pipeline = build(hash_state_entry::pod);
	m_compute.hash_warm_inputs_pipeline = build(hash_warm_inputs_entry::pod);
	m_compute.hash_adjacency_pipeline = build(hash_adjacency_entry::pod);
	m_compute.hash_colors_pipeline = build(hash_colors_entry::pod);
	m_compute.hash_bodies_pipeline = build(hash_bodies_entry::pod);

	create_buffers(gpu_s);

	m_compute.initialized = true;
	co_return;
}

auto gse::vbd::gpu_solver::capacities() const -> const vbd_capacities& {
	return m_capacities;
}

namespace gse::vbd {
	constexpr std::uint32_t spare_hash_substep = 2u;
}

struct gse::vbd::gpu_solver::solve_plan {
	per_frame_data* f = nullptr;
	per_frame_data* other = nullptr;
	vbd_bindings bindings{};
	vbd_bindings jointless_bindings{};
	vbd_bindings first_substep_bindings{};
	time_step sub_dt{};
	time_squared h_squared{};
	gap grid_cell_size{};
	std::uint32_t body_count = 0;
	std::uint32_t contact_capacity = 0;
	std::uint32_t motor_count = 0;
	std::uint32_t joint_count = 0;
	std::uint32_t impulse_count = 0;
	std::uint32_t island_count = 0;
	std::uint32_t jointless_body_count = 0;
	std::uint32_t body_workgroups = 0;
	std::uint32_t body_input_workgroups = 0;
	std::uint32_t reset_workgroups = 0;
	std::uint32_t joint_workgroups = 0;
	std::uint32_t adjacency_workgroups = 0;
	std::uint32_t impulse_workgroups = 0;
	std::uint32_t sweep_workgroups = 0;
	std::uint32_t color_cap = 0;
	std::uint32_t color_launch_bound = 0;
	std::uint32_t num_iterations = 0;
	std::uint32_t adaptive_iterations = 0;
	std::uint32_t substeps = 0;
	std::uint32_t substeps_per_tick = 1;
	std::vector<std::uint32_t> impulse_offsets;
	std::vector<std::uint32_t> impulse_counts;
	std::size_t joint_upload_size = 0;
	float solve_alpha = 0.f;
	bool apply_all_body_inputs = false;
	bool preserve_warm_starts = false;
	bool use_jacobi = false;
	bool use_solve_fold = false;
	bool post_stabilize = false;
	bool diag_hashes = false;
	bool upload_joints = false;
	bool merge_joint_inputs = false;
	bool apply_joint_drive_inputs = false;
	bool seed_device_local = false;

	[[nodiscard]] auto push_constants(
		std::uint32_t color_offset,
		std::uint32_t color_count,
		std::uint32_t substep,
		std::uint32_t iteration,
		float current_alpha
	) const -> vbd_push_constants;
};

auto gse::vbd::gpu_solver::solve_plan::push_constants(const std::uint32_t color_offset, const std::uint32_t color_count, const std::uint32_t substep, const std::uint32_t iteration, const float current_alpha) const -> vbd_push_constants {
	const std::uint32_t tick = substep / std::max(substeps_per_tick, 1u);
	const std::uint32_t tick_impulses = tick < impulse_counts.size() ? impulse_counts[tick] : 0u;
	const std::uint32_t tick_impulse_offset = tick < impulse_offsets.size() ? impulse_offsets[tick] : 0u;
	return vbd_push_constants{
		.body_count = body_count,
		.contact_count = contact_capacity,
		.motor_count = motor_count,
		.joint_count = joint_count,
		.impulse_count = tick_impulses,
		.color_offset = color_offset,
		.color_count = color_count,
		.substep = substep,
		.iteration = iteration,
		.h_squared = h_squared,
		.dt = sub_dt,
		.current_alpha = current_alpha,
		.grid_cell_size = grid_cell_size,
		.apply_all_body_inputs = apply_all_body_inputs ? 1u : 0u,
		.preserve_accel_weight = (apply_all_body_inputs && preserve_warm_starts) ? 1u : 0u,
		.sweep_workgroups = sweep_workgroups,
		.substeps_per_tick = std::max(substeps_per_tick, 1u),
		.impulse_offset = tick_impulse_offset,
		.fold_lambda = 0u,
		.lambda_pass = 0u,
	};
}

auto gse::vbd::gpu_solver::build_solve_plan(solve_plan& out) -> void {
	auto& f = m_frames[m_dispatch_slot];
	auto& other = m_frames[1 - m_dispatch_slot];

	const vbd_bindings bindings{
		.body_data = f.body_buffer.slot(),
		.contact_data = f.contact_buffer.slot(),
		.motor_data = m_motor_channel.current().slot(),
		.color_data = f.color_buffer.slot(),
		.contact_offsets = f.contact_offsets_buffer.slot(),
		.solve_state = f.solve_state_buffer.slot(),
		.collision_pairs = f.collision_pair_buffer.slot(),
		.collision_state = f.collision_state_buffer.slot(),
		.warm_starts = f.warm_start_buffer.slot(),
		.joint_data = f.joint_buffer.slot(),
		.contact_counts = f.contact_counts_buffer.slot(),
		.contact_adjacency = f.contact_adjacency_buffer.slot(),
		.motor_map = m_motor_map_channel.current().slot(),
		.joint_offsets = f.joint_offsets_buffer.slot(),
		.joint_counts = f.joint_counts_buffer.slot(),
		.joint_adjacency = f.joint_adjacency_buffer.slot(),
		.grid_data = f.grid_buffer.slot(),
		.indirect_args = f.indirect_dispatch_buffer.slot(),
		.frozen_jacobians = f.frozen_jacobian_buffer.slot(),
		.solve_deltas = f.solve_deltas_buffer.slot(),
		.grounded_bits = f.grounded_buffer.slot(),
		.impulse_data = m_impulse_channel.current().slot(),
		.solver_config_data = m_solver_config_channel.current().slot(),
		.jointed_pairs_data = m_jointed_pairs_channel.current().slot(),
		.body_input_data = m_body_input_channel.current().slot(),
		.jointless_color_data = f.jointless_color_buffer.slot(),
		.jointless_indirect_args = f.jointless_indirect_dispatch_buffer.slot(),
		.island_data = m_island_channel.current().slot(),
		.body_env_data = m_body_env_channel.current().slot(),
		.static_bodies_data = m_static_bodies_channel.current().slot(),
		.coloring_scratch = f.coloring_scratch_buffer.slot(),
		.joint_input_data = m_joint_upload_channel.current().slot(),
		.joint_drive_input_data = m_joint_drive_input_channel.current().slot(),
		.unowned_contacts = f.unowned_contact_buffer.slot(),
		.body_input_index_data = m_body_input_index_channel.current().slot(),
	};
	auto jointless_bindings = bindings;
	jointless_bindings.color_data = f.jointless_color_buffer.slot();
	jointless_bindings.indirect_args = f.jointless_indirect_dispatch_buffer.slot();
	auto first_substep_bindings = bindings;
	first_substep_bindings.warm_starts = other.warm_start_buffer.slot();

	const std::uint32_t total = m_steps;
	const time_step sub_dt = m_dt / static_cast<float>(total);
	const auto h_squared = sub_dt * sub_dt;

	auto ceil_div = [](const std::uint32_t a, const std::uint32_t b) {
		return (a + b - 1) / b;
	};

	const std::uint32_t sweep_workgroups = m_solver_cfg.sweep_workgroups != 0 ? std::min(m_solver_cfg.sweep_workgroups, 256u) : limits.solve_sweep_workgroups;
	const std::uint32_t color_cap = m_solver_cfg.color_cap != 0 ? std::min(m_solver_cfg.color_cap, limits.max_colors) : 0u;

	constexpr std::uint32_t num_colors = limits.max_colors;

	std::uint32_t color_launch_bound = num_colors;
	if (!m_apply_all_body_inputs && m_color_launch_hint > 0) {
		color_launch_bound = std::min(num_colors, std::max(m_color_launch_hint + 4u, 8u));
	}

	out.f = std::addressof(f);
	out.other = std::addressof(other);
	out.bindings = bindings;
	out.jointless_bindings = jointless_bindings;
	out.first_substep_bindings = first_substep_bindings;
	out.sub_dt = sub_dt;
	out.h_squared = h_squared;
	out.grid_cell_size = m_grid_cell_size;
	out.body_count = m_body_count;
	out.contact_capacity = m_capacities.max_contacts;
	out.motor_count = m_motor_count;
	out.joint_count = m_joint_count;
	out.impulse_count = m_impulse_count;
	out.island_count = m_island_count;
	out.jointless_body_count = m_jointless_body_count;
	out.body_workgroups = ceil_div(m_body_count, limits.workgroup_size);
	out.body_input_workgroups = m_apply_all_body_inputs
		? out.body_workgroups
		: ceil_div(std::max<std::uint32_t>(m_upload_body_input_indices.empty() ? 0u : m_upload_body_input_indices[0], 1u), limits.workgroup_size);
	out.reset_workgroups = ceil_div(
		std::max({ m_body_count, m_capacities.max_contacts, m_capacities.grid_table_size }),
		limits.workgroup_size
	);
	out.joint_workgroups = ceil_div(std::max(m_joint_count, 1u), limits.workgroup_size);
	out.adjacency_workgroups = ceil_div(std::max(m_body_count, m_joint_count), limits.workgroup_size);
	out.impulse_workgroups = ceil_div(m_impulse_count, limits.workgroup_size);
	out.sweep_workgroups = sweep_workgroups;
	out.color_cap = color_cap;
	out.color_launch_bound = color_launch_bound;
	out.num_iterations = m_solver_cfg.iterations;
	out.adaptive_iterations = std::max(m_solver_cfg.iterations, m_solver_cfg.max_iterations);
	out.substeps = total;
	out.substeps_per_tick = std::max(total / std::max(m_ticks, 1u), 1u);
	out.impulse_offsets = m_impulse_offsets;
	out.impulse_counts = m_impulse_counts;
	out.joint_upload_size = m_upload_joints.size() * sizeof(joint_constraint);
	out.solve_alpha = m_solver_cfg.post_stabilize ? 1.f : m_solver_cfg.alpha;
	out.apply_all_body_inputs = m_apply_all_body_inputs;
	out.preserve_warm_starts = m_preserve_warm_starts;
	out.use_jacobi = m_solver_cfg.use_jacobi;
	out.use_solve_fold = m_solver_cfg.use_solve_fold != 0;
	out.post_stabilize = m_solver_cfg.post_stabilize;
	out.diag_hashes = m_solver_cfg.trace_hashes != 0;
	out.upload_joints = m_upload_joints_dirty && !m_upload_joints.empty();
	out.merge_joint_inputs = out.upload_joints && m_merge_joint_inputs;
	out.apply_joint_drive_inputs = m_upload_joint_inputs_dirty && !m_upload_joint_inputs.empty();
	out.seed_device_local = !m_compute.device_local_seeded;
	m_compute.device_local_seeded = true;
}

auto gse::vbd::gpu_solver::stage_seed_state(const solve_plan& p, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_hash_state_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.hash_state_pipeline);

	rec.fill_buffer(p.f->collision_state_buffer, 0, limits.collision_state_header_uints * sizeof(std::uint32_t));

	if (p.seed_device_local) {
		for (auto& frame : m_frames) {
			rec.fill_buffer(frame.body_buffer, 0, m_capacities.max_bodies * sizeof(body_state));
			rec.fill_buffer(frame.contact_buffer, 0, m_capacities.max_contacts * sizeof(contact_constraint));
			rec.fill_buffer(frame.warm_start_buffer, 0, m_capacities.max_contacts * sizeof(contact_constraint));
			rec.fill_buffer(frame.contact_counts_buffer, 0, m_capacities.max_bodies * sizeof(std::uint32_t));
			rec.fill_buffer(frame.contact_offsets_buffer, 0, m_capacities.max_bodies * sizeof(std::uint32_t));
			rec.fill_buffer(frame.contact_adjacency_buffer, 0, m_capacities.max_contacts * 2 * sizeof(std::uint32_t));
		}
	}

	if (p.diag_hashes) {
		rec.dispatch<hash_state_entry>(p.push_constants(0u, 0u, spare_hash_substep, 2u, 0.f), p.bindings, vec3u{ 1u, 1u, 1u });
	}
}

auto gse::vbd::gpu_solver::stage_hash_state(const solve_plan& p, const std::uint32_t substep, const std::uint32_t slot, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_hash_state_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.hash_state_pipeline);

	rec.dispatch<hash_state_entry>(p.push_constants(0u, 0u, substep, slot, 0.f), p.bindings, vec3u{ 1u, 1u, 1u });
}

auto gse::vbd::gpu_solver::stage_hash_state_alt_body(const solve_plan& p, const std::uint32_t substep, const std::uint32_t slot, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_hash_state_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.hash_state_pipeline);

	auto alt_entry_bindings = p.bindings;
	alt_entry_bindings.body_data = p.f->body_alt_view.slot();

	rec.dispatch<hash_state_entry>(p.push_constants(0u, 0u, substep, slot, 0.f), alt_entry_bindings, vec3u{ 1u, 1u, 1u });
}

auto gse::vbd::gpu_solver::stage_hash_bodies(const solve_plan& p, const std::uint32_t substep, const std::uint32_t slot, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_hash_state_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.hash_bodies_pipeline);

	rec.dispatch<hash_bodies_entry>(p.push_constants(0u, 0u, substep, slot, 0.f), p.bindings, vec3u{ 1u, 1u, 1u });
}

auto gse::vbd::gpu_solver::stage_hash_bodies_other(const solve_plan& p, const std::uint32_t substep, const std::uint32_t slot, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_hash_state_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.hash_bodies_pipeline);

	auto other_body_bindings = p.bindings;
	other_body_bindings.body_data = p.other->body_buffer.slot();

	rec.dispatch<hash_bodies_entry>(p.push_constants(0u, 0u, substep, slot, 0.f), other_body_bindings, vec3u{ 1u, 1u, 1u });
}

auto gse::vbd::gpu_solver::stage_hash_adjacency(const solve_plan& p, const std::uint32_t substep, const std::uint32_t slot, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_hash_state_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.hash_adjacency_pipeline);

	rec.dispatch<hash_adjacency_entry>(p.push_constants(0u, 0u, substep, slot, 0.f), p.bindings, vec3u{ 1u, 1u, 1u });
}

auto gse::vbd::gpu_solver::stage_hash_colors(const solve_plan& p, const std::uint32_t substep, const std::uint32_t slot, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_hash_state_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.hash_colors_pipeline);

	rec.dispatch<hash_colors_entry>(p.push_constants(0u, 0u, substep, slot, 0.f), p.bindings, vec3u{ 1u, 1u, 1u });
}

auto gse::vbd::gpu_solver::stage_hash_warm_inputs(const solve_plan& p, const std::uint32_t sub, const std::uint32_t substep, const std::uint32_t slot, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_hash_state_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.hash_warm_inputs_pipeline);

	rec.dispatch<hash_warm_inputs_entry>(p.push_constants(0u, 0u, substep, slot, 0.f), sub == 0 ? p.first_substep_bindings : p.bindings, vec3u{ 1u, 1u, 1u });
}

auto gse::vbd::gpu_solver::stage_apply_body_inputs(const solve_plan& p, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_apply_body_inputs_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.apply_body_inputs_pipeline);

	rec.dispatch<apply_body_inputs_entry>(
		p.push_constants(0u, 0u, 0u, 0u, 0.f),
		p.bindings,
		vec3u{ p.body_input_workgroups, 1u, 1u }
	);
}

auto gse::vbd::gpu_solver::stage_apply_joint_inputs(const solve_plan& p, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_apply_joint_inputs_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.apply_joint_inputs_pipeline);

	rec.dispatch<apply_joint_inputs_entry>(
		p.push_constants(0u, 0u, 0u, 0u, 0.f),
		p.bindings,
		vec3u{ p.joint_workgroups, 1u, 1u }
	);
}

auto gse::vbd::gpu_solver::stage_apply_joint_drive_inputs(const solve_plan& p, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_apply_joint_drive_inputs_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.apply_joint_drive_inputs_pipeline);

	rec.dispatch<apply_joint_drive_inputs_entry>(
		p.push_constants(0u, 0u, 0u, 0u, 0.f),
		p.bindings,
		vec3u{ p.joint_workgroups, 1u, 1u }
	);
}

auto gse::vbd::gpu_solver::stage_render_mirror(const solve_plan& p, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_render_mirror_stage>(pass_out).on(gpu::queue_type::compute).in_chain<vbd_solve_chain>(chain_index).early_signal();
	rec.copy_buffer(p.f->body_buffer, p.f->render_body_buffer, p.body_count * sizeof(body_state));
}

auto gse::vbd::gpu_solver::stage_clear_state_buffers(const solve_plan& p, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	const std::size_t solve_state_clear_size = m_capacities.max_bodies * limits.solve_state_float4s_per_body * sizeof(float) * 4;
	const std::size_t solve_deltas_clear_size = m_capacities.max_bodies * 2 * sizeof(float) * 4;

	auto& f = *p.f;
	auto& other = *p.other;

	auto rec = co_await gpu::pass<vbd_clear_state_buffers_stage>(pass_out).on(gpu::queue_type::compute).in_chain<vbd_solve_chain>(chain_index);
	rec.fill_buffer(f.solve_state_buffer, 0, solve_state_clear_size);
	rec.fill_buffer(f.solve_deltas_buffer, 0, solve_deltas_clear_size);

	if (p.upload_joints && !p.merge_joint_inputs) {
		rec.copy_buffer(m_joint_upload_channel.current(), f.joint_buffer, p.joint_upload_size);
	}

	if (!p.apply_all_body_inputs || p.preserve_warm_starts) {
		rec.copy_buffer(other.contact_counts_buffer, f.contact_counts_buffer, m_capacities.max_bodies * sizeof(std::uint32_t));
		rec.copy_buffer(other.contact_offsets_buffer, f.contact_offsets_buffer, m_capacities.max_bodies * sizeof(std::uint32_t));
		rec.copy_buffer(other.contact_adjacency_buffer, f.contact_adjacency_buffer, m_capacities.max_contacts * 2 * sizeof(std::uint32_t));
	}

	if (p.apply_all_body_inputs) {
		rec.fill_buffer(f.contact_buffer, 0, m_capacities.max_contacts * sizeof(contact_constraint));
		rec.fill_buffer(f.contact_offsets_buffer, 0, m_capacities.max_bodies * sizeof(std::uint32_t));
		rec.fill_buffer(f.contact_counts_buffer, 0, m_capacities.max_bodies * sizeof(std::uint32_t));
		rec.fill_buffer(f.contact_adjacency_buffer, 0, m_capacities.max_contacts * 2 * sizeof(std::uint32_t));
		rec.fill_buffer(f.collision_pair_buffer, 0, sizeof(std::uint32_t) + m_capacities.max_collision_pairs * 2 * sizeof(std::uint32_t));
		rec.fill_buffer(f.collision_state_buffer, 0, (limits.collision_state_header_uints + limits.max_narrow_phase_debug_records * limits.narrow_phase_debug_record_uints) * sizeof(std::uint32_t));
		rec.fill_buffer(f.grid_buffer, 0, (1 + m_capacities.grid_table_size + m_capacities.max_bodies * 8 * 2) * sizeof(std::uint32_t));
		rec.fill_buffer(f.grounded_buffer, 0, m_capacities.max_grounded_uints * sizeof(std::uint32_t));
		rec.fill_buffer(f.color_buffer, 0, limits.max_colors * sizeof(std::uint32_t) * 2 + m_capacities.max_bodies * sizeof(std::uint32_t));
		rec.fill_buffer(f.jointless_color_buffer, 0, limits.max_colors * sizeof(std::uint32_t) * 2 + m_capacities.max_bodies * sizeof(std::uint32_t));
		rec.fill_buffer(f.indirect_dispatch_buffer, 0, (3 + limits.max_colors) * 3 * sizeof(std::uint32_t));
		rec.fill_buffer(f.jointless_indirect_dispatch_buffer, 0, (2 + limits.max_colors) * 3 * sizeof(std::uint32_t));
	}
}

auto gse::vbd::gpu_solver::stage_collision_reset(const solve_plan& p, const std::uint32_t sub, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_collision_reset_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.collision_reset_pipeline);

	rec.dispatch<collision_reset_entry>(
		p.push_constants(0u, 0u, sub, 0u, 0.f),
		p.bindings,
		vec3u{ p.reset_workgroups, 1u, 1u }
	);
}

auto gse::vbd::gpu_solver::stage_grid_build(const solve_plan& p, const std::uint32_t sub, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_grid_build_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.collision_grid_build_pipeline);

	rec.dispatch<collision_grid_build_entry>(
		p.push_constants(0u, 0u, sub, 0u, 0.f),
		p.bindings,
		vec3u{ p.body_workgroups, 1u, 1u }
	);
}

auto gse::vbd::gpu_solver::stage_broad_phase(const solve_plan& p, const std::uint32_t sub, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_broad_phase_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.collision_broad_phase_pipeline);

	rec.dispatch<collision_broad_phase_entry>(
		p.push_constants(0u, 0u, sub, 0u, 0.f),
		p.bindings,
		vec3u{ p.body_workgroups, 1u, 1u }
	);
}

auto gse::vbd::gpu_solver::stage_prepare_indirect(const solve_plan& p, const std::uint32_t sub, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_prepare_indirect_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.prepare_indirect_pipeline);

	rec.dispatch<prepare_indirect_entry>(
		p.push_constants(0u, 0u, sub, 0u, 0.f),
		p.bindings,
		vec3u{ 1u, 1u, 1u }
	);
}

auto gse::vbd::gpu_solver::stage_narrow_phase(const solve_plan& p, const std::uint32_t sub, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_narrow_phase_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.collision_narrow_phase_pipeline);

	rec.push_bindings<collision_narrow_phase_entry>(p.push_constants(0u, 0u, sub, 0u, 0.f), sub == 0 ? p.first_substep_bindings : p.bindings);
	rec.dispatch_indirect(p.f->indirect_dispatch_buffer, 0);
}

auto gse::vbd::gpu_solver::stage_prepare_contact_indirect(const solve_plan& p, const std::uint32_t sub, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_prepare_contact_indirect_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.prepare_contact_indirect_pipeline);

	rec.dispatch<prepare_contact_indirect_entry>(
		p.push_constants(0u, 0u, sub, 0u, 0.f),
		p.bindings,
		vec3u{ 1u, 1u, 1u }
	);
}

auto gse::vbd::gpu_solver::stage_build_adjacency(const solve_plan& p, const std::uint32_t sub, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_build_adjacency_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.collision_build_adjacency_pipeline);

	rec.dispatch<collision_build_adjacency_entry>(
		p.push_constants(0u, 0u, sub, 0u, 0.f),
		p.bindings,
		vec3u{ 1u, 1u, 1u }
	);

	rec.bind(m_compute.collision_sort_adjacency_pipeline);
	rec.push_bindings<collision_sort_adjacency_entry>(p.push_constants(0u, 0u, sub, 0u, 0.f), p.bindings);
	rec.dispatch(p.adjacency_workgroups, 1, 1);
}

auto gse::vbd::gpu_solver::stage_build_coloring(const solve_plan& p, const std::uint32_t sub, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto& f = *p.f;
	const auto& bindings = p.bindings;
	const auto color_cap = p.color_cap;
	const auto body_workgroups = p.body_workgroups;

	if (sub == 0) {
		auto rec = co_await gpu::pass<vbd_build_coloring_stage>(pass_out)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>(chain_index)
			.pipeline(m_compute.collision_color_round_pipeline);

		rec.push_bindings<collision_color_round_entry>(p.push_constants(0u, color_cap, sub, 0u, 0.f), bindings);
		rec.dispatch(body_workgroups, 1u, 1u);
		rec.bind(m_compute.collision_color_commit_pipeline);
		rec.push_bindings<collision_color_commit_entry>(p.push_constants(0u, color_cap, sub, 0u, 0.f), bindings);
		rec.dispatch(body_workgroups, 1u, 1u);
		for (std::uint32_t round = 1; round < limits.coloring_rounds; ++round) {
			rec.bind(m_compute.collision_color_round_pipeline);
			rec.push_bindings<collision_color_round_entry>(p.push_constants(0u, color_cap, sub, 0u, 0.f), bindings);
			rec.dispatch_indirect(f.indirect_dispatch_buffer, (2 + limits.max_colors) * 3 * sizeof(std::uint32_t));
			rec.bind(m_compute.collision_color_commit_pipeline);
			rec.push_bindings<collision_color_commit_entry>(p.push_constants(0u, color_cap, sub, 0u, 0.f), bindings);
			rec.dispatch_indirect(f.indirect_dispatch_buffer, (2 + limits.max_colors) * 3 * sizeof(std::uint32_t));
		}
		rec.bind(m_compute.collision_build_coloring_pipeline);
		rec.push_bindings<collision_build_coloring_entry>(p.push_constants(0u, color_cap, sub, 0u, 0.f), bindings);
		rec.dispatch(1u, 1u, 1u);
	}
	else {
		auto rec = co_await gpu::pass<vbd_build_coloring_stage>(pass_out)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>(chain_index)
			.pipeline(m_compute.prepare_color_indirect_pipeline);

		rec.dispatch<prepare_color_indirect_entry>(
			p.push_constants(0u, 0u, sub, 0u, 0.f),
			bindings,
			vec3u{ 1u, 1u, 1u }
		);
	}
}

auto gse::vbd::gpu_solver::stage_apply_impulses(const solve_plan& p, const std::uint32_t sub, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	const auto constants = p.push_constants(0u, 0u, sub, 0u, 0.f);
	const std::uint32_t workgroups = (constants.impulse_count + limits.workgroup_size - 1) / limits.workgroup_size;

	auto rec = co_await gpu::pass<vbd_apply_impulses_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.apply_impulses_pipeline);

	rec.dispatch<apply_impulses_entry>(
		constants,
		p.bindings,
		vec3u{ workgroups, 1u, 1u }
	);
}

auto gse::vbd::gpu_solver::stage_predict(const solve_plan& p, const std::uint32_t sub, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_predict_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.predict_pipeline);

	rec.dispatch<predict_entry>(
		p.push_constants(0u, 0u, sub, 0u, 0.f),
		p.bindings,
		vec3u{ p.body_workgroups, 1u, 1u }
	);
}

auto gse::vbd::gpu_solver::stage_freeze_jacobians(const solve_plan& p, const std::uint32_t sub, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_freeze_jacobians_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.freeze_jacobians_pipeline);

	rec.push_bindings<freeze_jacobians_entry>(p.push_constants(0u, 0u, sub, 0u, 0.f), p.bindings);
	rec.dispatch_indirect(p.f->indirect_dispatch_buffer, 3 * sizeof(std::uint32_t));
}

auto gse::vbd::gpu_solver::stage_solve_iterations(const solve_plan& p, const std::uint32_t sub, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto& f = *p.f;
	const auto& bindings = p.bindings;
	const auto& jointless_bindings = p.jointless_bindings;

	auto rec = co_await gpu::pass<vbd_solve_iterations_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.solve_color_pipeline);

	const bool fold_lambda = p.joint_count > 0 && !p.use_jacobi;
	const bool island_lambda = fold_lambda && p.jointless_body_count == 0;

	for (std::uint32_t it = 0; it < p.adaptive_iterations; ++it) {
		rec.bind(m_compute.solve_color_pipeline);
		auto color_pc = p.push_constants(0u, limits.max_colors, sub, it, p.solve_alpha);

		if (p.use_jacobi) {
			rec.mark(m_solve_marks.jacobi);
			rec.push_bindings<solve_color_entry>(color_pc, bindings);
			rec.dispatch(p.body_workgroups, 1, 1);

			rec.mark(m_solve_marks.apply_jacobi);
			rec.bind(m_compute.apply_jacobi_pipeline);
			rec.push_bindings<apply_jacobi_entry>(p.push_constants(0u, 0u, sub, it, p.solve_alpha), bindings);
			rec.dispatch(p.body_workgroups, 1, 1);
		}
		else if (p.joint_count > 0) {
			if (p.jointless_body_count > 0) {
				if (p.use_solve_fold) {
					color_pc.color_count = p.color_launch_bound;
					rec.mark(m_solve_marks.sweep);
					rec.bind(m_compute.solve_sweep_pipeline);
					rec.push_bindings<solve_sweep_entry>(color_pc, jointless_bindings);
					rec.dispatch(p.sweep_workgroups, 1u, 1u);
					rec.bind(m_compute.solve_color_pipeline);
				}
				else {
					for (std::uint32_t color = 0; color < p.color_launch_bound; ++color) {
						color_pc.color_offset = color;
						rec.mark(m_solve_marks.color[color]);
						rec.push_bindings<solve_color_entry>(color_pc, jointless_bindings);
						rec.dispatch_indirect(f.jointless_indirect_dispatch_buffer, (2 + color) * 3 * sizeof(std::uint32_t));
					}
				}
				color_pc.color_count = 0u;
			}
			color_pc.color_offset = 0xFFFFFFFFu;
			color_pc.fold_lambda = 1u;
			if (island_lambda) {
				color_pc.lambda_pass = it + 1 >= p.num_iterations ? 2u : 1u;
			}
			rec.mark(m_solve_marks.island);
			rec.bind(m_compute.solve_island_pipeline);
			rec.push_bindings<solve_island_entry>(color_pc, bindings);
			rec.dispatch(std::max(p.island_count, 1u), 1u, 1u);
		}
		else if (p.use_solve_fold) {
			color_pc.color_count = p.color_launch_bound;
			rec.mark(m_solve_marks.sweep);
			rec.bind(m_compute.solve_sweep_pipeline);
			rec.push_bindings<solve_sweep_entry>(color_pc, bindings);
			rec.dispatch(p.sweep_workgroups, 1u, 1u);
		}
		else {
			for (std::uint32_t color = 0; color < p.color_launch_bound; ++color) {
				color_pc.color_offset = color;
				rec.mark(m_solve_marks.color[color]);
				rec.push_bindings<solve_color_entry>(color_pc, bindings);
				rec.dispatch_indirect(f.indirect_dispatch_buffer, (2 + color) * 3 * sizeof(std::uint32_t));
			}
		}

		if (island_lambda) {
			continue;
		}
		auto lambda_pc = p.push_constants(it, p.num_iterations, sub, it, p.solve_alpha);
		lambda_pc.fold_lambda = fold_lambda ? 1u : 0u;
		rec.mark(m_solve_marks.update_lambda);
		rec.bind(m_compute.update_lambda_pipeline);
		rec.push_bindings<update_lambda_entry>(lambda_pc, bindings);
		rec.dispatch_indirect(f.indirect_dispatch_buffer, 3 * sizeof(std::uint32_t));
		if (fold_lambda) {
			continue;
		}
		if (p.joint_count > 0) {
			rec.mark(m_solve_marks.joint_lambda);
			rec.bind(m_compute.update_joint_lambda_pipeline);
			rec.push_bindings<update_joint_lambda_entry>(p.push_constants(0u, 0u, sub, it, p.solve_alpha), bindings);
			rec.dispatch(p.joint_workgroups, 1, 1);
		}

		rec.mark(m_solve_marks.convergence);
		rec.bind(m_compute.convergence_check_pipeline);
		rec.push_bindings<convergence_check_entry>(p.push_constants(it, p.num_iterations, sub, it, p.solve_alpha), bindings);
		rec.dispatch(1, 1, 1);
	}
}

auto gse::vbd::gpu_solver::stage_derive_velocities(const solve_plan& p, const std::uint32_t sub, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_derive_velocities_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.derive_velocities_pipeline);

	rec.dispatch<derive_velocities_entry>(
		p.push_constants(0u, 0u, sub, p.num_iterations, p.solve_alpha),
		p.bindings,
		vec3u{ p.body_workgroups, 1u, 1u }
	);
}

auto gse::vbd::gpu_solver::stage_apply_restitution(const solve_plan& p, const std::uint32_t sub, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_apply_restitution_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.apply_restitution_pipeline);

	const std::uint32_t island_groups = (std::max(p.island_count, 1u) + limits.adjacency_workgroup_size - 1) / limits.adjacency_workgroup_size;
	rec.push_bindings<apply_restitution_serial_entry>(p.push_constants(0u, 0u, sub, p.num_iterations, 0.f), p.bindings);
	rec.dispatch(island_groups, 1, 1);
}

auto gse::vbd::gpu_solver::stage_prepare_color_indirect(const solve_plan& p, const std::uint32_t sub, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	{
		auto rec = co_await gpu::pass<vbd_prepare_color_indirect_stage>(pass_out)
			.on(gpu::queue_type::compute)
			.in_chain<vbd_solve_chain>(chain_index)
			.pipeline(m_compute.prepare_color_indirect_pipeline);

		rec.dispatch<prepare_color_indirect_entry>(
			p.push_constants(0u, 0u, sub, p.num_iterations, 0.f),
			p.bindings,
			vec3u{ 1u, 1u, 1u }
		);
	}
}

auto gse::vbd::gpu_solver::stage_post_stabilize(const solve_plan& p, const std::uint32_t sub, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto& f = *p.f;
	const auto& bindings = p.bindings;
	const auto& jointless_bindings = p.jointless_bindings;

	auto rec = co_await gpu::pass<vbd_post_stabilize_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.solve_color_pipeline);

	auto color_pc = p.push_constants(0u, limits.max_colors, sub, p.num_iterations, 0.f);

	rec.bind(m_compute.solve_color_pipeline);

	if (p.use_jacobi) {
		rec.push_bindings<solve_color_entry>(color_pc, bindings);
		rec.dispatch(p.body_workgroups, 1, 1);

		rec.bind(m_compute.apply_jacobi_pipeline);
		rec.push_bindings<apply_jacobi_entry>(p.push_constants(0u, 0u, sub, p.num_iterations, 0.f), bindings);
		rec.dispatch(p.body_workgroups, 1, 1);
	}
	else if (p.joint_count > 0) {
		if (p.jointless_body_count > 0) {
			if (p.use_solve_fold) {
				color_pc.color_count = p.color_launch_bound;
				rec.bind(m_compute.solve_sweep_pipeline);
				rec.push_bindings<solve_sweep_entry>(color_pc, jointless_bindings);
				rec.dispatch(p.sweep_workgroups, 1u, 1u);
				rec.bind(m_compute.solve_color_pipeline);
			}
			else {
				for (std::uint32_t color = 0; color < p.color_launch_bound; ++color) {
					color_pc.color_offset = color;
					rec.push_bindings<solve_color_entry>(color_pc, jointless_bindings);
					rec.dispatch_indirect(f.jointless_indirect_dispatch_buffer, (2 + color) * 3 * sizeof(std::uint32_t));
				}
			}
			color_pc.color_count = 0u;
		}
		color_pc.color_offset = 0xFFFFFFFFu;
		rec.bind(m_compute.solve_island_pipeline);
		rec.push_bindings<solve_island_entry>(color_pc, bindings);
		rec.dispatch(std::max(p.island_count, 1u), 1u, 1u);
	}
	else if (p.use_solve_fold) {
		color_pc.color_count = p.color_launch_bound;
		rec.bind(m_compute.solve_sweep_pipeline);
		rec.push_bindings<solve_sweep_entry>(color_pc, bindings);
		rec.dispatch(p.sweep_workgroups, 1u, 1u);
	}
	else {
		for (std::uint32_t color = 0; color < p.color_launch_bound; ++color) {
			color_pc.color_offset = color;
			rec.push_bindings<solve_color_entry>(color_pc, bindings);
			rec.dispatch_indirect(f.indirect_dispatch_buffer, (2 + color) * 3 * sizeof(std::uint32_t));
		}
	}
}

auto gse::vbd::gpu_solver::stage_finalize(const solve_plan& p, const std::uint32_t sub, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto rec = co_await gpu::pass<vbd_finalize_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.finalize_pipeline);

	rec.dispatch<finalize_entry>(
		p.push_constants(0u, 0u, sub, 0u, 0.f),
		p.bindings,
		vec3u{ p.body_workgroups, 1u, 1u }
	);
}

auto gse::vbd::gpu_solver::stage_update_sticking(const solve_plan& p, const std::uint32_t sub, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto& f = *p.f;

	auto rec = co_await gpu::pass<vbd_update_sticking_stage>(pass_out)
		.on(gpu::queue_type::compute)
		.in_chain<vbd_solve_chain>(chain_index)
		.pipeline(m_compute.update_sticking_pipeline);

	rec.push_bindings<update_sticking_entry>(p.push_constants(0u, 0u, sub, p.num_iterations, 0.f), p.bindings);
	rec.dispatch_indirect(f.indirect_dispatch_buffer, 3 * sizeof(std::uint32_t));
}

auto gse::vbd::gpu_solver::stage_state_copy(const solve_plan& p, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto& f = *p.f;
	auto& other = *p.other;

	auto rec = co_await gpu::pass<vbd_state_copy_stage>(pass_out).on(gpu::queue_type::compute).in_chain<vbd_solve_chain>(chain_index);
	if (p.joint_count > 0) {
		rec.copy_buffer(f.joint_buffer, other.joint_buffer, p.joint_count * sizeof(joint_constraint));
	}

	rec.copy_buffer(f.body_buffer, other.body_buffer, p.body_count * sizeof(body_state));
}

auto gse::vbd::gpu_solver::stage_publish(const solve_plan& p, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	auto& f = *p.f;

	auto rec = co_await gpu::pass<vbd_state_copy_stage>(pass_out).on(gpu::queue_type::compute).in_chain<vbd_solve_chain>(chain_index);
	++m_dispatch_generation;
	m_ticks_dispatched += m_ticks;
	m_generation_ticks[m_dispatch_generation % m_generation_ticks.size()] = m_ticks_dispatched;
	m_generation_end_tick[m_dispatch_generation % m_generation_end_tick.size()] = m_first_tick + m_ticks;
	const std::size_t grounded_copy_size = m_capacities.max_grounded_uints * sizeof(std::uint32_t);
	constexpr std::size_t collision_state_copy_size = (limits.collision_state_header_uints + limits.max_narrow_phase_debug_records * limits.narrow_phase_debug_record_uints) * sizeof(std::uint32_t);
	constexpr std::size_t adjacency_meta_copy_size = debug_adjacency_meta_count * sizeof(std::uint32_t);
	const std::size_t body_copy_size = p.body_count * sizeof(body_state);
	rec.copy_buffer(f.body_buffer, m_snapshot_channel.publish_target(m_dispatch_generation, body_copy_size), body_copy_size);
	rec.copy_buffer(f.grounded_buffer, m_grounded_channel.publish_target(m_dispatch_generation, grounded_copy_size), grounded_copy_size);
	rec.copy_buffer(f.collision_state_buffer, m_collision_state_channel.publish_target(m_dispatch_generation, collision_state_copy_size), collision_state_copy_size);
	rec.copy_buffer(f.contact_buffer, m_contact_dump_channel.publish_target(m_dispatch_generation, debug_contact_dump_count * sizeof(contact_constraint)), debug_contact_dump_count * sizeof(contact_constraint));
	const auto& adjacency_meta_target = m_adjacency_meta_channel.publish_target(m_dispatch_generation, adjacency_meta_copy_size * 2);
	rec.copy_buffer(f.contact_counts_buffer, adjacency_meta_target, adjacency_meta_copy_size);
	rec.copy_buffer(f.contact_offsets_buffer, adjacency_meta_target, adjacency_meta_copy_size, 0, adjacency_meta_copy_size);
	rec.copy_buffer(f.contact_adjacency_buffer, m_contact_adjacency_channel.publish_target(m_dispatch_generation, debug_adjacency_dump_count * sizeof(std::uint32_t)), debug_adjacency_dump_count * sizeof(std::uint32_t));

	m_recorded_ring = m_frame->current_frame();
	m_recorded_frame = m_frame->frame_count();
	m_pending_dispatch = false;
	m_body_buffers_seeded = true;
	m_seeded_body_count = m_body_count;
	m_dispatch_slot = 1 - m_dispatch_slot;
}

auto gse::vbd::gpu_solver::dispatch_compute(context& ctx, const channel_write<gpu::render_pass_request> pass_out) -> async::task<> {
	if (!m_buffers_created || m_body_count == 0) {
		co_return;
	}

	solve_plan p;
	build_solve_plan(p);

	std::uint32_t chain_seq = 0;
	std::vector<async::task<>> stages;
	stages.reserve(64);

	stages.push_back(stage_seed_state(p, chain_seq++, pass_out));

	if (p.diag_hashes) {
		stages.push_back(stage_hash_state(p, spare_hash_substep, 10u, chain_seq++, pass_out));
		stages.push_back(stage_hash_state_alt_body(p, spare_hash_substep, 13u, chain_seq++, pass_out));
		stages.push_back(stage_hash_bodies(p, spare_hash_substep, 14u, chain_seq++, pass_out));
	}

	if (m_restore_tick) {
		stages.push_back(stage_ring_restore(*p.f, *p.other, *m_restore_tick, chain_seq++, pass_out));
		m_restore_tick.reset();
	}

	stages.push_back(stage_apply_body_inputs(p, chain_seq++, pass_out));
	stages.push_back(stage_render_mirror(p, chain_seq++, pass_out));
	stages.push_back(stage_clear_state_buffers(p, chain_seq++, pass_out));
	if (p.merge_joint_inputs) {
		stages.push_back(stage_apply_joint_inputs(p, chain_seq++, pass_out));
	}
	if (p.apply_joint_drive_inputs) {
		stages.push_back(stage_apply_joint_drive_inputs(p, chain_seq++, pass_out));
	}

	const std::uint32_t substeps_per_tick = std::max(p.substeps / std::max(m_ticks, 1u), 1u);

	for (std::uint32_t sub = 0; sub < p.substeps; ++sub) {
		stages.push_back(stage_collision_reset(p, sub, chain_seq++, pass_out));

		if (p.diag_hashes && sub == 0) {
			stages.push_back(stage_hash_state(p, spare_hash_substep, 0u, chain_seq++, pass_out));
		}

		stages.push_back(stage_grid_build(p, sub, chain_seq++, pass_out));

		if (p.diag_hashes) {
			stages.push_back(stage_hash_state(p, sub, 10u, chain_seq++, pass_out));
		}

		if (p.diag_hashes && sub == 0) {
			stages.push_back(stage_hash_state(p, spare_hash_substep, 1u, chain_seq++, pass_out));
		}

		stages.push_back(stage_broad_phase(p, sub, chain_seq++, pass_out));

		if (p.diag_hashes) {
			stages.push_back(stage_hash_state(p, sub, 9u, chain_seq++, pass_out));
			stages.push_back(stage_hash_warm_inputs(p, sub, spare_hash_substep, 3u + sub, chain_seq++, pass_out));
		}

		stages.push_back(stage_prepare_indirect(p, sub, chain_seq++, pass_out));
		stages.push_back(stage_narrow_phase(p, sub, chain_seq++, pass_out));
		stages.push_back(stage_prepare_contact_indirect(p, sub, chain_seq++, pass_out));

		if (p.diag_hashes) {
			stages.push_back(stage_hash_state(p, sub, 1u, chain_seq++, pass_out));
		}

		stages.push_back(stage_build_adjacency(p, sub, chain_seq++, pass_out));

		if (p.diag_hashes && sub == 0) {
			stages.push_back(stage_hash_adjacency(p, spare_hash_substep, 5u, chain_seq++, pass_out));
			stages.push_back(stage_hash_adjacency(p, spare_hash_substep, 6u, chain_seq++, pass_out));
		}

		if (p.jointless_body_count > 0 || p.use_jacobi || p.diag_hashes) {
			stages.push_back(stage_build_coloring(p, sub, chain_seq++, pass_out));
		}

		if (p.diag_hashes) {
			stages.push_back(stage_hash_state(p, sub, 2u, chain_seq++, pass_out));
		}

		if (p.diag_hashes && sub == 0) {
			stages.push_back(stage_hash_colors(p, spare_hash_substep, 7u, chain_seq++, pass_out));
			stages.push_back(stage_hash_colors(p, spare_hash_substep, 8u, chain_seq++, pass_out));
			stages.push_back(stage_build_coloring(p, sub, chain_seq++, pass_out));
			stages.push_back(stage_hash_colors(p, spare_hash_substep, 9u, chain_seq++, pass_out));
		}

		if (const std::uint32_t tick = sub / substeps_per_tick; sub % substeps_per_tick == 0 && tick < p.impulse_counts.size() && p.impulse_counts[tick] > 0) {
			stages.push_back(stage_apply_impulses(p, sub, chain_seq++, pass_out));
		}

		stages.push_back(stage_predict(p, sub, chain_seq++, pass_out));

		if (p.diag_hashes) {
			stages.push_back(stage_hash_state(p, sub, 0u, chain_seq++, pass_out));
		}

		stages.push_back(stage_freeze_jacobians(p, sub, chain_seq++, pass_out));

		if (p.diag_hashes) {
			stages.push_back(stage_hash_state(p, sub, 3u, chain_seq++, pass_out));
		}

		stages.push_back(stage_solve_iterations(p, sub, chain_seq++, pass_out));

		if (p.diag_hashes) {
			stages.push_back(stage_hash_state(p, sub, 4u, chain_seq++, pass_out));
		}

		stages.push_back(stage_derive_velocities(p, sub, chain_seq++, pass_out));

		if (p.diag_hashes) {
			stages.push_back(stage_hash_state(p, sub, 5u, chain_seq++, pass_out));
		}

		stages.push_back(stage_apply_restitution(p, sub, chain_seq++, pass_out));

		if (p.diag_hashes) {
			stages.push_back(stage_hash_state(p, sub, 6u, chain_seq++, pass_out));
		}

		if (p.post_stabilize) {
			stages.push_back(stage_prepare_color_indirect(p, sub, chain_seq++, pass_out));
			stages.push_back(stage_post_stabilize(p, sub, chain_seq++, pass_out));
		}

		if (p.diag_hashes) {
			stages.push_back(stage_hash_state(p, sub, 7u, chain_seq++, pass_out));
		}

		stages.push_back(stage_finalize(p, sub, chain_seq++, pass_out));

		if (p.diag_hashes) {
			stages.push_back(stage_hash_state(p, sub, 8u, chain_seq++, pass_out));
		}

		stages.push_back(stage_update_sticking(p, sub, chain_seq++, pass_out));

		if (m_ring_history > 0 && (sub + 1) % substeps_per_tick == 0) {
			stages.push_back(stage_ring_copy(*p.f, m_first_tick + (sub + 1) / substeps_per_tick, chain_seq++, pass_out));
		}
	}

	if (p.diag_hashes) {
		stages.push_back(stage_hash_bodies(p, spare_hash_substep, 11u, chain_seq++, pass_out));
	}

	co_await async::when_all_inline(std::move(stages));

	co_await stage_state_copy(p, chain_seq++, pass_out);

	if (p.diag_hashes) {
		co_await stage_hash_bodies_other(p, spare_hash_substep, 12u, chain_seq++, pass_out);
	}

	co_await stage_publish(p, chain_seq++, pass_out);
}

auto gse::vbd::gpu_solver::compute_initialized() const -> bool {
	return m_compute.initialized;
}

auto gse::vbd::gpu_solver::buffers_created() const -> bool {
	return m_buffers_created;
}
