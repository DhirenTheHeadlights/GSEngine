export module gse.shader:light_culling;

import std;

import gse.containers;
import gse.gpu;
import gse.math;

import :forward;

export namespace gse::shaders::light_culling {
	struct [[= shader_struct]] culling_params_data {
		mat4f projection;
		mat4f inv_proj;
		vec2u screen_size;
		std::uint32_t num_lights;
	};

	struct [[= binding<0, 0>{}, = sampler2d]] g_depth {};

	struct [[= binding<0, 1>{}]] culling_params {
		using element = culling_params_data;
	};

	struct [[= binding<0, 2>{}, = ssbo_readonly]] lights {
		using element = forward::light;
	};

	struct [[= binding<0, 3>{}, = ssbo_readwrite]] light_index_list {
		using element = std::uint32_t;
	};

	struct [[= binding<0, 4>{}, = ssbo_readwrite]] tile_light_table {
		using element = vec2u;
	};

	using shader_binding_types = type_pack<
		g_depth,
		culling_params,
		lights,
		light_index_list,
		tile_light_table
	>;

	using shader_types = type_pack<culling_params_data>;

	using entry = gpu::compute_entry<
		gpu::body_path<"Compute/light_culling">,
		gpu::layout<"light_culling">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::threads<16, 16, 1>,
		gpu::system_values<gpu::group_id, gpu::group_thread_id, gpu::group_index>
	>;
}

static_assert(sizeof(gse::shaders::light_culling::culling_params_data) == gse::shaders::slang_scalar_size<gse::shaders::light_culling::culling_params_data>());
