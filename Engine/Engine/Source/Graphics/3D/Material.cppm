export module gse.graphics:material;

import std;

import :texture;

import gse.math;
import gse.utility;
import gse.platform;

export namespace gse {
	struct material {
		vec3f base_color = vec3f(1.0f);
		float roughness = 0.5f;
		float metallic = 0.0f;

		resource::handle<texture> diffuse_texture;
		resource::handle<texture> normal_texture;
		resource::handle<texture> specular_texture;
	};
}
