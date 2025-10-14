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

constexpr gse::time max_time_step = gse::seconds(0.25f);
gse::time accumulator;

auto gse::physics::update(const std::vector<std::reference_wrapper<registry>>& registries) -> void {
	time frame_time = system_clock::dt();
	std::println("dt: {}", frame_time);

	frame_time = std::min(frame_time, max_time_step);

	accumulator += frame_time;

	const auto const_update_time = system_clock::const_update_time;

	while (accumulator >= const_update_time) {
		for (int i = 0; i < 5; i++) {
			for (auto& registry : registries) {
				broad_phase_collision::update(
					registry,
					const_update_time
				);
			}
		}

		for (auto& registry : registries) {
			for (auto& object : registry.get().linked_objects_write<motion_component>()) {
				update_object(object, const_update_time, registry.get().try_linked_object_write<collision_component>(object.owner_id()));
			}
		}

		accumulator -= const_update_time;
	}
}