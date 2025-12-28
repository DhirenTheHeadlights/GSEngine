export module gse.physics:simulation_system;

import std;

import gse.utility;
import gse.physics.math;

import :broad_phase_collision;
import :motion_component;
import :system;

export namespace gse::physics {
	struct simulation_system : ecs_system<
		read_set<motion_component, collision_component>,
		write_set<motion_component, collision_component>
	> {
		using ecs_system::ecs_system;

		using schedule = system_schedule<
			system_stage<
				system_stage_kind::update,
				gse::read_set<motion_component, collision_component>,
				gse::write_set<motion_component, collision_component>
			>
		>;

		auto update() -> void override;
	};
}

auto gse::physics::simulation_system::update() -> void {
	time frame_time = system_clock::dt<time_t<float, seconds>>();
	constexpr time_t<float, seconds> max_time_step = seconds(0.25f);
	frame_time = min(frame_time, max_time_step);

	static time_t<float, seconds> accumulator{};
	accumulator += frame_time;

	const auto const_update_time = system_clock::constant_update_time<time_t<float, seconds>>();

	while (accumulator >= const_update_time) {
		auto [motion_chunk, collision_chunk] = write<motion_component, collision_component>();

		for (auto& motion : motion_chunk) {
			motion.airborne = true;
		}

		for (auto& collision : collision_chunk) {
			if (!collision.resolve_collisions) {
				continue;
			}
			collision.collision_information = {
				.colliding = false,
				.collision_normal = {},
				.penetration = {},
				.collision_point = {}
			};
		}

		std::vector<broad_phase_collision::broad_phase_entry> objects;
		objects.reserve(collision_chunk.size());

		for (auto& collision : collision_chunk) {
			if (!collision.resolve_collisions) {
				continue;
			}
			
			auto* motion = collision_chunk.other_write_from<motion_component>(collision);
			
			objects.push_back({
				.collision = std::addressof(collision),
				.motion = motion
			});
		}

		broad_phase_collision::update(objects);

		struct task_entry {
			motion_component* motion;
			collision_component* collision;
		};

		std::vector<task_entry> tasks;
		tasks.reserve(motion_chunk.size());

		for (auto& mc : motion_chunk) {
			auto* cc = motion_chunk.other_write_from<collision_component>(mc);
			tasks.push_back(task_entry{
				.motion = std::addressof(mc),
				.collision = cc
			});
		}

		task::parallel_for(std::size_t{ 0 }, tasks.size(), [&](const std::size_t i) {
			auto& [motion, collision] = tasks[i];
			update_object(*motion, collision);
		});

		accumulator -= const_update_time;
	}
}