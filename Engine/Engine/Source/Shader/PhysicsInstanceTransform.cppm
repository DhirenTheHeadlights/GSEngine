export module gse.shader:physics_instance_transform;

import std;

import gse.containers;
import gse.gpu;

import :common;
import :vbd_physics;

export namespace gse::shaders::physics_instance_transform {
	struct [[= shader_struct]] physics_mapping {
		std::uint32_t body_index;
		std::uint32_t instance_index;
		vec3f center_of_mass;
	};

	struct [[= shader_struct]] push_constants {
		std::uint32_t mapping_count;
		std::uint32_t body_count;
	};

	struct [[= binding<0, 0>{}, = ssbo_readonly]] body_data {
		using element = vbd_physics::gpu_body;
	};

	struct [[= binding<0, 1>{}, = ssbo_readonly]] mapping_data {
		using element = physics_mapping;
	};

	struct [[= binding<0, 2>{}, = ssbo_readwrite]] instance_data_buffer {
		using element = common::instance_data;
	};

	using shader_binding_types = type_pack<
		body_data,
		mapping_data,
		instance_data_buffer
	>;

	using shader_types = type_pack<physics_mapping>;

	using entry = gpu::compute_entry<
		gpu::body_path<"Compute/physics_instance_transform">,
		gpu::layout<"physics_instance_transform">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;
}

static_assert(sizeof(gse::shaders::physics_instance_transform::push_constants) == gse::shaders::slang_scalar_size<gse::shaders::physics_instance_transform::push_constants>());
static_assert(sizeof(gse::shaders::physics_instance_transform::physics_mapping) == gse::shaders::slang_scalar_size<gse::shaders::physics_instance_transform::physics_mapping>());
