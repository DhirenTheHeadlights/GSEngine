export module gse.physics:system;

import std;

import gse.utility;
import gse.physics.math;

import :broad_phase_collision;
import :motion_component;
import :collision_component;
import :integrators;

export namespace gse::physics {
	struct state {
		time_t<float, seconds> accumulator{};
		bool update_phys = true;
	};

	struct system {
		static auto initialize(initialize_phase& phase, state& s) -> void;
		static auto update(update_phase& phase, state& s) -> void;
	};
}

auto gse::physics::system::initialize(initialize_phase& phase, state& s) -> void {
	phase.channels.push(save::register_property{
		.category = "Physics",
		.name = "Update Physics",
		.description = "Enable or disable the physics system update loop",
		.ref = &s.update_phys,
		.type = typeid(bool)
	});

	if (const auto* save_state = phase.try_state_of<save::state>()) {
		s.update_phys = save_state->read("Physics", "Update Physics", true);
	}
}

auto gse::physics::system::update(update_phase& phase, state& s) -> void {
	if (!s.update_phys) {
		return;
	}

	auto frame_time = system_clock::dt<time_t<float, seconds>>();
	constexpr time_t<float, seconds> max_time_step = seconds(0.25f);
	frame_time = std::min(frame_time, max_time_step);
	s.accumulator += frame_time;

	const time_t<float, seconds> const_update_time = system_clock::constant_update_time<time_t<float, seconds>>();

	int steps = 0;
	while (s.accumulator >= const_update_time) {
		s.accumulator -= const_update_time;
		steps++;
	}

	if (steps == 0) return;

	phase.schedule([steps](chunk<motion_component> motion, chunk<collision_component> collision) {
		for (int step = 0; step < steps; ++step) {
			for (motion_component& mc : motion) {
				mc.airborne = true;
			}

			for (collision_component& cc : collision) {
				if (!cc.resolve_collisions) {
					continue;
				}

				cc.collision_information = {
					.colliding = false,
					.collision_normal = {},
					.penetration = {},
					.collision_points = {}
				};
			}

			std::vector<broad_phase_collision::broad_phase_entry> objects;
			objects.reserve(collision.size());

			for (collision_component& cc : collision) {
				if (!cc.resolve_collisions) {
					continue;
				}

				objects.push_back({
					.collision = std::addressof(cc),
					.motion = motion.find(cc.owner_id())
				});
			}

			broad_phase_collision::update(objects);

			struct task_entry {
				motion_component* motion_ptr;
				collision_component* collision_ptr;
			};

			std::vector<task_entry> tasks;
			tasks.reserve(motion.size());

			for (motion_component& mc : motion) {
				tasks.push_back({
					.motion_ptr = std::addressof(mc),
					.collision_ptr = collision.find(mc.owner_id())
				});
			}

			task::parallel_for(0uz, tasks.size(), [&](const std::size_t i) {
				auto& [motion_ptr, collision_ptr] = tasks[i];
				update_object(*motion_ptr, collision_ptr);
			});
		}
	});
}
