export module gse.physics:update;

import std;

import gse.physics.math;

import :broad_phase_collision;
import :motion_component;
import :system;

export namespace gse::physics {
	auto update(
		const std::vector<std::reference_wrapper<registry>>& registries
	) -> void;
}

constexpr gse::time g_max_time_step = gse::seconds(0.25f);
gse::time g_accumulator;

auto gse::physics::update(const std::vector<std::reference_wrapper<registry>>& registries) -> void {
	time frame_time = system_clock::dt();

	if (frame_time > g_max_time_step) {
		frame_time = g_max_time_step;
	}

	g_accumulator += frame_time;

	while (g_accumulator >= system_clock::const_update_time) {
		for (int i = 0; i < 5; i++) {
			for (auto& registry : registries) {
				broad_phase_collision::update(
					registry,
					system_clock::const_update_time
				);
			}
		}

		for (auto& registry : registries) {
			for (auto& object : registry.get().linked_objects_write<motion_component>()) {
				update_object(object, system_clock::const_update_time, registry.get().try_linked_object_write<collision_component>(object.owner_id()));
			}
		}

		g_accumulator -= system_clock::const_update_time;
	}
}


