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

	struct primitive_cylinder_spec {
		[[= networked]] material_spec material;
		[[= networked]] length radius = meters(1.0f);
		[[= networked]] length height = meters(1.0f);
		bool resolved = false;
	};

	struct primitive_mountain_ring_spec {
		[[= networked]] material_spec material;
		[[= networked]] vec3f snow_color = vec3f(0.90f, 0.93f, 0.97f);
		[[= networked]] float snow_line = 0.70f;
		[[= networked]] float snow_slope = 0.90f;
		[[= networked]] float snow_roughness = 0.30f;
		[[= networked]] length inner_radius = meters(700.0f);
		[[= networked]] length outer_radius = meters(5000.0f);
		[[= networked]] length peak_height = meters(800.0f);
		[[= networked]] float ridge_base = 0.35f;
		[[= networked]] float massif_depth = 0.45f;
		[[= networked]] std::uint32_t massif_cells = 4;
		[[= networked]] std::uint32_t angular_segments = 384;
		[[= networked]] std::uint32_t radial_segments = 72;
		[[= networked]] std::uint32_t ridge_cells = 32;
		[[= networked]] std::uint32_t radial_cells = 8;
		[[= networked]] std::uint32_t octaves = 6;
		[[= networked]] std::uint32_t seed = 1337;
		bool resolved = false;
	};
}
