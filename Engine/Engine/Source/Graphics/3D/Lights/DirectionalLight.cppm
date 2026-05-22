export module gse.graphics:directional_light;

import std;

import gse.math;
import gse.core;
import gse.ecs;

export namespace gse {
	struct directional_light_component {
		vec3f color = { 1.0f, 1.0f, 1.0f };
		irradiance intensity = watts_per_square_meter(1.0f);
		vec3f direction = { 0.0f, -1.0f, 0.0f };
		float ambient_strength = 1.0f;
		length source_radius = meters(0.02f);
	};
}
