module;

#include <oneapi/tbb.h>

export module gse.physics:update;

import std;

import gse.physics.math;
import gse.utility;

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
	if (registries.empty()) {
		return;
	}
	time frame_time = system_clock::dt();

	// Clamp frame time to prevent spiraling on major stutters
	frame_time = std::min(frame_time, max_time_step);

	accumulator += frame_time;

	const auto const_update_time = system_clock::constant_update_time();

	// Process physics in fixed time steps
	while (accumulator >= const_update_time) {
		for (int i = 0; i < 5; i++) {
			for (auto& registry : registries) {
				broad_phase_collision::update(
					registry
				);
			}
		}

		// Gather together all components to be updated
		std::vector<std::tuple<
			std::reference_wrapper<motion_component>,
			std::reference_wrapper<collision_component>>
			> update_tasks;

		for (auto& reg_ref : registries) {
			auto& registry = reg_ref.get();
			for (auto& object : registry.linked_objects_write<motion_component>()) {
				update_tasks.emplace_back(
					object,
					registry.linked_object_write<collision_component>(object.owner_id())
				);
			}
		}

		// Use the index-based overload of parallel_for.
		task::parallel_for(std::size_t{ 0 }, update_tasks.size(), [&](const std::size_t i) {
			// Access the task data for the current index
			const auto& task_data = update_tasks[i];

			// Unpack the tuple for each task
			const auto& [object_ref, coll_comp_opt] = task_data;

			// Call the update function with the provided data
			update_object(object_ref.get(), &coll_comp_opt.get());
		});

		accumulator -= const_update_time;
	}
}
