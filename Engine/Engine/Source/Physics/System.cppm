export module gse.physics:system;

import std;

import gse.utility;
import gse.physics.math;

import :broad_phase_collision;
import :motion_component;
import :collision_component;
import :integrators;

export namespace gse::physics {
	class system final : public gse::system {
	public:
		auto initialize(
		) -> void override;

		auto update(
		) -> void override;
	private:
		time_t<float, seconds> m_accumulator{};
		bool m_update_phys = true;
	};
}

auto gse::physics::system::initialize() -> void {
	publish([this](channel<save::register_property>& ch) {
		ch.push({
			.category = "Physics",
			.name = "Update Physics",
			.description = "Enable or disable the physics system update loop",
			.ref = reinterpret_cast<void*>(&m_update_phys),
			.type = typeid(bool)
		});
	});
}

auto gse::physics::system::update() -> void {
	if (!m_update_phys) {
		return;
	}

	auto frame_time = system_clock::dt<time_t<float, seconds>>();
	constexpr time_t<float, seconds> max_time_step = seconds(0.25f);
	frame_time = std::min(frame_time, max_time_step);
	m_accumulator += frame_time;

	const time_t<float, seconds> const_update_time = system_clock::constant_update_time<time_t<float, seconds>>();

	int steps = 0;
	while (m_accumulator >= const_update_time) {
		m_accumulator -= const_update_time;
		steps++;
	}

	if (steps == 0) return;

	write([steps](
		const component_chunk<motion_component>& motion_chunk,
		const component_chunk<collision_component>& collision_chunk
	) {
		for (int step = 0; step < steps; ++step) {
			for (motion_component& motion : motion_chunk) {
				motion.airborne = true;
			}

			for (collision_component& collision : collision_chunk) {
				if (!collision.resolve_collisions) {
					continue;
				}

				collision.collision_information = {
					.colliding = false,
					.collision_normal = {},
					.penetration = {},
					.collision_points = {}
				};
			}

			std::vector<broad_phase_collision::broad_phase_entry> objects;
			objects.reserve(collision_chunk.size());

			for (collision_component& collision : collision_chunk) {
				if (!collision.resolve_collisions) {
					continue;
				}

				motion_component* motion = collision_chunk.write_from<motion_component>(collision);

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

			for (motion_component& mc : motion_chunk) {
				collision_component* cc = motion_chunk.write_from<collision_component>(mc);

				tasks.push_back({
					.motion = std::addressof(mc),
					.collision = cc
				});
			}

			task::parallel_for(0uz, tasks.size(), [&](const std::size_t i) {
				auto& [motion, collision] = tasks[i];
				update_object(*motion, collision);
			});
		}
	});
}
