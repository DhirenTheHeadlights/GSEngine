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
			build("Floor")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(0.f, -0.5f, 0.f),
					.size = gse::vec3<gse::length>(20.f, 1.f, 20.f)
				})
				.with<positioned_object_hook>({
					.position = gse::vec3<gse::length>(0.f, -0.5f, 0.f)
				});

			for (int i = 0; i < 8; ++i) {
				const float angle = static_cast<float>(i) * 3.14159f * 2.f / 8.f;
				const float r = 4.f;
				const float x = 5.f + r * std::cos(angle);
				const float z = 5.f + r * std::sin(angle);
				build(std::format("Small Box {}", i + 1))
					.with<gse::box>({
						.initial_position = gse::vec3<gse::length>(x, 0.5f, z),
						.size = gse::vec3<gse::length>(1.f),
						.box_mass = gse::kilograms(20.f)
					});
			}

			build("Player")
				.with<player>({
					.initial_position = gse::vec3<gse::length>(5.f, 2.f, 5.f)
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
		struct positioned_object_hook final : hook<gse::entity> {
			struct params {
				gse::vec3<gse::length> position;
			};

			positioned_object_hook(const params& p) : position(p.position) {}

			gse::vec3<gse::length> position;

			auto initialize() -> void override {
				configure_when_present([](gse::physics::motion_component& mc) {
					mc.affected_by_gravity = false;
					mc.position_locked = true;
				});
			}

			auto update() -> void override {
				component_write<gse::physics::motion_component>().current_position = position;
			}
		};
	};
}