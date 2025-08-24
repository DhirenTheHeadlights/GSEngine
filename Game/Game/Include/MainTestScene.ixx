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
			arena::create(this);

			build("Player")
				.with<gse::free_camera>({
					.initial_position = {}
				});

			build("Backpack")
				.with<backpack>();

			build("Smaller Box")
				.with<gse::box>({
					.initial_position = gse::vec::meters(20.f, -400.f, 20.f),
					.size = gse::vec::meters(20.f, 20.f, 20.f)
				});

			build("Bigger Box")
				.with<gse::box>({
					.initial_position = gse::vec::meters(-20.f, -400.f, 20.f),
					.size = gse::vec::meters(40.f, 40.f, 40.f)
				});

			build("Center Sphere Light")
				.with<sphere_light>({
					.initial_position = gse::vec::meters(0.f, -300.f, 0.f),
					.radius = gse::meters(10.f),
					.sectors = 18,
					.stacks = 10
				});

			build("Second Sphere")
				.with<gse::sphere>({
					.initial_position = gse::vec::meters(0.f, 0.f, 200.f),
					.radius = gse::meters(10.f),
					.sectors = 18,
					.stacks = 10
				});
		}
	};
} 