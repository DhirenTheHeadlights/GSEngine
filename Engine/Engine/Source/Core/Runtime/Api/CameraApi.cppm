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

	auto camera_yaw() -> angle {
		const quat q = camera_orientation();
		return radians(std::atan2(2.f * (q.s() * q.y() + q.x() * q.z()), 1.f - 2.f * (q.y() * q.y() + q.z() * q.z())));
	}

	auto direction_from_angle(
		const unitless::vec3& direction, 
		const angle yaw
	) -> unitless::vec3 {
		const float r = yaw.as<radians>();
		const float c = std::cos(r);
		const float s = std::sin(r);
		return unitless::vec3(
			direction.x() * c + direction.z() * s,
			direction.y(),
			-direction.x() * s + direction.z() * c
		);
	}

	auto camera_direction_relative_to_origin(
		const unitless::vec3& direction,
		const id entity_id
	) -> unitless::vec3 {
		if (has_state<camera::state>()) {
			return state_of<camera::state>().direction_relative_to_origin(direction);
		}
		if (const auto entity_yaw = actions::entity_camera_yaw(entity_id)) {
			return direction_from_angle(direction, *entity_yaw);
		}
		if (const auto context_yaw = actions::context_camera_yaw()) {
			return direction_from_angle(direction, *context_yaw);
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
		channel_add(camera::viewport_update{ 
			.size = viewport
		});
	}
}
