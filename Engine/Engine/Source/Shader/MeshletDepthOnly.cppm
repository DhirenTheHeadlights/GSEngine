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
}
