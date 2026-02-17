export module gse.graphics:camera_system;

import std;

import gse.math;
import gse.utility;
import gse.platform;

import :camera_data;
import :camera_component;

export namespace gse::camera {
	struct ui_focus_update {
		bool focus = false;
	};

	struct viewport_update {
		unitless::vec2 size{ 1920.f, 1080.f };
	};
}

namespace gse::camera {
	auto interpolate_target(
		const target& from,
		const target& to,
		const float t
	) -> target {
		return {
			.position = lerp(from.position, to.position, t),
			.orientation = slerp(from.orientation, to.orientation, t),
			.fov = from.fov + (to.fov - from.fov) * t,
			.near_plane = from.near_plane + (to.near_plane - from.near_plane) * t,
			.far_plane = from.far_plane + (to.far_plane - from.far_plane) * t
		};
	}

	auto compute_view_matrix(
		const target& t
	) -> mat4 {
		const auto rotation = mat4(conjugate(t.orientation));
		const mat4 translation = translate(mat4(1.0f), -t.position);
		return rotation * translation;
	}

	auto compute_projection_matrix(
		const target& t,
		const unitless::vec2& viewport
	) -> mat4 {
		const float aspect_ratio = viewport.x() / viewport.y();
		return perspective(degrees(t.fov), aspect_ratio, t.near_plane, t.far_plane);
	}
}

export namespace gse::camera {
	struct system {
		static auto initialize(initialize_phase& phase, state& s) -> void;
		static auto update(update_phase& phase, state& s) -> void;
	};
}

auto gse::camera::system::initialize(initialize_phase&, state& s) -> void {
	s.current.orientation = identity<float>();
	s.view_matrix = compute_view_matrix(s.current);
	s.projection_matrix = compute_projection_matrix(s.current, s.viewport);
}

auto gse::camera::system::update(update_phase& phase, state& s) -> void {
	const time dt = system_clock::dt();

	for (const auto& [focus] : phase.read_channel<ui_focus_update>()) {
		s.ui_focus = focus;
	}

	for (const auto& [size] : phase.read_channel<viewport_update>()) {
		s.viewport = size;
	}

	if (!s.ui_focus) {
		if (const auto* input_state = phase.try_state_of<input::system_state>()) {
			const auto delta = input_state->current_state().mouse_delta();
			const auto transformed_offset = delta * s.mouse_sensitivity;
			s.yaw -= degrees(transformed_offset.x());
			s.pitch -= degrees(transformed_offset.y());
			s.pitch = std::clamp(s.pitch, degrees(-89.0f), degrees(89.0f));
		}
	}

	const quat yaw_rotation = from_axis_angle({ 0.f, 1.f, 0.f }, s.yaw);
	const quat pitch_rotation = from_axis_angle({ 1.f, 0.f, 0.f }, s.pitch);
	const quat new_orientation = normalize(yaw_rotation * pitch_rotation);

	phase.schedule([&s, new_orientation, dt](chunk<follow_component> cameras) {
		int highest_priority = -1;
		id best_controller{};
		target best_target{};
		time best_blend_duration = milliseconds(300);

		for (const auto& cam : cameras) {
			if (!cam.active) {
				continue;
			}

			if (cam.priority > highest_priority) {
				highest_priority = cam.priority;
				best_controller = cam.owner_id();
				best_blend_duration = cam.blend_in_duration;

				best_target.position = cam.position + cam.offset;
				best_target.orientation = new_orientation;
				best_target.fov = 45.0f;
				best_target.near_plane = meters(0.1f);
				best_target.far_plane = meters(10000.0f);
			}
		}

		if (best_controller.exists() && best_controller != s.active_controller_entity) {
			if (s.active_controller_entity.exists()) {
				s.blend_from = s.current;
				s.blend_to = best_target;
				s.blend_duration = best_blend_duration;
				s.blend_elapsed = time{};
				s.blending = true;
			} else {
				s.current = best_target;
			}
			s.active_controller_entity = best_controller;
			s.active_priority = highest_priority;
		} else if (best_controller.exists()) {
			if (s.blending) {
				s.blend_to = best_target;
			} else {
				s.current = best_target;
			}
		}

		if (s.blending) {
			s.blend_elapsed += dt;
			const float t = std::clamp(s.blend_elapsed / s.blend_duration, 0.f, 1.f);
			s.current = interpolate_target(s.blend_from, s.blend_to, t);

			if (t >= 1.0f) {
				s.blending = false;
				s.current = s.blend_to;
			}
		}

		s.current.orientation = new_orientation;

		s.view_matrix = compute_view_matrix(s.current);
		s.projection_matrix = compute_projection_matrix(s.current, s.viewport);
	});
}
