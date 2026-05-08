export module gse.shader:forward;

import std;

import gse.containers;
import gse.gpu;
import gse.math;

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

	using shader_types = type_pack<light_type, material_data, light>;
}

static_assert(sizeof(gse::shaders::forward::material_data) == gse::shaders::slang_scalar_size<gse::shaders::forward::material_data>());
static_assert(sizeof(gse::shaders::forward::light) == gse::shaders::slang_scalar_size<gse::shaders::forward::light>());
