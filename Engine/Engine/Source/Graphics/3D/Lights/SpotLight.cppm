export module gse.graphics:spot_light;

import std;

import gse.math;
import gse.core;
import gse.ecs;
import :gui;

export namespace gse {
	struct spot_light_component {
		vec3f color;
		float intensity = 1.0f;
		vec3<position> position;
		vec3f direction;
		float constant = 1.0f;
		float linear = 0.09f;
		float quadratic = 0.032f;
		angle cut_off;
		angle outer_cut_off;
		float ambient_strength = 0.025f;
		float source_radius = 0.3f;

		id owner_id;
	};
}
