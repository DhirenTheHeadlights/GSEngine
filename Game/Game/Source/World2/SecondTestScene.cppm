export module gs:second_test_scene;

import std;
import gse;

import :player;
import :sphere_light;

export namespace gs {
	class second_test_scene final : public gse::hook<gse::scene> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			const auto floor_pos = gse::vec3<gse::length>(0.f, -0.5f, 0.f);
			build("Floor")
				.with<gse::box>({
					.initial_position = floor_pos,
					.size = gse::vec3<gse::length>(20.f, 1.f, 20.f)
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
						mc.position_locked = true;
					});
				})
				.with_update([floor_pos](hook<gse::entity>& h) {
					h.component_write<gse::physics::motion_component>().current_position = floor_pos;
				});

			for (int i = 0; i < 5; ++i) {
				build(std::format("Stack Box {}", i + 1))
					.with<gse::box>({
						.initial_position = gse::vec3<gse::length>(5.f, 0.5f + static_cast<float>(i) * 1.0f, 5.f),
						.size = gse::vec3<gse::length>(1.f),
						.mass = gse::kilograms(200.f)
					});
			}

			build("Player")
				.with<player>({
					.initial_position = gse::vec3<gse::length>(0.f, 0.f, 0.f)
				});

			build("Scene Camera")
				.with<gse::free_camera>({
					.initial_position = gse::vec3<gse::length>(15.f, 8.f, 15.f)
				});

			build("Test Light")
				.with<sphere_light>({
					.initial_position = gse::vec3<gse::length>(10.f, 15.f, 10.f),
					.radius = gse::meters(0.5f),
					.sectors = 12,
					.stacks = 8
				});
		}
	private:
	};
}