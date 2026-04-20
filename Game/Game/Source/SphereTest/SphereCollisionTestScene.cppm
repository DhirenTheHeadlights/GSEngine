export module gs:sphere_collision_test_scene;

import std;
import gse;

import :player;
import :entity_builders;

export namespace gs {
	class sphere_collision_test_scene final : public gse::hook<gse::scene> {
	public:
		using hook::hook;

		auto initialize(
		) -> void override;
	private:
		auto build_sphere_drop_test(
		) const -> void;

		auto build_sphere_on_sphere_stack(
		) const -> void;

		auto build_sphere_vs_box_test(
		) const -> void;

		auto build_sphere_bowling(
		) const -> void;

		auto build_sphere_size_test(
		) const -> void;
	};
}

auto gs::sphere_collision_test_scene::initialize() -> void {
	const auto floor_pos = gse::vec3<gse::position>(0.f, -0.5f, 0.f);
	build_static_box(this, "Floor", floor_pos, gse::vec3<gse::length>(60.f, 1.f, 60.f));

	build_sphere_drop_test();
	build_sphere_on_sphere_stack();
	build_sphere_vs_box_test();
	build_sphere_bowling();
	build_sphere_size_test();

	build("Player")
		.with<gs::player::component>({
			.initial_position = gse::vec3<gse::position>(0.f, 3.f, 20.f),
		});

	build("Scene Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(0.f, 15.f, 35.f),
		});

	build_sphere_light(this, "Scene Light",
		gse::vec3<gse::position>(0.f, 25.f, 0.f),
		gse::meters(0.5f),
		12,
		8);
}

auto gs::sphere_collision_test_scene::build_sphere_drop_test() const -> void {
	constexpr float x = -20.f;
	constexpr float z = 0.f;

	for (int i = 0; i < 5; ++i) {
		build_sphere(this, std::format("Drop Sphere {}", i),
			gse::vec3<gse::position>(x, 2.f + static_cast<float>(i) * 3.f, z),
			gse::meters(1.f));
	}
}

auto gs::sphere_collision_test_scene::build_sphere_on_sphere_stack() const -> void {
	constexpr float x = -10.f;
	constexpr float z = 0.f;

	build_sphere(this, "Stack Base Sphere", gse::vec3<gse::position>(x, 1.f, z), gse::meters(1.f));
	build_sphere(this, "Stack Mid Sphere", gse::vec3<gse::position>(x, 3.f, z), gse::meters(1.f));
	build_sphere(this, "Stack Top Sphere", gse::vec3<gse::position>(x, 5.f, z), gse::meters(1.f));
}

auto gs::sphere_collision_test_scene::build_sphere_vs_box_test() const -> void {
	constexpr float x = 0.f;
	constexpr float z = 0.f;

	build_static_box(this, "Box Pedestal",
		gse::vec3<gse::position>(x, 0.5f, z),
		gse::vec3<gse::length>(3.f, 1.f, 3.f));

	build_sphere(this, "Sphere On Box",
		gse::vec3<gse::position>(x, 4.f, z),
		gse::meters(1.f));

	build_static_box(this, "Box Wall Left",
		gse::vec3<gse::position>(x - 3.f, 2.f, z),
		gse::vec3<gse::length>(0.5f, 4.f, 4.f));

	build_static_box(this, "Box Wall Right",
		gse::vec3<gse::position>(x + 3.f, 2.f, z),
		gse::vec3<gse::length>(0.5f, 4.f, 4.f));

	build_sphere(this, "Sphere Between Walls",
		gse::vec3<gse::position>(x, 8.f, z),
		gse::meters(1.2f));
}

auto gs::sphere_collision_test_scene::build_sphere_bowling() const -> void {
	constexpr float x = 10.f;
	constexpr float z = 0.f;

	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col <= row; ++col) {
			const float bx = x + static_cast<float>(col) * 2.2f - static_cast<float>(row) * 1.1f;
			const float bz = z - static_cast<float>(row) * 2.f;
			build_box(this, std::format("Pin r{}c{}", row, col),
				gse::vec3<gse::position>(bx, 1.f, bz),
				gse::vec3<gse::length>(0.8f, 2.f, 0.8f),
				gse::kilograms(20.f));
		}
	}

	build_sphere(this, "Bowling Sphere",
		gse::vec3<gse::position>(x, 1.5f, z + 12.f),
		gse::meters(1.5f))
		.configure([](gse::physics::motion_component& mc) {
			mc.mass = gse::kilograms(500.f);
			mc.pending_impulse = gse::vec3<gse::velocity>(0.f, 0.f, -15.f) * gse::kilograms(500.f);
		});
}

auto gs::sphere_collision_test_scene::build_sphere_size_test() const -> void {
	constexpr float x = 22.f;
	constexpr float z = 0.f;

	const float radii[] = { 0.3f, 0.6f, 1.f, 1.5f, 2.5f };
	for (int i = 0; i < 5; ++i) {
		const float r = radii[i];
		build_sphere(this, std::format("Size Sphere {}", i),
			gse::vec3<gse::position>(x + static_cast<float>(i) * 4.f, r + 3.f, z),
			gse::meters(r));
	}
}
