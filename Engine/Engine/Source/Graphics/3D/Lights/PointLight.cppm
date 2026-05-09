export module gse.graphics:point_light;

import std;

import gse.math;
import gse.core;
import gse.ecs;

export namespace gse {
	struct point_light_component {
		vec3f color;
		float intensity = 1.0f;
		vec3<position> position;
		float constant = 1.0f;
		float linear = 0.09f;
		float quadratic = 0.032f;
		float ambient_strength = 0.025f;
		float source_radius = 0.5f;

		id owner_id;
	};
}
