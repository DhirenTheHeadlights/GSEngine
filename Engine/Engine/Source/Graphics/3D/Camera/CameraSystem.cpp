module gse.graphics;

import std;

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

import :camera_data;
import :camera_component;
import :camera_system;

auto gse::camera::system::position(const data& d) -> vec3<gse::position> {
	return d.current.position;
}

auto gse::camera::system::orientation(const data& d) -> quat {
	return d.current.orientation;
}

auto gse::camera::system::direction_relative_to_origin(const data& d, const vec3f& direction) -> vec3f {
	const auto u = d.current.orientation.imaginary_part();
	const auto t = 2.f * cross(u, direction);
	return direction + d.current.orientation.s() * t + cross(u, t);
}

auto gse::camera::system::interpolate_target(const target& from, const target& to, const float t) -> target {
	return { .position = lerp(from.position, to.position, t),
			 .orientation = slerp(from.orientation, to.orientation, t),
			 .fov = from.fov + (to.fov - from.fov) * t,
			 .near_plane = from.near_plane + (to.near_plane - from.near_plane) * t,
			 .far_plane = from.far_plane + (to.far_plane - from.far_plane) * t };
}

auto gse::camera::system::compute_view_matrix(const target& t) -> view_matrix {
	const auto rotation = mat4f(conjugate(t.orientation));
	const mat4f translation = translate(mat4f(1.0f), -(t.position - vec3<gse::position>{}));
	return spatial_matrix(rotation * translation);
}

auto gse::camera::system::compute_projection_matrix(const target& t, const vec2f& viewport) -> projection_matrix {
	if (viewport.x() <= 0.f || viewport.y() <= 0.f) {
		return perspective(t.fov, 1.f, t.near_plane, t.far_plane);
	}
	const float aspect_ratio = viewport.x() / viewport.y();
	return perspective(t.fov, aspect_ratio, t.near_plane, t.far_plane);
}

auto gse::camera::system::run(run_context& ctx, data& d) -> async::task<> {
	d.view_matrix = compute_view_matrix(d.current);
	d.projection_matrix = compute_projection_matrix(d.current, d.viewport);

	while (true) {
		const time dt = system_clock::dt();

		for (const auto& [focus] : ctx.read_channel<ui_focus_request>()) {
			d.ui_focus = focus;
		}

		for (const auto& [size] : ctx.read_channel<viewport_update>()) {
			d.viewport = size;
		}

		int highest_priority = -1;
		id best_controller{};
		target best_target{};
		time best_blend_duration = milliseconds(300);

		{
			auto [cameras] = co_await ctx.acquire<read<follow_component>>();

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
		}

		if (best_controller.exists() && best_controller != d.active_controller_entity) {
			if (d.active_controller_entity.exists()) {
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

		const vec3f forward = rotate_vector(d.current.orientation, vec3f(0.f, 0.f, -1.f));
		d.yaw = radians(std::atan2(-forward.x(), -forward.z()));

		d.view_matrix = compute_view_matrix(d.current);
		d.projection_matrix = compute_projection_matrix(d.current, d.viewport);

		co_await ctx.next_tick();
	}
}
