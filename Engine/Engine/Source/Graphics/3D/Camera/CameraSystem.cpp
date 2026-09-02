module gse.graphics:camera_system_impl;

import std;

import :camera_system;
import :camera_data;
import :camera_component;

import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.os;
import gse.assets;
import gse.gpu;

auto gse::camera::position(const data& d) -> vec3<gse::position> {
	return d.current.position;
}

auto gse::camera::orientation(const data& d) -> quat {
	return d.current.orientation;
}

auto gse::camera::direction_relative_to_origin(const data& d, const vec3f& direction) -> vec3f {
	const auto u = d.current.orientation.imaginary_part();
	const auto t = 2.f * cross(u, direction);
	return direction + d.current.orientation.s() * t + cross(u, t);
}

auto gse::camera::interpolate_target(const target& from, const target& to, const float t) -> target {
	return {
		.position = lerp(from.position, to.position, t),
		.orientation = slerp(from.orientation, to.orientation, t),
		.fov = from.fov + (to.fov - from.fov) * t,
		.near_plane = from.near_plane + (to.near_plane - from.near_plane) * t,
		.far_plane = from.far_plane + (to.far_plane - from.far_plane) * t
	};
}

auto gse::camera::compute_view_matrix(const target& t) -> view_matrix {
	const auto rotation = mat4f(conjugate(t.orientation));
	const mat4f translation = translate(mat4f(1.0f), -(t.position - vec3<gse::position>{}));
	return spatial_matrix(rotation * translation);
}

auto gse::camera::compute_projection_matrix(const target& t, const vec2f& viewport) -> projection_matrix {
	if (viewport.x() <= 0.f || viewport.y() <= 0.f) {
		return perspective(t.fov, 1.f, t.near_plane, t.far_plane);
	}
	const float aspect_ratio = viewport.x() / viewport.y();
	return perspective(t.fov, aspect_ratio, t.near_plane, t.far_plane);
}

namespace gse::camera {
	auto halton(std::uint32_t base, std::uint32_t index) -> float {
		float f = 1.0f;
		float r = 0.0f;
		while (index > 0) {
			f /= static_cast<float>(base);
			r += f * static_cast<float>(index % base);
			index /= base;
		}
		return r;
	}

	auto apply_jitter(projection_matrix& proj, const vec2f& jitter_ndc) -> void {
		using elem_x = projection_matrix::element_t<2, 0>;
		using elem_y = projection_matrix::element_t<2, 1>;
		proj.set<2, 0>(proj.at<2, 0>() + elem_x(jitter_ndc.x()));
		proj.set<2, 1>(proj.at<2, 1>() + elem_y(jitter_ndc.y()));
	}
}

auto gse::camera::init(data& d) -> async::task<> {
	d.view_matrix = compute_view_matrix(d.current);
	d.projection_matrix = compute_projection_matrix(d.current, d.viewport);
	d.jitter_ndc = vec2f{ 0.f, 0.f };
	d.prev_jitter_ndc = d.jitter_ndc;
	d.prev_view_matrix = d.view_matrix;
	d.prev_projection_matrix = d.projection_matrix;
	return {};
}

auto gse::camera::scripted_requester_id() -> id {
	return find_or_generate_id("gse::camera::scripted");
}

