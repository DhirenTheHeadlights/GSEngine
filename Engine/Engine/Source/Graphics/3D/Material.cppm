export module gse.graphics:material;

import std;

import :texture;

import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.os;
import gse.assets;
import gse.gpu;

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
		if (!h.id().exists()) {
			return true;
		}
		if (!h.valid()) {
			return false;
		}
		return h->upload_token().ready();
	};
	return check(diffuse_texture) && check(normal_texture) && check(specular_texture);
}
