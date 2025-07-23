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
					.size = gse::vec::meters(10.f, 10.f, 10.f)
				});

			build("Free Cam")
				.with<gse::free_camera>({
					.initial_position = gse::vec::meters(0.f, 0.f, 5.f)
				});
		}
	private:
		struct positioned_object_hook final : hook<gse::entity> {
			positioned_object_hook(const gse::id& owner_id, gse::registry* reg, const gse::vec3<gse::length> position) : hook(owner_id, reg), position(position) {}

			gse::vec3<gse::length> position;

			auto update() -> void override {
				component<gse::physics::motion_component>().current_position = position;
			}
		};
	};
}