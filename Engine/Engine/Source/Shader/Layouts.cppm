export module gse.shader:layouts;

import std;

import gse.gpu;

import :forward;
import :post_process;
import :standard_2d;
import :standard_3d;
import :vbd_physics;

export namespace gse::shaders {
	auto initialize_layouts(
		gpu::shader_registry& registry
	) -> void;
}

auto gse::shaders::initialize_layouts(gpu::shader_registry& registry) -> void {
	registry.register_family("forward_3d", build_family_sets(forward::shader_binding_types{}));
	registry.register_family("post_process", build_family_sets(post_process::shader_binding_types{}));
	registry.register_family("standard_2d", build_family_sets(standard_2d::shader_binding_types{}));
	registry.register_family("standard_3d", build_family_sets(standard_3d::shader_binding_types{}));
	registry.register_family("vbd_physics", build_family_sets(vbd_physics::shader_binding_types{}));
}
