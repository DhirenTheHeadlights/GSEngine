export module gse.shader:meshlet_depth_only;

import std;

import gse.containers;
import gse.gpu;
import gse.math;

import :common;
import :forward;

export namespace gse::shaders::meshlet_depth_only {
	struct [[= binding<0, 0>{}]] camera_ubo {
		using element = common::camera_data;
	};

	struct [[= binding<1, 0>{}, = ssbo_readonly]] vertices_buffer {
		using element = forward::vertex;
	};

	struct [[= binding<1, 1>{}, = ssbo_readonly]] meshlets_buffer {
		using element = forward::meshlet_descriptor;
	};

	struct [[= binding<1, 2>{}, = ssbo_readonly]] meshlet_vertex_indices {
		using element = std::uint32_t;
	};

	struct [[= binding<1, 3>{}, = byte_address_buffer]] meshlet_triangles {};

	struct [[= binding<1, 4>{}, = ssbo_readonly]] meshlet_bounds_buffer {
		using element = forward::meshlet_bounds;
	};

	struct [[= binding<1, 5>{}, = ssbo_readonly]] instance_data_buffer {
		using element = common::instance_data;
	};

	using shader_binding_types = type_pack<
		camera_ubo,
		vertices_buffer,
		meshlets_buffer,
		meshlet_vertex_indices,
		meshlet_triangles,
		meshlet_bounds_buffer,
		instance_data_buffer
	>;

	struct [[= shader_struct]] push_constants {
		std::uint32_t meshlet_offset;
		std::uint32_t meshlet_count;
		std::uint32_t first_instance;
	};

	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/meshlet_depth_only">,
		gpu::layout<"meshlet_depth_only">,
		gpu::bindings<shader_binding_types>,
		gpu::amplification_stage<"as_main">,
		gpu::mesh_stage<"ms_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<push_constants>,
		gpu::depth<true, true, gpu::compare_op::less>,
		gpu::color_target<gpu::color_format::none>
	>;
}

static_assert(sizeof(gse::shaders::meshlet_depth_only::push_constants) == gse::shaders::slang_scalar_size<gse::shaders::meshlet_depth_only::push_constants>());
