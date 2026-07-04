export module gse.graphics:primitive_specs;

import std;

import gse.core;
import gse.ecs;
import gse.math;

export namespace gse {
	struct material_spec {
		vec3f base_color = vec3f(1.0f);
		float roughness = 0.5f;
		float metallic = 0.0f;
		float opacity = 1.0f;
		std::optional<std::string> diffuse_texture_name;
		std::optional<std::string> normal_texture_name;
		std::optional<std::string> specular_texture_name;
	};

	enum class sphere_lod : std::uint8_t {
		lo,
		mid,
		hi,
	};

	struct primitive_box_spec {
		[[= networked]] material_spec material;
		[[= networked]] vec3<length> size = vec3<length>(meters(1.0f));
		bool resolved = false;
	};

	struct primitive_sphere_spec {
		[[= networked]] material_spec material;
		[[= networked]] sphere_lod lod = sphere_lod::mid;
		[[= networked]] length radius = meters(1.0f);
		bool resolved = false;
	};
}
