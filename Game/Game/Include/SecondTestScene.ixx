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
					.initial_position = gse::vec::meters(0.f, 0.f, 0.f),
					.size = gse::vec::meters(1.f, 1.f, 1.f)
				})
				.with<positioned_object_hook>({
					.position = gse::vec::meters(0.f, 0.f, 0.f)
				});

			build("Free Cam")
				.with<gse::free_camera>({
					.initial_position = gse::vec::meters(30.f, 0.f, 0.f)
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
				});

				configure_when_present([](gse::physics::collision_component& cc) {
					cc.resolve_collisions = false;
				});
			}

			auto update() -> void override {
				component<gse::physics::motion_component>().current_position = position;
			}
		};
	};
}