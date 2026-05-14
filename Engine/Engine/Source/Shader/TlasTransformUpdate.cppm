export module gse.shader:tlas_transform_update;

import std;

import gse.containers;
import gse.gpu;

export namespace gse::shaders::tlas_transform_update {
	struct [[= shader_struct]] push_constants {
		std::uint32_t count;
		std::uint32_t instance_stride;
		std::uint32_t model_matrix_offset;
	};

	struct [[= binding<0, 0>{}, = byte_address_buffer]] source_instance_data {};

	struct [[= binding<0, 1>{}, = ssbo_readonly]] index_mapping {
		using element = std::uint32_t;
	};

	struct [[= binding<0, 2>{}, = rw_byte_address_buffer]] tlas_instances {};

	using shader_binding_types = type_pack<
		source_instance_data,
		index_mapping,
		tlas_instances
	>;

	using entry = gpu::compute_entry<
		gpu::body_path<"Compute/tlas_transform_update">,
		gpu::layout<"tlas_transform_update">,
		gpu::bindings<shader_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;
}

static_assert(sizeof(gse::shaders::tlas_transform_update::push_constants) == gse::shaders::slang_scalar_size<gse::shaders::tlas_transform_update::push_constants>());
