export module gse.graphics:shared_shaders;

import std;

import gse.containers;
import gse.gpu;
import gse.math;

export namespace gse::shaders::bindless {
	struct [[
		= binding<2, 0>{},
		= sampler2d_array
	]] textures {};

	constexpr std::uint32_t invalid_index = std::numeric_limits<std::uint32_t>::max();
}

export namespace gse::shaders::common {
	struct [[= shader_struct]] camera_data {
		view_matrix view;
		projection_matrix proj;
		view_matrix inv_view;
		inverse_projection_matrix inv_view_proj;
		view_matrix prev_view;
		projection_matrix prev_proj;
		vec2f jitter_ndc;
		vec2f prev_jitter_ndc;
	};

	struct [[= shader_struct]] instance_data {
		spatial_matrix model_matrix;
		spatial_matrix normal_matrix;
		spatial_matrix prev_model_matrix;
		std::uint32_t material_index;
		vec3f tint;
	};

	using shader_types = type_pack<camera_data, instance_data>;
}

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
		std::uint32_t diffuse_index;
	};

	struct [[= shader_struct]] light {
		light_type light_type;
		vec3<gse::position> position;
		vec3<gse::displacement> direction;
		vec3<gse::position> world_position;
		vec3<gse::displacement> world_direction;
		vec3f color;
		irradiance intensity;
		float constant;
		inverse_length linear;
		float quadratic;
		float cut_off;
		float outer_cut_off;
		float ambient_strength;
		length source_radius;
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

	using shader_types = type_pack<light_type, material_data, light, vertex, meshlet_descriptor, meshlet_bounds>;
}

export namespace gse::shaders::meshlet {
	struct [[
		= binding<1, 0>{},
		= ssbo_readonly
	]] vertices_buffer {
		using element = forward::vertex;
	};

	struct [[
		= binding<1, 1>{},
		= ssbo_readonly
	]] meshlets_buffer {
		using element = forward::meshlet_descriptor;
	};

	struct [[
		= binding<1, 2>{},
		= ssbo_readonly
	]] meshlet_vertex_indices {
		using element = std::uint32_t;
	};

	struct [[
		= binding<1, 3>{},
		= byte_address_buffer
	]] meshlet_triangles {};

	struct [[
		= binding<1, 4>{},
		= ssbo_readonly
	]] meshlet_bounds_buffer {
		using element = forward::meshlet_bounds;
	};
}

export namespace gse::shaders::standard_3d {
	struct [[= binding<0, 0>{}]] camera_ubo {
		using element = common::camera_data;
	};

	struct [[
		= binding<1, 4>{},
		= ssbo_readonly
	]] instance_data_buffer {
		using element = common::instance_data;
	};

	using shader_binding_types = type_pack<camera_ubo, instance_data_buffer, bindless::textures>;
}
