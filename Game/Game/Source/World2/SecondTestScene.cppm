export module gs:second_test_scene;

import std;
import gse;

import :player;
import :sphere_light;

export namespace gs {
	class second_test_scene final : public gse::hook<gse::scene> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			// SHADOW DEBUG SCENE
			// Light at (0, 10, 0) pointing straight down
			// Floor at y = 0 (10 units below light)
			// Small box at (0, 2, 0) - should cast shadow on floor
			//
			// Expected: Point (0, 0, 0) on floor should have:
			//   - shadow_uv ≈ (0.5, 0.5) - center of shadow map
			//   - depth ≈ some value based on projection

			build("Shadow Test Box")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(0.f, 2.f, 0.f),
					.size = gse::vec3<gse::length>(2.f, 2.f, 2.f)  // Small 2x2x2 box
				});

			build("Floor")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(0.f, -0.5f, 0.f),
					.size = gse::vec3<gse::length>(20.f, 1.f, 20.f)  // Floor at y=0 (top surface)
				})
				.with<positioned_object_hook>({
					.position = gse::vec3<gse::length>(0.f, -0.5f, 0.f)
				});

			build("Player")
				.with<gse::free_camera>({
					.initial_position = gse::vec3<gse::length>(15.f, 5.f, 15.f)
				});

			// Spotlight straight down from y=10
			build("Test Light")
				.with<gse::spot_light_component>({
					.color = gse::unitless::vec3(1.f),
					.intensity = 10.f,
					.position = gse::vec3<gse::length>(0.f, 10.f, 0.f),
					.direction = gse::unitless::vec3(0.0f, -1.0f, 0.0f),
					.constant = 1.0f,
					.linear = 0.09f,
					.quadratic = 0.032f,
					.cut_off = gse::degrees(45.f),  // Wider angle for easier testing
					.outer_cut_off = gse::degrees(60.f),
					.ambient_strength = 0.1f,
					.near_plane = gse::meters(1.f),
					.far_plane = gse::meters(100.f),
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
				component_write<gse::physics::motion_component>().current_position = position;
			}
		};
	};
}