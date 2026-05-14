export module gse.shader:physics_debug;

import std;

import gse.gpu;

import :common;
import :standard_3d;

export namespace gse::shaders::physics_debug {
	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/physics_debug">,
		gpu::layout<"standard_3d">,
		gpu::types<common::shader_types>,
		gpu::bindings<standard_3d::shader_binding_types>,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::rasterization<gpu::polygon_mode::line, gpu::cull_mode::none>,
		gpu::depth<false, false>,
		gpu::blend<gpu::blend_preset::alpha>,
		gpu::primitive_topology<gpu::topology::line_list>
	>;
}
