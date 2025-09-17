export module gs:second_test_scene;

import std;
import gse;

export namespace gs {
	class second_test_scene final : public gse::hook<gse::scene> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			build("Box")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(0.f, 0.f, 0.f),
					.size = gse::vec3<gse::length>(10.f, 10.f, 10.f)
				});

			build("Floor")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(0.f, -20.f, 0.f),
					.size = gse::vec3<gse::length>(100.f, 1.f, 100.f)
				})
				.with<positioned_object_hook>({
					.position = gse::vec3<gse::length>(0.f, -20.f, 0.f)
				});

			build("Free Cam")
				.with<gse::free_camera>({
					.initial_position = gse::vec3<gse::length>(30.f, 0.f, 0.f)
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
				component<gse::physics::motion_component>().current_position = position;
			}
		};
	};
}