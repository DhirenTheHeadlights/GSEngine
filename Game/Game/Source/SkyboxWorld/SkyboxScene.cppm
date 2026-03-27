export module gs:skybox_scene;

import :skybox;
import :player;

import std;
import gse;

export namespace gs {
	class skybox_scene final : public gse::hook<gse::scene> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			build("Skybox")
				.with<skybox>({
					.initial_position = gse::vec3<gse::position>(0.f, 0.f, 0.f),
					.size = gse::vec3<gse::length>(20000.f, 20000.f, 20000.f)
				});

			build("Skybox Floor")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::position>(0.f, -500.f, 0.f),
					.size = gse::vec3<gse::length>(20000.f, 10.f, 20000.f)
				})
				.with_init([](gse::hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::collision_component& cc) {
						cc.resolve_collisions = false;
					});
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
					});
				});

			build("Player")
				.with<player>({
					.initial_position = gse::vec3<gse::position>(0.f, 0.f, 0.f)
				});
		}

		auto render() -> void override {
			
		}
	private:
	};
}
