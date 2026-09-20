export module gse.graphics:material;

import gse.assets;
import gse.core;
import gse.math;

import :texture;

export namespace gse {
	struct material {
		vec3f base_color = vec3f(1.0f);
		float roughness = 0.5f;
		float metallic = 0.0f;

		resource::handle<texture> diffuse_texture;
		resource::handle<texture> normal_texture;
		resource::handle<texture> specular_texture;

		[[nodiscard]] auto textures_ready() const -> bool;
	};
}

auto gse::material::textures_ready() const -> bool {
	const auto check = [](const resource::handle<texture>& h) {
		return !h.id().exists() || h.valid();
	};
	return check(diffuse_texture) && check(normal_texture) && check(specular_texture);
}