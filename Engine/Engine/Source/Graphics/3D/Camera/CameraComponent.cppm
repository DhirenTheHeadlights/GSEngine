export module gse.graphics:camera_component;

import gse.math;

export namespace gse::camera {
	struct follow_component {
		vec3<length> offset{};
		int priority = 50;
		time blend_in_duration = milliseconds(300);
		bool active = true;
		bool use_entity_position = true;
		vec3<position> position{};
		quat orientation = identity<float>();
	};
}