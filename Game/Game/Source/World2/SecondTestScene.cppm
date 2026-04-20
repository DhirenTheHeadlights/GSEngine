export module gs:second_test_scene;

import std;
import gse;

import :player;
import :entity_builders;

export namespace gs {
	class second_test_scene final : public gse::hook<gse::scene> {
	public:
		using hook::hook;

		auto initialize() -> void override;
	};
}

auto gs::second_test_scene::initialize() -> void {
	const auto floor_pos = gse::vec3<gse::position>(0.f, -0.5f, 0.f);
	build_static_box(this, "Floor", floor_pos, gse::vec3<gse::length>(20.f, 1.f, 20.f));

	for (int i = 0; i < 5; ++i) {
		build_box(this, std::format("Stack Box {}", i + 1),
			gse::vec3<gse::position>(5.f, 0.5f + static_cast<float>(i) * 1.0f, 5.f),
			gse::vec3<gse::length>(gse::meters(1.f)),
			gse::kilograms(200.f));
	}

	build_sphere(this, "Sphere",
		gse::vec3<gse::position>(-3.f, 5.f, 5.f),
		gse::meters(1.f));

	build("Player")
		.with<gs::player::component>({
			.initial_position = gse::vec3<gse::position>(0.f, 0.f, 0.f),
		});

	build("Scene Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(15.f, 8.f, 15.f),
		});

	build_sphere_light(this, "Test Light",
		gse::vec3<gse::position>(10.f, 15.f, 10.f),
		gse::meters(0.5f),
		12,
		8);
}
