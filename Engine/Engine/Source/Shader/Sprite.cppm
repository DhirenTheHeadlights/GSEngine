export module gse.shader:sprite;

import std;

import gse.containers;
import gse.gpu;
import gse.math;

import :standard_2d;

export namespace gse::shaders::sprite {
	struct [[= shader_struct]] push_constants {
		mat4f projection;
		std::uint32_t tex_idx;
	};

	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/sprite">,
		gpu::layout<"standard_2d">,
		gpu::bindings<standard_2d::shader_binding_types>,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::depth<false, false>,
		gpu::blend<gpu::blend_preset::alpha_premultiplied>,
		gpu::depth_target<gpu::depth_format::none>
	>;
}

static_assert(sizeof(gse::shaders::sprite::push_constants) == gse::shaders::slang_scalar_size<gse::shaders::sprite::push_constants>());
