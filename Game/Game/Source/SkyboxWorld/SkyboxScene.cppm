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
					.initial_position = gse::vec3<gse::length>(0.f, 0.f, 0.f),
					.size = gse::vec3<gse::length>(20000.f, 20000.f, 20000.f)
				});

			build("Skybox Floor")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(0.f, -500.f, 0.f),
					.size = gse::vec3<gse::length>(20000.f, 10.f, 20000.f)
				});

			build("Player")
				.with<player>();
		}

		auto render() -> void override {
			
		}
	private:
		class floor_hook final : hook<gse::entity> {
		public:
			using hook::hook;

			auto initialize() -> void override {
				component_write<gse::physics::collision_component>().resolve_collisions = false;
				component_write<gse::physics::motion_component>().affected_by_gravity = false;
			}
		};
	};
}
