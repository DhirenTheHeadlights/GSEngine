export module gs:sphere_collision_test_scene;

import std;
import gse;

import :player;
import :sphere_light;

export namespace gs {
	class sphere_collision_test_scene final : public gse::hook<gse::scene> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			const auto floor_pos = gse::vec3<gse::length>(0.f, -0.5f, 0.f);
			build("Floor")
				.with<gse::box>({
					.initial_position = floor_pos,
					.size = gse::vec3<gse::length>(60.f, 1.f, 60.f)
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
						mc.position_locked = true;
					});
				})
				.with_update([floor_pos](hook<gse::entity>& h) {
					h.component_write<gse::physics::motion_component>().current_position = floor_pos;
				});

			build_sphere_drop_test();
			build_sphere_on_sphere_stack();
			build_sphere_vs_box_test();
			build_sphere_bowling();
			build_sphere_size_test();

			build("Player")
				.with<player>({
					.initial_position = gse::vec3<gse::length>(0.f, 3.f, 20.f)
				});

			build("Scene Camera")
				.with<gse::free_camera>({
					.initial_position = gse::vec3<gse::length>(0.f, 15.f, 35.f)
				});

			build("Scene Light")
				.with<sphere_light>({
					.initial_position = gse::vec3<gse::length>(0.f, 25.f, 0.f),
					.radius = gse::meters(0.5f),
					.sectors = 12,
					.stacks = 8
				});
		}

	private:
		auto build_sphere_drop_test() const -> void {
			constexpr float x = -20.f;
			constexpr float z = 0.f;

			for (int i = 0; i < 5; ++i) {
				build(std::format("Drop Sphere {}", i))
					.with<gse::sphere>({
						.initial_position = gse::vec3<gse::length>(
							x, 2.f + static_cast<float>(i) * 3.f, z
						),
						.radius = gse::meters(1.f),
						.sectors = 24,
						.stacks = 16
					});
			}
		}

		auto build_sphere_on_sphere_stack() const -> void {
			constexpr float x = -10.f;
			constexpr float z = 0.f;

			build("Stack Base Sphere")
				.with<gse::sphere>({
					.initial_position = gse::vec3<gse::length>(x, 1.f, z),
					.radius = gse::meters(1.f),
					.sectors = 24,
					.stacks = 16
				});

			build("Stack Mid Sphere")
				.with<gse::sphere>({
					.initial_position = gse::vec3<gse::length>(x, 3.f, z),
					.radius = gse::meters(1.f),
					.sectors = 24,
					.stacks = 16
				});

			build("Stack Top Sphere")
				.with<gse::sphere>({
					.initial_position = gse::vec3<gse::length>(x, 5.f, z),
					.radius = gse::meters(1.f),
					.sectors = 24,
					.stacks = 16
				});
		}

		auto build_sphere_vs_box_test() const -> void {
			constexpr float x = 0.f;
			constexpr float z = 0.f;

			build("Box Pedestal")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x, 0.5f, z),
					.size = gse::vec3<gse::length>(3.f, 1.f, 3.f)
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
						mc.position_locked = true;
					});
				});

			build("Sphere On Box")
				.with<gse::sphere>({
					.initial_position = gse::vec3<gse::length>(x, 4.f, z),
					.radius = gse::meters(1.f),
					.sectors = 24,
					.stacks = 16
				});

			build("Box Wall Left")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x - 3.f, 2.f, z),
					.size = gse::vec3<gse::length>(0.5f, 4.f, 4.f)
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
						mc.position_locked = true;
					});
				});

			build("Box Wall Right")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x + 3.f, 2.f, z),
					.size = gse::vec3<gse::length>(0.5f, 4.f, 4.f)
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
						mc.position_locked = true;
					});
				});

			build("Sphere Between Walls")
				.with<gse::sphere>({
					.initial_position = gse::vec3<gse::length>(x, 8.f, z),
					.radius = gse::meters(1.2f),
					.sectors = 24,
					.stacks = 16
				});
		}

		auto build_sphere_bowling() const -> void {
			constexpr float x = 10.f;
			constexpr float z = 0.f;

			for (int row = 0; row < 4; ++row) {
				for (int col = 0; col <= row; ++col) {
					const float bx = x + static_cast<float>(col) * 2.2f - static_cast<float>(row) * 1.1f;
					const float bz = z - static_cast<float>(row) * 2.f;
					build(std::format("Pin r{}c{}", row, col))
						.with<gse::box>({
							.initial_position = gse::vec3<gse::length>(bx, 1.f, bz),
							.size = gse::vec3<gse::length>(0.8f, 2.f, 0.8f),
							.mass = gse::kilograms(20.f)
						});
				}
			}

			build("Bowling Sphere")
				.with<gse::sphere>({
					.initial_position = gse::vec3<gse::length>(x, 1.5f, z + 12.f),
					.radius = gse::meters(1.5f),
					.sectors = 24,
					.stacks = 16
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.mass = gse::kilograms(500.f);
						mc.pending_impulse = gse::vec3<gse::velocity>(0.f, 0.f, -15.f);
					});
				});
		}

		auto build_sphere_size_test() const -> void {
			constexpr float x = 22.f;
			constexpr float z = 0.f;

			const float radii[] = { 0.3f, 0.6f, 1.f, 1.5f, 2.5f };
			for (int i = 0; i < 5; ++i) {
				const float r = radii[i];
				build(std::format("Size Sphere {}", i))
					.with<gse::sphere>({
						.initial_position = gse::vec3<gse::length>(
							x + static_cast<float>(i) * 4.f, r + 3.f, z
						),
						.radius = gse::meters(r),
						.sectors = 24,
						.stacks = 16
					});
			}
		}
	};
}
