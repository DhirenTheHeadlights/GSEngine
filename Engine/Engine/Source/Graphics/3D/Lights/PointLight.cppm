export module gse.graphics:point_light;

import std;

import gse.math;
import gse.core;
import gse.ecs;

export namespace gse {
	struct point_light_component {
		vec3f color;
		irradiance intensity = watts_per_square_meter(1.0f);
		vec3<position> position;
		float constant = 1.0f;
		inverse_length linear = per_meter(0.09f);
		float quadratic = 0.032f;
		float ambient_strength = 0.025f;
		length source_radius = meters(0.5f);
	};
}
