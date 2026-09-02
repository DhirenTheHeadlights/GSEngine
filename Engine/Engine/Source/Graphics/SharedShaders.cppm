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

	struct [[
		= binding<2, 1>{},
		= sampler_state
	]] textures_sampler {};

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
		vec4f tint;
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
		vec3<displacement> direction;
		vec3<gse::position> world_position;
		vec3<displacement> world_direction;
		vec3f color;
		irradiance intensity;
		float constant;
		inverse_length linear;
		inverse_area quadratic;
		float cut_off;
		float outer_cut_off;
		float ambient_strength;
		length source_radius;
		length radius;
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

	struct [[= shader_constant_block]] trace_quality_limits {
		std::int32_t max_shadow_samples = 4;
		std::int32_t max_ao_samples = 8;
		std::int32_t max_reflection_samples = 2;

		vec4<length> shadow_hard_distances = { meters(50.f), meters(40.f), meters(10.f), meters(30.f) };
		vec4i shadow_hard_samples = { 1, 1, 1, 1 };
		vec4<length> shadow_low_distances = { meters(100.f), meters(80.f), meters(20.f), meters(60.f) };
		vec4i shadow_low_samples = { 1, 1, 1, 1 };
		vec4<length> shadow_medium_distances = { meters(200.f), meters(150.f), meters(30.f), meters(100.f) };
		vec4i shadow_medium_samples = { 2, 2, 1, 1 };
		vec4<length> shadow_high_distances = { meters(500.f), meters(400.f), meters(50.f), meters(200.f) };
		vec4i shadow_high_samples = { 4, 4, 2, 1 };

		vec4<length> ao_low_distances = { meters(5.f), meters(4.f), meters(3.f), meters(4.f) };
		vec4i ao_low_samples = { 1, 1, 1, 1 };
		vec4<length> ao_medium_distances = { meters(10.f), meters(8.f), meters(5.f), meters(8.f) };
		vec4i ao_medium_samples = { 2, 2, 1, 1 };
		vec4<length> ao_high_distances = { meters(20.f), meters(15.f), meters(8.f), meters(15.f) };
		vec4i ao_high_samples = { 4, 4, 2, 1 };
		vec4<length> ao_ultra_distances = { meters(30.f), meters(25.f), meters(10.f), meters(20.f) };
		vec4i ao_ultra_samples = { 8, 8, 4, 2 };

		vec2<length> reflection_low_distances = { meters(100.f), meters(80.f) };
		std::int32_t reflection_low_samples = 1;
		float reflection_low_max_roughness = 0.1f;
		vec2<length> reflection_medium_distances = { meters(200.f), meters(150.f) };
		std::int32_t reflection_medium_samples = 1;
		float reflection_medium_max_roughness = 0.3f;
		vec2<length> reflection_high_distances = { meters(500.f), meters(400.f) };
		std::int32_t reflection_high_samples = 2;
		float reflection_high_max_roughness = 0.5f;

		vec4f env_brdf_fit_c0 = { -1.f, -0.0275f, -0.572f, 0.022f };
		vec4f env_brdf_fit_c1 = { 1.f, 0.0425f, 1.04f, -0.04f };
	};

	using shader_types = type_pack<light_type, material_data, light, vertex, meshlet_descriptor, meshlet_bounds, trace_quality_limits>;
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