auto gse::camera::run(context& ctx, data& d, const channel_read<ui_focus_request, viewport_update, camera_yaw_request, request, jitter_request> requests_in, read<follow_component> cameras) -> async::task<> {
	const time dt = system_clock::dt();

	for (const auto& [focus] : requests_in.of<ui_focus_request>()) {
		d.ui_focus = focus;
	}

	for (const auto& [size] : requests_in.of<viewport_update>()) {
		d.viewport = size;
	}

	for (const auto& req : requests_in.of<request>()) {
		const auto requester = req.requester_id.exists() ? req.requester_id : scripted_requester_id();
		const bool same_requester = d.scripted_active && requester == d.scripted_requester;
		if (d.scripted_active && !same_requester && req.priority < d.scripted_priority) {
			continue;
		}
		d.scripted_target = req.target;
		d.scripted_requester = requester;
		d.scripted_priority = req.priority;
		d.scripted_blend_duration = req.blend_duration;
		d.scripted_continuous = req.continuous;
		d.scripted_active = true;
	}

	int highest_priority = -1;
	id best_controller{};
	target best_target{};
	time best_blend_duration = milliseconds(300);

	const auto camera_ids = cameras.owner_ids();
	for (std::size_t i = 0; i < cameras.size(); ++i) {
		const auto& cam = cameras[i];
		if (!cam.active) {
			continue;
		}

		if (cam.priority > highest_priority) {
			highest_priority = cam.priority;
			best_controller = camera_ids[i];
			best_blend_duration = cam.blend_in_duration;

			best_target.position = cam.position + cam.offset;
			best_target.orientation = cam.orientation;
			best_target.fov = degrees(45.0f);
			best_target.near_plane = meters(0.1f);
			best_target.far_plane = meters(10000.0f);
		}
	}

	if (d.scripted_active && d.scripted_priority > highest_priority) {
		highest_priority = d.scripted_priority;
		best_controller = d.scripted_requester;
		best_blend_duration = d.scripted_blend_duration;
		best_target = d.scripted_target;
	}

	if (best_controller.exists() && best_controller != d.active_controller_entity) {
		if (d.active_controller_entity.exists() && best_blend_duration > time{}) {
			d.blend_from = d.current;
			d.blend_to = best_target;
			d.blend_duration = best_blend_duration;
			d.blend_elapsed = time{};
			d.blending = true;
		}
		else {
			d.current = best_target;
		}
		d.active_controller_entity = best_controller;
		d.active_priority = highest_priority;
	}
	else if (best_controller.exists()) {
		if (d.blending) {
			d.blend_to = best_target;
		}
		else {
			d.current = best_target;
		}
	}

	if (d.blending) {
		d.blend_elapsed += dt;
		const float t = std::clamp(d.blend_elapsed / d.blend_duration, 0.f, 1.f);
		d.current = interpolate_target(d.blend_from, d.blend_to, t);

		if (t >= 1.0f) {
			d.blending = false;
			d.current = d.blend_to;
		}
	}

	if (d.scripted_active && !d.scripted_continuous && !d.blending && d.active_controller_entity == d.scripted_requester) {
		d.scripted_active = false;
		d.scripted_priority = -1;
		d.active_controller_entity = {};
		d.active_priority = -1;
	}

	const vec3f forward = rotate_vector(d.current.orientation, vec3f(0.f, 0.f, -1.f));
	d.yaw = radians(std::atan2(-forward.x(), -forward.z()));
	for (const auto& req : requests_in.of<camera_yaw_request>()) {
		req.promise.fulfill(d.yaw);
	}

	d.prev_view_matrix = d.view_matrix;
	d.prev_projection_matrix = d.projection_matrix;
	d.prev_jitter_ndc = d.jitter_ndc;
	d.view_matrix = compute_view_matrix(d.current);
	d.projection_matrix = compute_projection_matrix(d.current, d.viewport);

	for (const auto& [enabled] : requests_in.of<jitter_request>()) {
		d.jitter_enabled = enabled;
	}

	if (!d.jitter_enabled) {
		d.jitter_ndc = vec2f{ 0.f, 0.f };
		d.jitter_index = 1;
		return {};
	}

	const float jitter_x = halton(2, d.jitter_index) - 0.5f;
	const float jitter_y = halton(3, d.jitter_index) - 0.5f;
	++d.jitter_index;
	if (d.jitter_index >= 64) {
		d.jitter_index = 1;
	}

	d.jitter_ndc = vec2f{
		d.viewport.x() > 0.f ? jitter_x * 2.0f / d.viewport.x() : 0.f,
		d.viewport.y() > 0.f ? jitter_y * 2.0f / d.viewport.y() : 0.f
	};
	apply_jitter(d.projection_matrix, d.jitter_ndc);

	return {};
}
