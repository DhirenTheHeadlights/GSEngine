export module gse.graphics:camera_data;

import gse.math;
import gse.utility;

export namespace gse::camera {
	struct target {
		vec3<length> position{};
		quat orientation = identity<float>();
		float fov = 45.0f;
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

	struct state {
		target current{};
		target blend_from{};
		target blend_to{};
		time blend_elapsed{};
		time blend_duration{};
		bool blending = false;

		id active_controller_entity{};
		int active_priority = -1;

		angle yaw = degrees(-90.0f);
		angle pitch = degrees(0.0f);
		float mouse_sensitivity = 0.1f;

		mat4 view_matrix = mat4(1.0f);
		mat4 projection_matrix = mat4(1.0f);

		unitless::vec2 viewport{ 1920.f, 1080.f };
		bool ui_focus = false;

		auto position(
		) const -> vec3<length>;

		auto orientation(
		) const -> quat;

		auto direction_relative_to_origin(
			const unitless::vec3& direction
		) const -> unitless::vec3;
	};
}

auto gse::camera::state::position() const -> vec3<length> {
	return current.position;
}

auto gse::camera::state::orientation() const -> quat {
	return current.orientation;
}

auto gse::camera::state::direction_relative_to_origin(const unitless::vec3& direction) const -> unitless::vec3 {
	const auto u = current.orientation.imaginary_part();
	const auto t = 2.f * cross(u, direction);
	return direction + current.orientation.s() * t + cross(u, t);
}
