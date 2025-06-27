export module gse.physics:update;

import std;

import gse.physics.math;

import :broad_phase_collision;
import :motion_component;
import :system;

export namespace gse::physics {
	auto update(
		std::span<motion_component> motion_components,
		std::span<collision_component> collision_components,
		std::span<render_component> render_components, 
		time fixed_update_time, 
		time frame_time
	) -> void;
}

constexpr gse::time g_max_time_step = gse::seconds(0.25);
gse::time g_accumulator;

auto gse::physics::update(const std::span<motion_component> motion_components, const std::span<collision_component> collision_components, const std::span<render_component> render_components, const time fixed_update_time, time frame_time) -> void {
	if (frame_time > g_max_time_step) {
		frame_time = g_max_time_step;
	}

	g_accumulator += frame_time;

	while (g_accumulator >= fixed_update_time) {
		for (int i = 0; i < 5; i++) {
			broad_phase_collision::update(collision_components, motion_components, fixed_update_time);
		}

		for (auto& object : motion_components) {
			collision_component* collision = nullptr;
			if (const auto collision_it = std::ranges::find_if(
				collision_components, 
				[object](const auto& c) {
					return c.parent_id == object.parent_id;
				}
			); collision_it != collision_components.end()) {
				collision = &*collision_it;
			}

			const render_component* render = nullptr;
			if (const auto render_it = std::ranges::find_if(
				render_components, 
				[object](const auto& r) {
					return r.parent_id == object.parent_id;
				}
			); render_it != render_components.end()) {
				render = &*render_it;
			}

			update_object(object, fixed_update_time, collision, render);
		}

		g_accumulator -= fixed_update_time;
	}
}


