export module gse.shader:rgba_to_nv12;

import std;

import gse.containers;
import gse.gpu;
import gse.math;

export namespace gse::shaders::rgba_to_nv12 {
	struct [[= shader_struct]] push_constants {
		vec2u extent;
	};

	struct [[= binding<0, 0>{}, = sampler2d]] input_rgba {};

	struct [[= binding<0, 1>{}, = storage_image]] output_y {
		using element = float;
	};

	struct [[= binding<0, 2>{}, = storage_image]] output_uv {
		using element = vec2f;
	};

	using shader_binding_types = type_pack<
		input_rgba,
		output_y,
		output_uv
	>;

	using entry = gpu::compute_entry<
		gpu::body_path<"Compute/rgba_to_nv12">,
		gpu::layout<"rgba_to_nv12">,
		gpu::bindings<shader_binding_types>,
		gpu::threads<16, 16, 1>,
		gpu::push_constant<push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;
}

static_assert(sizeof(gse::shaders::rgba_to_nv12::push_constants) == gse::shaders::slang_scalar_size<gse::shaders::rgba_to_nv12::push_constants>());
