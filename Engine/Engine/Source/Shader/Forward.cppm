export module gse.shader:forward;

import std;

import gse.containers;
import gse.gpu;
import gse.math;

import :common;

export namespace gse::shaders::forward {
	enum class [[= shader_enum]] light_type : std::uint32_t {
		directional = 0,
		point = 1,
		spot = 2,
	};

	struct [[= shader_struct]] material_data {
		vec3f base_color;
		float roughness;
		float metallic;
		float pad0;
		float pad1;
		float pad2;
	};

	struct [[= shader_struct]] light {
		light_type light_type;
		vec3f position;
		vec3f direction;
		vec3f world_position;
		vec3f world_direction;
		vec3f color;
		float intensity;
		float constant;
		float linear;
		float quadratic;
		float cut_off;
		float outer_cut_off;
		float ambient_strength;
		float source_radius;
	};

	struct [[= shader_struct]] vertex {
		vec3f position;
		vec3f normal;
		vec2f tex_coords;
	};

	struct [[= shader_struct]] meshlet_descriptor {
		std::uint32_t vertex_offset;
		std::uint32_t triangle_offset;
		std::uint32_t vertex_count;
		std::uint32_t triangle_count;
	};

	struct [[= shader_struct]] meshlet_bounds {
		vec3f center;
		float radius;
		vec3f cone_axis;
		float cone_cutoff;
	};

	struct [[= binding<0, 0>{}]] camera_ubo {
		using element = common::camera_data;
	};

	struct [[= binding<0, 1>{}, = ssbo_readonly]] lights_ssbo {
		using element = light;
	};

	struct [[= binding<0, 2>{}, = tlas]] scene_tlas {};

	struct [[= binding<0, 3>{}, = ssbo_readonly]] light_index_list {
		using element = std::uint32_t;
	};

	struct [[= binding<0, 4>{}, = ssbo_readonly]] tile_light_table {
		using element = vec2u;
	};

	struct [[= binding<0, 5>{}, = ssbo_readonly]] material_palette {
		using element = material_data;
	};

	struct [[= binding<1, 0>{}, = ssbo_readonly]] vertices_buffer {
		using element = vertex;
	};

	struct [[= binding<1, 1>{}, = ssbo_readonly]] meshlets_buffer {
		using element = meshlet_descriptor;
	};

	struct [[= binding<1, 2>{}, = ssbo_readonly]] meshlet_vertex_indices {
		using element = std::uint32_t;
	};

	struct [[= binding<1, 3>{}, = byte_address_buffer]] meshlet_triangles {};

	struct [[= binding<1, 4>{}, = ssbo_readonly]] meshlet_bounds_buffer {
		using element = meshlet_bounds;
	};

	struct [[= binding<1, 5>{}, = ssbo_readonly]] instance_data_buffer {
		using element = common::instance_data;
	};

	struct [[= binding<1, 6>{}, = sampler2d]] diffuse_sampler {};

	using shader_types = type_pack<light_type, material_data, light, vertex, meshlet_descriptor, meshlet_bounds>;
	using shader_binding_types = type_pack<
		camera_ubo,
		lights_ssbo,
		scene_tlas,
		light_index_list,
		tile_light_table,
		material_palette,
		vertices_buffer,
		meshlets_buffer,
		meshlet_vertex_indices,
		meshlet_triangles,
		meshlet_bounds_buffer,
		instance_data_buffer,
		diffuse_sampler
	>;
}

static_assert(sizeof(gse::shaders::forward::material_data) == gse::shaders::slang_scalar_size<gse::shaders::forward::material_data>());
static_assert(sizeof(gse::shaders::forward::light) == gse::shaders::slang_scalar_size<gse::shaders::forward::light>());
static_assert(sizeof(gse::shaders::forward::vertex) == gse::shaders::slang_scalar_size<gse::shaders::forward::vertex>());
static_assert(sizeof(gse::shaders::forward::meshlet_descriptor) == gse::shaders::slang_scalar_size<gse::shaders::forward::meshlet_descriptor>());
static_assert(sizeof(gse::shaders::forward::meshlet_bounds) == gse::shaders::slang_scalar_size<gse::shaders::forward::meshlet_bounds>());
