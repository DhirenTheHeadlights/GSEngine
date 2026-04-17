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
			const auto floor_pos = gse::vec3<gse::position>(0.f, -0.5f, 0.f);
			build("Floor")
				.with<gse::box>({
					.initial_position = floor_pos,
					.size = gse::vec3<gse::length>(60.f, 1.f, 60.f),
					.roughness = 0.05f,
					.metallic = 1.0f
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
			build_spring_tests();
			build_tumbler();

			build("Bouncy Sphere")
				.with<gse::sphere>({
					.initial_position = gse::vec3<gse::position>(-15.f, 8.f, 0.f),
					.radius = gse::meters(1.f),
					.sectors = 24,
					.stacks = 16
				});

			build("Player")
				.with<player>({
					.initial_position = gse::vec3<gse::position>(0.f, 10.f, 0.f)
				});

			build("Scene Camera")
				.with<gse::free_camera>({
					.initial_position = gse::vec3<gse::position>(0.f, 20.f, 40.f)
				});

			build("Scene Light")
				.with<sphere_light>({
					.initial_position = gse::vec3<gse::position>(0.f, 25.f, 0.f),
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
					.initial_position = gse::vec3<gse::position>(x, 0.5f, z),
					.size = gse::vec3(gse::meters(1.f)),
					.mass = gse::kilograms(5.f)
				});

			build("Pyramid Mid")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::position>(x, 1.5f, z),
					.size = gse::vec3(gse::meters(1.f)),
					.mass = gse::kilograms(50.f)
				});

			build("Pyramid Heavy Top")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::position>(x, 2.5f, z),
					.size = gse::vec3(gse::meters(1.f)),
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
						.initial_position = gse::vec3<gse::position>(x, (i == 0) ? 1.2f : 1.f, z),
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

			const gse::quat left_rot(gse::axis_y, gse::degrees(angle_deg));
			const gse::quat right_rot(gse::axis_y, gse::degrees(-angle_deg));

			const float half_len = wall_len * 0.5f;
			const float mid_offset = (spread + half_opening) * 0.5f;

			build("Funnel Left Wall")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::position>(cx - mid_offset, wall_height * 0.5f, cz),
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
					.initial_position = gse::vec3<gse::position>(cx + mid_offset, wall_height * 0.5f, cz),
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
					.initial_position = gse::vec3<gse::position>(cx, wall_height * 0.5f, cz - half_len),
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
							.initial_position = gse::vec3<gse::position>(bx, by, bz),
							.size = gse::vec3(gse::meters(1.f)),
							.mass = gse::kilograms(40.f)
						});
				}
			}
		}

		auto build_slope_friction_test() const -> void {
			constexpr float x = 0.f;
			constexpr float z = 15.f;

			const gse::vec3<gse::length> ramp_size(10.f, 0.5f, 4.f);
			const gse::vec3 box_size(gse::meters(1.f));
			const auto resting_offset_for = [&](const gse::quat& tilt) {
				return gse::rotate_vector(
					tilt,
					gse::vec3<gse::length>(0.f, ramp_size.y() * 0.5f + box_size.y() * 0.5f, 0.f)
				);
			};

			const gse::quat ramp_tilt(gse::axis_z, gse::degrees(30.f));
			const gse::vec3<gse::position> ramp_position(x, 2.f, z);

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

			const gse::quat steep_tilt(gse::axis_z, gse::degrees(45.f));
			const gse::vec3<gse::position> steep_ramp_position(x + 12.f, 2.f, z);

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
							.initial_position = gse::vec3<gse::position>(bx, by, z),
							.size = gse::vec3(gse::meters(1.f)),
							.mass = gse::kilograms(80.f)
						});
				}
			}
		}

		auto build_spring_tests() const -> void {
			constexpr float x = -25.f;
			constexpr float z = -20.f;

			constexpr std::array compliances = { gse::per_kilograms(0.001f), gse::per_kilograms(0.01f), gse::per_kilograms(0.1f) };

			for (int i = 0; i < 3; ++i) {
				const std::array<std::string, 3> labels = { "Stiff", "Medium", "Soft" };
				const float bx = x + static_cast<float>(i) * 5.f;

				const auto anchor_id = build(std::format("Spring {} Anchor", labels[i]))
					.with<gse::box>({
						.initial_position = gse::vec3<gse::position>(bx, 10.f, z),
						.size = gse::vec3<gse::length>(0.5f, 0.5f, 0.5f)
					})
					.with_init([](hook<gse::entity>& h) {
						h.configure_when_present([](gse::physics::motion_component& mc) {
							mc.affected_by_gravity = false;
							mc.position_locked = true;
						});
					})
					.identify();

				const auto bob_id = build(std::format("Spring {} Bob", labels[i]))
					.with<gse::sphere>({
						.initial_position = gse::vec3<gse::position>(bx + 2.f, 10.f, z),
						.radius = gse::meters(0.5f),
						.sectors = 16,
						.stacks = 12
					})
					.identify();

				gse::physics::join(anchor_id, bob_id, gse::physics::spring_joint{
					.target = gse::meters(4.f),
					.compliance = compliances[i],
					.damping = 0.3f,
				});
			}

			const auto chain_anchor = build("Spring Chain Anchor")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::position>(x + 18.f, 12.f, z),
					.size = gse::vec3<gse::length>(0.5f, 0.5f, 0.5f)
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
						mc.position_locked = true;
					});
				})
				.identify();

			auto prev_id = chain_anchor;
			for (int i = 0; i < 5; ++i) {
				const float by = 12.f - static_cast<float>(i + 1) * 2.f;
				const auto link_id = build(std::format("Spring Chain Link {}", i))
					.with<gse::sphere>({
						.initial_position = gse::vec3<gse::position>(x + 18.f, by, z),
						.radius = gse::meters(0.4f),
						.sectors = 16,
						.stacks = 12
					})
					.identify();

				gse::physics::join(prev_id, link_id, gse::physics::spring_joint{
					.target = gse::meters(1.5f),
					.compliance = gse::per_kilograms(0.02f),
					.damping = 0.5f,
				});
				prev_id = link_id;
			}
		}

		auto build_tumbler() const -> void {
			constexpr float cx = 0.f;
			constexpr float cy = 10.f;
			constexpr float cz = 24.f;
			const gse::vec3<gse::position> center(cx, cy, cz);
			constexpr auto rotation_axis = gse::axis_z;
			const gse::angular_velocity angular_speed = gse::radians_per_second(0.6f);

			constexpr float interior_half = 3.5f;
			constexpr float length_half = 6.0f;
			constexpr float thickness = 0.3f;
			constexpr float outer_half = interior_half + thickness;
			constexpr float wall_offset = interior_half + thickness * 0.5f;
			constexpr float side_wall_length = (length_half + thickness) * 2.f;

			struct wall_def {
				std::string_view suffix;
				gse::vec3<gse::length> local_offset;
				gse::vec3<gse::length> size;
			};

			const std::array walls = {
				wall_def{ "Bottom", gse::vec3<gse::length>(0.f, -wall_offset, 0.f), gse::vec3<gse::length>(outer_half * 2.f, thickness, side_wall_length) },
				wall_def{ "Top",    gse::vec3<gse::length>(0.f,  wall_offset, 0.f), gse::vec3<gse::length>(outer_half * 2.f, thickness, side_wall_length) },
				wall_def{ "Left",   gse::vec3<gse::length>(-wall_offset, 0.f, 0.f), gse::vec3<gse::length>(thickness, outer_half * 2.f, side_wall_length) },
				wall_def{ "Right",  gse::vec3<gse::length>( wall_offset, 0.f, 0.f), gse::vec3<gse::length>(thickness, outer_half * 2.f, side_wall_length) },
				wall_def{ "Front",  gse::vec3<gse::length>(0.f, 0.f,  length_half + thickness * 0.5f), gse::vec3<gse::length>(outer_half * 2.f, outer_half * 2.f, thickness) },
				wall_def{ "Back",   gse::vec3<gse::length>(0.f, 0.f, -(length_half + thickness * 0.5f)), gse::vec3<gse::length>(outer_half * 2.f, outer_half * 2.f, thickness) }
			};

			for (const auto& wall : walls) {
				build(std::format("Tumbler Wall {}", wall.suffix))
					.with<gse::box>({
						.initial_position = center + wall.local_offset,
						.size = wall.size,
						.mass = gse::kilograms(10000.f)
					})
					.with_init([](hook<gse::entity>& h) {
						h.configure_when_present([](gse::physics::motion_component& mc) {
							mc.affected_by_gravity = false;
							mc.position_locked = true;
							mc.update_orientation = true;
						});
					})
					.with_update([center, rotation_axis, angular_speed, local_offset = wall.local_offset, phase = gse::radians(0.f), accumulator = gse::seconds(0.f)](hook<gse::entity>& h) mutable {
						constexpr auto physics_step = gse::seconds(1.f / 60.f);
						accumulator += gse::system_clock::dt();
						while (accumulator >= physics_step) {
							accumulator -= physics_step;
							phase += angular_speed * physics_step;
						}
						const gse::quat world_rot(rotation_axis, phase);
						const auto world_offset = gse::rotate_vector(world_rot, local_offset);
						const gse::vec3<gse::angular_velocity> ang_vel(
							rotation_axis.x() * angular_speed,
							rotation_axis.y() * angular_speed,
							rotation_axis.z() * angular_speed
						);
						const auto lin_vel = cross(ang_vel, world_offset) / gse::rad;

						auto& mc = h.component_write<gse::physics::motion_component>();
						mc.current_position = center + world_offset;
						mc.orientation = world_rot;
						mc.current_velocity = lin_vel;
						mc.angular_velocity = ang_vel;
						mc.sleeping = false;
					});
			}

			constexpr int nx = 8;
			constexpr int ny = 8;
			constexpr int nz = 15;
			constexpr float content_size = 0.5f;
			constexpr float radial_span = interior_half - content_size;
			constexpr float axial_span = length_half - content_size;

			int content_id = 0;
			for (int ix = 0; ix < nx; ++ix) {
				for (int iy = 0; iy < ny; ++iy) {
					for (int iz = 0; iz < nz; ++iz) {
						const float fx = -radial_span + (static_cast<float>(ix) + 0.5f) * (radial_span * 2.f / nx);
						const float fy = -radial_span + (static_cast<float>(iy) + 0.5f) * (radial_span * 2.f / ny);
						const float fz = -axial_span + (static_cast<float>(iz) + 0.5f) * (axial_span * 2.f / nz);
						build(std::format("Tumbler Cube {}", content_id++))
							.with<gse::box>({
								.initial_position = gse::vec3<gse::position>(cx + fx, cy + fy, cz + fz),
								.size = gse::vec3(gse::meters(content_size)),
								.mass = gse::kilograms(1.f)
							});
					}
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
								.initial_position = gse::vec3<gse::position>(x, y, z),
								.size = gse::vec3(gse::meters(1.f)),
								.mass = gse::kilograms(20.f)
							});
					}
				}
			}
		}
	};
}
