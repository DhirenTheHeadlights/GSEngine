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

			constexpr float box_size = 2.f;
			constexpr float half_box = box_size / 2.f;

			for (int i = 0; i < 1; ++i) {
				const float y = half_box + static_cast<float>(i) * box_size;
				build(std::format("Stacked Box {}", i + 1))
					.with<gse::box>({
						.initial_position = gse::vec3<gse::length>(0.f, y, 0.f),
						.size = gse::vec3<gse::length>(box_size, box_size, box_size)
					});
			}

			build("Player")
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