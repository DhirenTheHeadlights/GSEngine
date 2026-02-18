export module gse.runtime:camera_api;

import std;

import gse.utility;
import gse.graphics;
import gse.math;
import gse.platform;

import :core_api;

export namespace gse {
	auto camera_view() -> mat4 {
		if (!has_state<camera::state>()) {
			return mat4(1.f);
		}
		return state_of<camera::state>().view_matrix;
	}

	auto camera_projection() -> mat4 {
		if (!has_state<camera::state>()) {
			return mat4(1.f);
		}
		return state_of<camera::state>().projection_matrix;
	}

	auto camera_position() -> vec3<length> {
		if (!has_state<camera::state>()) {
			return vec3<length>(0.f, 0.f, 0.f);
		}
		return state_of<camera::state>().position();
	}

	auto camera_orientation() -> quat {
		if (!has_state<camera::state>()) {
			return quat(1.f, 0.f, 0.f, 0.f);
		}
		return state_of<camera::state>().orientation();
	}

	// Returns the camera's yaw angle in radians (rotation around Y axis)
	auto camera_yaw() -> float {
		const quat q = camera_orientation();
		// Extract yaw from quaternion (Y-up coordinate system)
		return std::atan2(2.f * (q.s() * q.y() + q.x() * q.z()),
		                  1.f - 2.f * (q.y() * q.y() + q.z() * q.z()));
	}

	// Rotate a direction vector by a yaw angle (radians) around Y axis
	auto direction_from_yaw(const unitless::vec3& direction, float yaw) -> unitless::vec3 {
		const float cos_yaw = std::cos(yaw);
		const float sin_yaw = std::sin(yaw);
		return unitless::vec3(
			direction.x() * cos_yaw + direction.z() * sin_yaw,
			direction.y(),
			-direction.x() * sin_yaw + direction.z() * cos_yaw
		);
	}

	auto camera_direction_relative_to_origin(
		const unitless::vec3& direction
	) -> unitless::vec3 {
		if (has_state<camera::state>()) {
			return state_of<camera::state>().direction_relative_to_origin(direction);
		}
		// Fallback for server: use camera_yaw from actions state (transmitted from client)
		if (has_state<actions::system_state>()) {
			const float yaw = state_of<actions::system_state>().current_state().camera_yaw();
			return direction_from_yaw(direction, yaw);
		}
		return direction;
	}

	auto active_camera_entity() -> id {
		if (!has_state<camera::state>()) {
			return {};
		}
		return state_of<camera::state>().active_controller_entity;
	}

	auto set_camera_viewport(
		const unitless::vec2& viewport
	) -> void {
		if (!has_state<camera::state>()) {
			return;
		}
		state_of<camera::state>().viewport = viewport;
	}
}
