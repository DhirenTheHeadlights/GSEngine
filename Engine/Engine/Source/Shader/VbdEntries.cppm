export module gse.shader:vbd_entries;

import std;

import gse.gpu;

import :vbd_physics;

export namespace gse::shaders::vbd {
	using predict = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_predict">,
		gpu::layout<"vbd_physics">,
		gpu::bindings<vbd_physics::shader_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<vbd_physics::vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using solve_color = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_solve_color">,
		gpu::layout<"vbd_physics">,
		gpu::bindings<vbd_physics::shader_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<vbd_physics::vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using update_lambda = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_update_lambda">,
		gpu::layout<"vbd_physics">,
		gpu::bindings<vbd_physics::shader_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<vbd_physics::vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using derive_velocities = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_derive_velocities">,
		gpu::layout<"vbd_physics">,
		gpu::bindings<vbd_physics::shader_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<vbd_physics::vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using finalize = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_finalize">,
		gpu::layout<"vbd_physics">,
		gpu::bindings<vbd_physics::shader_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<vbd_physics::vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using collision_reset = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/collision_reset">,
		gpu::layout<"vbd_physics">,
		gpu::bindings<vbd_physics::shader_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<vbd_physics::vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using collision_grid_build = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/collision_grid_build">,
		gpu::layout<"vbd_physics">,
		gpu::bindings<vbd_physics::shader_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<vbd_physics::vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using collision_broad_phase = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/collision_broad_phase">,
		gpu::layout<"vbd_physics">,
		gpu::bindings<vbd_physics::shader_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<vbd_physics::vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using collision_narrow_phase = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/collision_narrow_phase">,
		gpu::layout<"vbd_physics">,
		gpu::bindings<vbd_physics::shader_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<vbd_physics::vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using collision_build_adjacency = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/collision_build_adjacency">,
		gpu::layout<"vbd_physics">,
		gpu::bindings<vbd_physics::shader_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<vbd_physics::vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id, gpu::group_thread_id>
	>;

	using update_joint_lambda = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_update_joint_lambda">,
		gpu::layout<"vbd_physics">,
		gpu::bindings<vbd_physics::shader_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<vbd_physics::vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using prepare_indirect = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_prepare_indirect">,
		gpu::layout<"vbd_physics">,
		gpu::bindings<vbd_physics::shader_binding_types>,
		gpu::threads<1>,
		gpu::push_constant<vbd_physics::vbd_push_constants>
	>;

	using prepare_contact_indirect = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_prepare_contact_indirect">,
		gpu::layout<"vbd_physics">,
		gpu::bindings<vbd_physics::shader_binding_types>,
		gpu::threads<1>,
		gpu::push_constant<vbd_physics::vbd_push_constants>
	>;

	using prepare_color_indirect = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_prepare_color_indirect">,
		gpu::layout<"vbd_physics">,
		gpu::bindings<vbd_physics::shader_binding_types>,
		gpu::threads<16>,
		gpu::push_constant<vbd_physics::vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using freeze_jacobians = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_freeze_jacobians">,
		gpu::layout<"vbd_physics">,
		gpu::bindings<vbd_physics::shader_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<vbd_physics::vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using apply_jacobi = gpu::compute_entry<
		gpu::body_path<"VBDPhysics/vbd_apply_jacobi">,
		gpu::layout<"vbd_physics">,
		gpu::bindings<vbd_physics::shader_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<vbd_physics::vbd_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;
}
