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

	struct surface_spec {
		vec3f base_color = vec3f(1.0f);
		float roughness = 0.5f;
		float metallic = 0.0f;
	};

	struct primitive_hull_spec {
		[[= networked]] surface_spec surface;
		[[= networked]] float opacity = 1.0f;
		bool resolved = false;
	};

	struct mountain_ring_shape {
		surface_spec surface;
		vec3f snow_color = vec3f(0.90f, 0.93f, 0.97f);
		float snow_line = 0.70f;
		float snow_slope = 0.90f;
		float snow_roughness = 0.30f;
		length inner_radius = meters(700.0f);
		length outer_radius = meters(5000.0f);
		length peak_height = meters(800.0f);
		float ridge_base = 0.35f;
		float massif_depth = 0.45f;
		std::uint32_t massif_cells = 4;
		std::uint32_t angular_segments = 384;
		std::uint32_t radial_segments = 72;
		std::uint32_t ridge_cells = 32;
		std::uint32_t radial_cells = 8;
		std::uint32_t octaves = 6;
		std::uint32_t seed = 1337;
	};

	struct primitive_mountain_ring_spec {
		[[= networked]] mountain_ring_shape shape;
		[[= networked]] float opacity = 1.0f;
		bool resolved = false;
	};
}
