export module gse.shader:layouts;

import std;

import gse.gpu;

import :forward;
import :standard_2d;
import :standard_3d;
import :vbd_physics;
import :tlas_transform_update;
import :physics_instance_transform;
import :skin_compute;
import :cull_instances;
import :light_culling;
import :rgba_to_nv12;
import :meshlet_depth_only;
import :skinned_depth_only;

export namespace gse::shaders {
	auto initialize_layouts(
		gpu::shader_registry& registry
	) -> void;
}

auto gse::shaders::initialize_layouts(gpu::shader_registry& registry) -> void {
	registry.register_family("forward_3d", build_family_sets(forward::shader_binding_types{}));
	registry.register_family("standard_2d", build_family_sets(standard_2d::shader_binding_types{}));
	registry.register_family("standard_3d", build_family_sets(standard_3d::shader_binding_types{}));
	registry.register_family("vbd_physics", build_family_sets(vbd_physics::shader_binding_types{}));
	registry.register_family("tlas_transform_update", build_family_sets(tlas_transform_update::shader_binding_types{}));
	registry.register_family("physics_instance_transform", build_family_sets(physics_instance_transform::shader_binding_types{}));
	registry.register_family("skin_compute", build_family_sets(skin_compute::shader_binding_types{}));
	registry.register_family("cull_instances", build_family_sets(cull_instances::shader_binding_types{}));
	registry.register_family("light_culling", build_family_sets(light_culling::shader_binding_types{}));
	registry.register_family("rgba_to_nv12", build_family_sets(rgba_to_nv12::shader_binding_types{}));
	registry.register_family("meshlet_depth_only", build_family_sets(meshlet_depth_only::shader_binding_types{}));
	registry.register_family("skinned_depth_only", build_family_sets(skinned_depth_only::shader_binding_types{}));
}
