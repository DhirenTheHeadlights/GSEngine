export module gse.graphics.material;

import std;

import gse.graphics.shader;
import gse.physics.math;

export namespace gse {
	struct mtl_material {
		unitless::vec3 ambient = unitless::vec3(1.0f);
		unitless::vec3 diffuse = unitless::vec3(1.0f);
		unitless::vec3 specular = unitless::vec3(1.0f);
		unitless::vec3 emission = unitless::vec3(0.0f);

		float shininess = 0.0f;
		float optical_density = 1.0f;
		float transparency = 1.0f;
		int illumination_model = 0;

		std::uint32_t diffuse_texture = 0;
		std::uint32_t normal_texture = 0;
		std::uint32_t specular_texture = 0;
	};
}