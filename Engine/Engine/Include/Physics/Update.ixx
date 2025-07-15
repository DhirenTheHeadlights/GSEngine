export module gse.physics:update;

import std;

import gse.physics.math;

import :broad_phase_collision;
import :motion_component;
import :system;

export namespace gse::physics {
	auto update(
		std::span<std::reference_wrapper<registry>> registries,
		time fixed_update_time, 
		time frame_time
	) -> void;
}

constexpr gse::time g_max_time_step = gse::seconds(0.25);
gse::time g_accumulator;

auto gse::physics::update(const std::span<std::reference_wrapper<registry>> registries, const time fixed_update_time, time frame_time) -> void {
	if (frame_time > g_max_time_step) {
		frame_time = g_max_time_step;
	}

	g_accumulator += frame_time;

	while (g_accumulator >= fixed_update_time) {
		for (int i = 0; i < 5; i++) {
			for (auto& registry : registries) {
				broad_phase_collision::update(
					registry.get().linked_objects<collision_component>(), 
					registry.get().linked_objects<motion_component>(), 
					fixed_update_time
				);
			}
		}

		for (auto& registry : registries) {
			for (auto& object : registry.get().linked_objects<motion_component>()) {
				collision_component* collision = nullptr;
				if (const auto collision_it = std::ranges::find_if(
					registry.get().linked_objects<collision_component>(),
					[object](const auto& c) {
						return c.owner_id == object.owner_id;
					}
				); collision_it != registry.get().linked_objects<collision_component>().end()) {
					collision = &*collision_it;
				}

				const render_component* render = nullptr;
				if (const auto render_it = std::ranges::find_if(
					registry.get().linked_objects<render_component>(),
					[object](const auto& r) {
						return r.owner_id == object.owner_id;
					}
				); render_it != registry.get().linked_objects<render_component>().end()) {
					render = &*render_it;
				}

				update_object(object, fixed_update_time, collision, render);
			}
		}

		g_accumulator -= fixed_update_time;
	}
}


