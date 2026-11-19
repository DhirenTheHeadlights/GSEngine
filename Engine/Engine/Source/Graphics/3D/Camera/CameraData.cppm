export module gse.graphics:camera_data;

import gse.core;
import gse.math;
import std;

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
		time blend_duration = milliseconds(300.f);
		bool continuous = true;
	};

	enum class latch_model : std::uint32_t {
		none = 0,
		eye = 1,
		orbit = 2,
	};

	struct latch {
		latch_model model = latch_model::none;
		angle yaw = degrees(0.f);
		angle pitch = degrees(0.f);
		angle min_pitch = degrees(-89.f);
		angle max_pitch = degrees(89.f);
		vec2<angle> per_count{};
		vec3<position> pivot{};
		length distance{};
	};
}