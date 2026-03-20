export module gs:physics_stress_test_scene;

import std;
import gse;

import :player;
import :sphere_light;

export namespace gs {
	class physics_stress_test_scene final : public gse::hook<gse::scene> {
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

			build_inverted_mass_pyramid();
			build_domino_chain();
			build_funnel();
			build_slope_friction_test();
			build_high_speed_impact_target();
			build_box_grid();

			build("Bouncy Sphere")
				.with<gse::sphere>({
					.initial_position = gse::vec3<gse::length>(-15.f, 8.f, 0.f),
					.radius = gse::meters(1.f),
					.sectors = 24,
					.stacks = 16
				});

			build("Player")
				.with<player>({
					.initial_position = gse::vec3<gse::length>(0.f, 10.f, 0.f)
				});

			build("Scene Camera")
				.with<gse::free_camera>({
					.initial_position = gse::vec3<gse::length>(0.f, 20.f, 40.f)
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
		auto build_inverted_mass_pyramid() const -> void {
			constexpr float x = -15.f;
			constexpr float z = 0.f;

			build("Pyramid Light Base")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x, 0.5f, z),
					.size = gse::vec3<gse::length>(gse::meters(1.f)),
					.mass = gse::kilograms(5.f)
				});

			build("Pyramid Mid")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x, 1.5f, z),
					.size = gse::vec3<gse::length>(gse::meters(1.f)),
					.mass = gse::kilograms(50.f)
				});

			build("Pyramid Heavy Top")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x, 2.5f, z),
					.size = gse::vec3<gse::length>(gse::meters(1.f)),
					.mass = gse::kilograms(500.f)
				});
		}

		auto build_domino_chain() const -> void {
			constexpr float z = -10.f;
			constexpr float start_x = -8.f;
			constexpr float spacing = 0.9f;

			for (int i = 0; i < 12; ++i) {
				const float x = start_x + static_cast<float>(i) * spacing;
				build(std::format("Domino {}", i + 1))
					.with<gse::box>({
						.initial_position = gse::vec3<gse::length>(x, (i == 0) ? 1.2f : 1.f, z),
						.size = gse::vec3<gse::length>(0.3f, 2.f, 1.f),
						.initial_orientation = (i == 0) ? gse::quat({ 0.f, 0.f, 1.f }, gse::radians(-0.8f)) : gse::quat(),
						.mass = gse::kilograms(30.f)
					});
			}
		}

		auto build_funnel() const -> void {
			constexpr float cx = 15.f;
			constexpr float cz = 0.f;

			constexpr float wall_len = 8.f;
			constexpr float wall_height = 4.f;
			constexpr float half_opening = 0.8f;
			constexpr float spread = 4.f;
			const float angle_deg = std::atan2(spread - half_opening, wall_len) * (180.f / 3.14159265f);

			const gse::quat left_rot(gse::unitless::axis_y, gse::degrees(angle_deg));
			const gse::quat right_rot(gse::unitless::axis_y, gse::degrees(-angle_deg));

			const float half_len = wall_len * 0.5f;
			const float mid_offset = (spread + half_opening) * 0.5f;

			build("Funnel Left Wall")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(cx - mid_offset, wall_height * 0.5f, cz),
					.size = gse::vec3<gse::length>(wall_len, wall_height, 0.3f),
					.initial_orientation = left_rot
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
						mc.position_locked = true;
					});
				});

			build("Funnel Right Wall")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(cx + mid_offset, wall_height * 0.5f, cz),
					.size = gse::vec3<gse::length>(wall_len, wall_height, 0.3f),
					.initial_orientation = right_rot
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
						mc.position_locked = true;
					});
				});

			build("Funnel Back Wall")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(cx, wall_height * 0.5f, cz - half_len),
					.size = gse::vec3<gse::length>(spread * 2.f + 1.f, wall_height, 0.3f)
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
						mc.position_locked = true;
					});
				});

			for (int row = 0; row < 3; ++row) {
				for (int col = 0; col < 3; ++col) {
					const float bx = cx - 1.f + static_cast<float>(col) * 1.1f;
					const float by = 0.5f + static_cast<float>(row) * 1.1f;
					const float bz = cz - 3.f;
					build(std::format("Funnel Box r{}c{}", row, col))
						.with<gse::box>({
							.initial_position = gse::vec3<gse::length>(bx, by, bz),
							.size = gse::vec3<gse::length>(gse::meters(1.f)),
							.mass = gse::kilograms(40.f)
						});
				}
			}
		}

		auto build_slope_friction_test() const -> void {
			constexpr float x = 0.f;
			constexpr float z = 15.f;

			const gse::vec3<gse::length> ramp_size(10.f, 0.5f, 4.f);
			const gse::vec3<gse::length> box_size(gse::meters(1.f));
			const auto resting_offset_for = [&](const gse::quat& tilt) {
				return gse::rotate_vector(
					tilt,
					gse::vec3<gse::length>(0.f, ramp_size.y() * 0.5f + box_size.y() * 0.5f, 0.f)
				);
			};

			const gse::quat ramp_tilt(gse::unitless::axis_z, gse::degrees(30.f));
			const gse::vec3<gse::length> ramp_position(x, 2.f, z);

			build("Ramp 30deg")
				.with<gse::box>({
					.initial_position = ramp_position,
					.size = ramp_size,
					.initial_orientation = ramp_tilt
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
						mc.position_locked = true;
					});
				});

			build("Ramp Box Should Hold")
				.with<gse::box>({
					.initial_position = ramp_position + resting_offset_for(ramp_tilt),
					.size = box_size,
					.initial_orientation = ramp_tilt,
					.mass = gse::kilograms(50.f)
				});

			const gse::quat steep_tilt(gse::unitless::axis_z, gse::degrees(45.f));
			const gse::vec3<gse::length> steep_ramp_position(x + 12.f, 2.f, z);

			build("Steep Ramp 45deg")
				.with<gse::box>({
					.initial_position = steep_ramp_position,
					.size = ramp_size,
					.initial_orientation = steep_tilt
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
						mc.position_locked = true;
					});
				});

			build("Steep Box Should Slide")
				.with<gse::box>({
					.initial_position = steep_ramp_position + resting_offset_for(steep_tilt),
					.size = box_size,
					.initial_orientation = steep_tilt,
					.mass = gse::kilograms(50.f)
				});
		}

		auto build_high_speed_impact_target() const -> void {
			constexpr float x = 0.f;
			constexpr float z = -20.f;

			for (int row = 0; row < 3; ++row) {
				for (int col = 0; col < 3; ++col) {
					const float bx = x - 1.1f + static_cast<float>(col) * 1.1f;
					const float by = 0.5f + static_cast<float>(row) * 1.05f;
					build(std::format("Impact Wall r{}c{}", row, col))
						.with<gse::box>({
							.initial_position = gse::vec3<gse::length>(bx, by, z),
							.size = gse::vec3<gse::length>(gse::meters(1.f)),
							.mass = gse::kilograms(80.f)
						});
				}
			}
		}

		auto build_box_grid() const -> void {
			constexpr int grid_x = 6;
			constexpr int grid_z = 6;
			constexpr int layers = 3;
			constexpr float spacing = 1.1f;
			constexpr float base_x = 20.f;
			constexpr float base_z = -10.f;

			for (int layer = 0; layer < layers; ++layer) {
				for (int ix = 0; ix < grid_x; ++ix) {
					for (int iz = 0; iz < grid_z; ++iz) {
						const float x = base_x + static_cast<float>(ix) * spacing;
						const float y = 0.5f + static_cast<float>(layer) * 1.05f;
						const float z = base_z + static_cast<float>(iz) * spacing;
						build(std::format("Grid L{}R{}C{}", layer, ix, iz))
							.with<gse::box>({
								.initial_position = gse::vec3<gse::length>(x, y, z),
								.size = gse::vec3<gse::length>(gse::meters(1.f)),
								.mass = gse::kilograms(20.f)
							});
					}
				}
			}
		}
	};
}
