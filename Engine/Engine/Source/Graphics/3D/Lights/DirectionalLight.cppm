export module gse.graphics:directional_light;

import std;

import gse.math;
import gse.core;
import gse.ecs;

export namespace gse {
	struct directional_light_component {
		vec3f color = { 1.0f, 1.0f, 1.0f };
		float intensity = 1.0f;
		vec3f direction = { 0.0f, -1.0f, 0.0f };
		float ambient_strength = 1.0f;
		float source_radius = 0.02f;

		id owner_id;
	};
}
