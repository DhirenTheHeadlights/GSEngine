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

//auto gse::physics::update(const std::vector<std::reference_wrapper<registry>>& registries) -> void {
//	time frame_time = system_clock::dt();
//	std::println("dt: {}", frame_time);
//
//	frame_time = std::min(frame_time, max_time_step);
//
//	accumulator += frame_time;
//
//	const auto const_update_time = system_clock::const_update_time;
//
//	while (accumulator >= const_update_time) {
//		for (int i = 0; i < 5; i++) {
//			for (auto& registry : registries) {
//				broad_phase_collision::update(
//					registry,
//					const_update_time
//				);
//			}
//		}
//
//		for (auto& registry : registries) {
//			for (auto& object : registry.get().linked_objects_write<motion_component>()) {
//				update_object(object, const_update_time, registry.get().try_linked_object_write<collision_component>(object.owner_id()));
//			}
//		}
//
//		accumulator -= const_update_time;
//	}
//}



//auto gse::physics::update(const std::vector<std::reference_wrapper<registry>>& registries) -> void {
//	if (registries.empty()) {
//		return;
//	}
//	time frame_time = system_clock::dt();
//
//	// Clamp frame time to prevent spiraling on major stutters
//	frame_time = std::min(frame_time, max_time_step);
//
//	accumulator += frame_time;
//
//	const auto const_update_time = system_clock::const_update_time;
//
//	// Process physics in fixed time steps
//	while (accumulator >= const_update_time) {
//		// --- Broad Phase (Sequential) ---
//		for (int i = 0; i < 5; i++) {
//			for (auto& registry : registries) {
//				broad_phase_collision::update(
//					registry,
//					const_update_time
//				);
//			}
//		}
//
//		// --- Narrow Phase / Object Update (Parallel) ---
//
//		// 1. POST TASKS
//		// Iterate through all objects and post one task for each object's update.
//		for (auto& reg_ref : registries) {
//			auto& registry = reg_ref.get();
//			for (auto& object : registry.linked_objects_write<motion_component>()) {
//				auto coll_comp = registry.try_linked_object_write<collision_component>(object.owner_id());
//
//				// *** THE FIX ***
//				// We must capture a pointer to the object, not a reference to the loop variable.
//				// The loop variable 'object' is a reference that will be invalid after the loop finishes.
//				// By getting a pointer to the object and capturing that pointer *by value*,
//				// each lambda gets its own unique, stable pointer to the correct motion_component.
//				motion_component* object_ptr = &object;
//
//				task::post([object_ptr, const_update_time, coll_comp] {
//					// Inside the task, we dereference the pointer to get the object back.
//					update_object(*object_ptr, const_update_time, coll_comp);
//					});
//			}
//		}
//
//		// 2. SYNCHRONIZE
//		// Wait for all posted tasks to finish. This will now work because the worker threads
//		// will be able to complete their tasks without crashing.
//		task::wait_idle();
//
//		accumulator -= const_update_time;
//	}
//}

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
		// --- Broad Phase (Sequential) ---
		// This phase often needs to be sequential as it builds the data structures
		// (like collision pairs) that the next phase depends on.
		for (int i = 0; i < 5; i++) {
			for (auto& registry : registries) {
				broad_phase_collision::update(
					registry
				);
			}
		}

		// --- Narrow Phase / Object Update (Parallel) ---

		// 1. GATHER TASKS
		// Create a single list of all objects to be updated from all registries.
		// Each task is a tuple containing all necessary data for the update.
		std::vector<std::tuple<
			std::reference_wrapper<motion_component>,
			std::reference_wrapper<collision_component>>
			> update_tasks;
		//update_tasks.reserve(1024); // Pre-allocate memory to avoid reallocations

		for (auto& reg_ref : registries) {
			auto& registry = reg_ref.get();
			for (auto& object : registry.linked_objects_write<motion_component>()) {
				update_tasks.emplace_back(
					object,
					registry.linked_object_write<collision_component>(object.owner_id())
				);
			}
		}

		// 2. EXECUTE TASKS IN PARALLEL
		// Use the index-based overload of parallel_for.
		task::parallel_for(std::size_t{ 0 }, update_tasks.size(), [&](const std::size_t i) {
			// Access the task data for the current index
			const auto& task_data = update_tasks[i];

			// Unpack the tuple for each task
			// Note: We use .get() to retrieve the actual reference from the reference_wrapper
			const auto& [object_ref, coll_comp_opt] = task_data;

			// Call the update function with the provided data
			update_object(object_ref.get(), const_update_time, &coll_comp_opt.get());
			});

		// 3. SYNCHRONIZE
		// Wait for all parallel_for tasks in this physics step to finish before proceeding.
		// Although parallel_for might be internally synchronous depending on the TBB implementation,
		// explicitly calling wait_idle ensures correctness according to your API's design.
		task::wait_idle();

		accumulator -= const_update_time;
	}
}
