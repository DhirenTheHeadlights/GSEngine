export module gse.shader:cull_instances;

import std;

import gse.containers;
import gse.gpu;
import gse.math;

export namespace gse::shaders::cull_instances {
	struct [[= shader_struct]] frustum_data {
		vec4f planes[6];
	};

	struct [[= shader_struct]] batch_info {
		std::uint32_t first_instance;
		std::uint32_t instance_count;
		vec3<length> aabb_min;
		vec3<length> aabb_max;
	};

	struct [[= shader_struct]] push_constants {
		std::uint32_t batch_offset;
		std::uint32_t indirect_stride;
	};

	struct [[= binding<0, 0>{}]] frustum_ubo {
		using element = frustum_data;
	};

	struct [[= binding<0, 1>{}, = ssbo_readonly]] batches {
		using element = batch_info;
	};

	struct [[= binding<0, 2>{}, = rw_byte_address_buffer]] indirect_commands {};

	using shader_binding_types = type_pack<
		frustum_ubo,
		batches,
		indirect_commands
	>;

	using shader_types = type_pack<frustum_data, batch_info>;

	using entry = gpu::compute_entry<
		gpu::body_path<"Compute/cull_instances">,
		gpu::layout<"cull_instances">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::threads<1>,
		gpu::push_constant<push_constants>,
		gpu::system_values<gpu::group_id>
	>;
}

static_assert(sizeof(gse::shaders::cull_instances::push_constants) == gse::shaders::slang_scalar_size<gse::shaders::cull_instances::push_constants>());
static_assert(sizeof(gse::shaders::cull_instances::batch_info) == gse::shaders::slang_scalar_size<gse::shaders::cull_instances::batch_info>());
