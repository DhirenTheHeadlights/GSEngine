export module gse.graphics:camera_data;

import gse.core;
import gse.math;

export namespace gse::camera {
	struct target {
		vec3<position> position{};
		quat orientation = identity<float>();
		angle fov = degrees(45.0f);
		length near_plane = meters(0.1f);
		length far_plane = meters(10000.0f);
	};

	struct request {
		id requester_id{};
		target target{};
		int priority = 0;
		time blend_duration = milliseconds(300);
		bool continuous = true;
	};
}