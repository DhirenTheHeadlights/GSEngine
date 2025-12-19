export module gse.physics:simulation_system;

import std;

import gse.utility;
import gse.physics.math;

import :broad_phase_collision;
import :motion_component;

export namespace gse::physics {
	struct simulation_system : system<
		read_set<motion_component, collision_component>,
		write_set<motion_component, collision_component>
	> {
		using base = system;
		using system::system;

		using schedule = system_schedule<
			system_stage<
				system_stage_kind::update,
				gse::read_set<motion_component, collision_component>,
				gse::write_set<motion_component, collision_component>
			>
		>;

		auto update(
		) -> void;
	};
}

using namespace gse;
using namespace gse::physics;

auto simulation_system::update() -> void {
	time frame_time = system_clock::dt<time_t<float, seconds>>();
	constexpr time_t<float, seconds> max_time_step = seconds(0.25f);
	frame_time = min(frame_time, max_time_step);

	static time_t<float, seconds> accumulator{};
	accumulator += frame_time;

	const auto const_update_time = system_clock::constant_update_time<time_t<float, seconds>>();

	while (accumulator >= const_update_time) {
		for (auto& reg_ref : registries()) {
			auto& reg = reg_ref.get();
			broad_phase_collision::update(reg);
		}

		auto [motion_chunks, collision_chunks] = write<motion_component, collision_component>();

		struct task_entry {
			motion_component* motion;
			collision_component* collision;
		};

		std::vector<task_entry> tasks;

		for (auto& chunk : motion_chunks) {
			for (auto& mc : chunk.span) {
				auto* cc = chunk.other_write_from<collision_component>(mc);
				tasks.push_back(task_entry{
					.motion = std::addressof(mc),
					.collision = cc
				});
			}
		}

		task::parallel_for(std::size_t{ 0 }, tasks.size(), [&](std::size_t i) {
			auto& [motion, collision] = tasks[i];
			update_object(*motion, collision);
		});

		accumulator -= const_update_time;
	}
}
