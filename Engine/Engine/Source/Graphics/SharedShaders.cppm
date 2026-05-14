export module gse.graphics:shared_shaders;

import std;

import gse.containers;
import gse.gpu;
import gse.math;

export namespace gse::shaders::common {
	struct [[= shader_struct]] camera_data {
		mat4f view;
		mat4f proj;
		mat4f inv_view;
	};

	struct [[= shader_struct]] instance_data {
		mat4f model_matrix;
		mat4f normal_matrix;
		std::uint32_t skin_offset;
		std::uint32_t joint_count;
		std::uint32_t material_index;
		std::uint32_t pad0;
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
		float pad0;
		float pad1;
		float pad2;
	};

	struct [[= shader_struct]] light {
		light_type light_type;
		vec3<gse::position> position;
		vec3<gse::displacement> direction;
		vec3<gse::position> world_position;
		vec3<gse::displacement> world_direction;
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

	using shader_types = type_pack<light_type, material_data, light, vertex, meshlet_descriptor, meshlet_bounds>;
}

export namespace gse::shaders::standard_3d {
	struct [[= binding<0, 0>{}]] camera_ubo {
		using element = common::camera_data;
	};

	struct [[= binding<1, 2>{}, = sampler2d]] diffuse_sampler {};

	struct [[= binding<1, 3>{}, = ssbo_readonly]] skin_matrices {
		using element = mat4f;
	};

	struct [[= binding<1, 4>{}, = ssbo_readonly]] instance_data_buffer {
		using element = common::instance_data;
	};

	using shader_binding_types = type_pack<camera_ubo, diffuse_sampler, skin_matrices, instance_data_buffer>;
}

