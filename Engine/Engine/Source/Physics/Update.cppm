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

namespace gse::physics {
	constexpr time max_time_step = seconds(0.25f);
	time_t<float, seconds> accumulator;
}

auto gse::physics::update(const std::vector<std::reference_wrapper<registry>>& registries) -> void {
	if (registries.empty()) {
		return;
	}
	time frame_time = system_clock::dt();

	frame_time = std::min(frame_time, max_time_step);

	accumulator += frame_time;

	const auto const_update_time = system_clock::constant_update_time<time_t<float, seconds>>();

	while (accumulator >= const_update_time) {
		for (auto& registry : registries) {
			broad_phase_collision::update(
				registry
			);
		}

		std::vector<std::tuple<
			std::reference_wrapper<motion_component>,
			collision_component*
		>> update_tasks;

		for (auto& reg_ref : registries) {
			for (auto& registry = reg_ref.get(); auto& object : registry.linked_objects_write<motion_component>()) {
				update_tasks.emplace_back(
					object,
					registry.try_linked_object_write<collision_component>(object.owner_id())
				);
			}
		}

		task::parallel_for(std::size_t{ 0 }, update_tasks.size(), [&](const std::size_t i) {
			const auto& task_data = update_tasks[i];
			const auto& [object_ref, coll_comp_opt] = task_data;

			update_object(object_ref.get(), coll_comp_opt);
		});

		accumulator -= const_update_time;
	}
}
