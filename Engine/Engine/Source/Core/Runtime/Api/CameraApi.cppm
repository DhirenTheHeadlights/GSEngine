export module gse.runtime:camera_api;

import std;

import gse.utility;
import gse.graphics;
import gse.physics.math;

import :core_api;

export namespace gse {
	auto camera_view() -> mat4 {
		return state_of<camera::state>().view_matrix;
	}

	auto camera_projection() -> mat4 {
		return state_of<camera::state>().projection_matrix;
	}

	auto camera_position() -> vec3<length> {
		return state_of<camera::state>().position();
	}

	auto camera_orientation() -> quat {
		return state_of<camera::state>().orientation();
	}

	auto camera_direction_relative_to_origin(
		const unitless::vec3& direction
	) -> unitless::vec3 {
		return state_of<camera::state>().direction_relative_to_origin(direction);
	}

	auto active_camera_entity() -> id {
		return state_of<camera::state>().active_controller_entity;
	}

	auto set_camera_viewport(
		const unitless::vec2& viewport
	) -> void {
		state_of<camera::state>().viewport = viewport;
	}
}
