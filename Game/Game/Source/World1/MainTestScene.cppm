export module gs:main_test_scene;

import :player;
import :arena;
import :sphere_light;
import :backpack;
import :ironman;
import :black_knight;

import gse;

export namespace gs {
	class main_test_scene final : public gse::hook<gse::scene> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			m_owner->set_player_factory([](gse::scene& s) -> gse::id {
				auto player_id = s.add_entity("Player");
				s.registry().add_hook<gse::free_camera>(player_id, gse::free_camera::params{
					.initial_position = gse::vec3<gse::length>(0.f, 0.f, 0.f)
				});
				return player_id;
			});

			arena::create(this);

			build("Backpack")
				.with<backpack>();

			build("Smaller Box")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(2.f, -40.f, 2.f),
					.size = gse::vec3<gse::length>(2.f, 2.f, 2.f)
				});

			build("Bigger Box")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(-2.f, -40.f, 2.f),
					.size = gse::vec3<gse::length>(4.f, 4.f, 4.f)
				});

			build("Center Sphere Light")
				.with<sphere_light>({
					.initial_position = gse::vec3<gse::length>(0.f, -30.f, 0.f),
					.radius = gse::meters(1.f),
					.sectors = 18,
					.stacks = 10
				});

			build("Second Sphere")
				.with<gse::sphere>({
					.initial_position = gse::vec3<gse::length>(0.f, 0.f, 20.f),
					.radius = gse::meters(1.f),
					.sectors = 18,
					.stacks = 10
				});

			build("Animated Box")
				.with<gse::animated_box>({
					.initial_position = gse::vec3<gse::length>(5.f, -35.f, 0.f),
					.size = gse::vec3<gse::length>(2.f, 2.f, 2.f)
				});
		}
	};
}
