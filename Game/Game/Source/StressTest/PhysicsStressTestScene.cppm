export module gs:physics_stress_test_scene;

import std;
import gse;

import :player;
import :tumbler;
import :entity_builders;

export namespace gs {
	auto physics_stress_test_scene_setup(
		gse::scene& s
	) -> void;
}

namespace gs {
	auto build_inverted_mass_pyramid(
		gse::scene& s
	) -> void;

	auto build_domino_chain(
		gse::scene& s
	) -> void;

	auto build_funnel(
		gse::scene& s
	) -> void;

	auto build_slope_friction_test(
		gse::scene& s
	) -> void;

	auto build_high_speed_impact_target(
		gse::scene& s
	) -> void;

	auto build_spring_tests(
		gse::scene& s
	) -> void;

	auto build_tumbler(
		gse::scene& s
	) -> void;

	auto build_box_grid(
		gse::scene& s
	) -> void;
}

auto gs::build_inverted_mass_pyramid(gse::scene& s) -> void {
	constexpr float x = -15.f;
	constexpr float z = 0.f;

	build_box(
		&s,
		"Pyramid Light Base",
		gse::vec3<gse::position>(x, 0.5f, z),
		gse::vec3<gse::length>(gse::meters(1.f)),
		gse::kilograms(5.f)
	);

	build_box(
		&s,
		"Pyramid Mid",
		gse::vec3<gse::position>(x, 1.5f, z),
		gse::vec3<gse::length>(gse::meters(1.f)),
		gse::kilograms(50.f)
	);

	build_box(
		&s,
		"Pyramid Heavy Top",
		gse::vec3<gse::position>(x, 2.5f, z),
		gse::vec3<gse::length>(gse::meters(1.f)),
		gse::kilograms(500.f)
	);
}

auto gs::build_domino_chain(gse::scene& s) -> void {
	constexpr float z = -10.f;
	constexpr float start_x = -8.f;
	constexpr float spacing = 0.9f;

	for (int i = 0; i < 12; ++i) {
		const float x = start_x + static_cast<float>(i) * spacing;
		build_box(
			&s,
			std::format("Domino {}", i + 1),
			gse::vec3<gse::position>(x, (i == 0) ? 1.2f : 1.f, z),
			gse::vec3<gse::length>(0.3f, 2.f, 1.f),
			gse::kilograms(30.f),
			(i == 0) ? gse::quat({ 0.f, 0.f, 1.f }, gse::radians(-0.8f)) : gse::quat()
		);
	}
}

auto gs::build_funnel(gse::scene& s) -> void {
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

	build_static_box(
		&s,
		"Funnel Left Wall",
		gse::vec3<gse::position>(cx - mid_offset, wall_height * 0.5f, cz),
		gse::vec3<gse::length>(wall_len, wall_height, 0.3f),
		left_rot
	);

	build_static_box(
		&s,
		"Funnel Right Wall",
		gse::vec3<gse::position>(cx + mid_offset, wall_height * 0.5f, cz),
		gse::vec3<gse::length>(wall_len, wall_height, 0.3f),
		right_rot
	);

	build_static_box(
		&s,
		"Funnel Back Wall",
		gse::vec3<gse::position>(cx, wall_height * 0.5f, cz - half_len),
		gse::vec3<gse::length>(spread * 2.f + 1.f, wall_height, 0.3f)
	);

	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			const float bx = cx - 1.f + static_cast<float>(col) * 1.1f;
			const float by = 0.5f + static_cast<float>(row) * 1.1f;
			const float bz = cz - 3.f;
			build_box(
				&s,
				std::format("Funnel Box r{}c{}", row, col),
				gse::vec3<gse::position>(bx, by, bz),
				gse::vec3<gse::length>(gse::meters(1.f)),
				gse::kilograms(40.f)
			);
		}
	}
}

auto gs::build_slope_friction_test(gse::scene& s) -> void {
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

	build_static_box(&s, "Ramp 30deg", ramp_position, ramp_size, ramp_tilt);

	build_box(
		&s,
		"Ramp Box Should Hold",
		ramp_position + resting_offset_for(ramp_tilt),
		box_size,
		gse::kilograms(50.f),
		ramp_tilt
	);

	const gse::quat steep_tilt(gse::axis_z, gse::degrees(45.f));
	const gse::vec3<gse::position> steep_ramp_position(x + 12.f, 2.f, z);

	build_static_box(&s, "Steep Ramp 45deg", steep_ramp_position, ramp_size, steep_tilt);

	build_box(
		&s,
		"Steep Box Should Slide",
		steep_ramp_position + resting_offset_for(steep_tilt),
		box_size,
		gse::kilograms(50.f),
		steep_tilt
	);
}

auto gs::build_high_speed_impact_target(gse::scene& s) -> void {
	constexpr float x = 0.f;
	constexpr float z = -20.f;

	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			const float bx = x - 1.1f + static_cast<float>(col) * 1.1f;
			const float by = 0.5f + static_cast<float>(row) * 1.05f;
			build_box(
				&s,
				std::format("Impact Wall r{}c{}", row, col),
				gse::vec3<gse::position>(bx, by, z),
				gse::vec3<gse::length>(gse::meters(1.f)),
				gse::kilograms(80.f)
			);
		}
	}
}

auto gs::build_spring_tests(gse::scene& s) -> void {
	constexpr float x = -25.f;
	constexpr float z = -20.f;

	constexpr std::array compliances = {
		gse::per_kilograms(0.001f),
		gse::per_kilograms(0.01f),
		gse::per_kilograms(0.1f),
	};

	for (int i = 0; i < 3; ++i) {
		const std::array<std::string, 3> labels = { "Stiff", "Medium", "Soft" };
		const float bx = x + static_cast<float>(i) * 5.f;

		const auto anchor_id = build_static_box(
			&s,
			std::format("Spring {} Anchor", labels[i]),
			gse::vec3<gse::position>(bx, 10.f, z),
			gse::vec3<gse::length>(0.5f, 0.5f, 0.5f)
		).identify();

		const auto bob_id = build_sphere(
			&s,
			std::format("Spring {} Bob", labels[i]),
			gse::vec3<gse::position>(bx + 2.f, 10.f, z),
			gse::meters(0.5f),
			16,
			12
		).identify();

		gse::physics::join(anchor_id, bob_id, gse::physics::spring_joint{
			.target = gse::meters(4.f),
			.compliance = compliances[i],
			.damping = 0.3f,
		});
	}

	const auto chain_anchor = build_static_box(
		&s,
		"Spring Chain Anchor",
		gse::vec3<gse::position>(x + 18.f, 12.f, z),
		gse::vec3<gse::length>(0.5f, 0.5f, 0.5f)
	).identify();

	auto prev_id = chain_anchor;
	for (int i = 0; i < 5; ++i) {
		const float by = 12.f - static_cast<float>(i + 1) * 2.f;
		const auto link_id = build_sphere(
			&s,
			std::format("Spring Chain Link {}", i),
			gse::vec3<gse::position>(x + 18.f, by, z),
			gse::meters(0.4f),
			16,
			12
		).identify();

		gse::physics::join(prev_id, link_id, gse::physics::spring_joint{
			.target = gse::meters(1.5f),
			.compliance = gse::per_kilograms(0.02f),
			.damping = 0.5f,
		});
		prev_id = link_id;
	}
}

auto gs::build_tumbler(gse::scene& s) -> void {
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
		wall_def{
			.suffix = "Bottom",
			.local_offset = gse::vec3<gse::length>(0.f, -wall_offset, 0.f),
			.size = gse::vec3<gse::length>(outer_half * 2.f, thickness, side_wall_length),
		},
		wall_def{
			.suffix = "Top",
			.local_offset = gse::vec3<gse::length>(0.f, wall_offset, 0.f),
			.size = gse::vec3<gse::length>(outer_half * 2.f, thickness, side_wall_length),
		},
		wall_def{
			.suffix = "Left",
			.local_offset = gse::vec3<gse::length>(-wall_offset, 0.f, 0.f),
			.size = gse::vec3<gse::length>(thickness, outer_half * 2.f, side_wall_length),
		},
		wall_def{
			.suffix = "Right",
			.local_offset = gse::vec3<gse::length>(wall_offset, 0.f, 0.f),
			.size = gse::vec3<gse::length>(thickness, outer_half * 2.f, side_wall_length),
		},
		wall_def{
			.suffix = "Front",
			.local_offset = gse::vec3<gse::length>(0.f, 0.f, length_half + thickness * 0.5f),
			.size = gse::vec3<gse::length>(outer_half * 2.f, outer_half * 2.f, thickness),
		},
		wall_def{
			.suffix = "Back",
			.local_offset = gse::vec3<gse::length>(0.f, 0.f, -(length_half + thickness * 0.5f)),
			.size = gse::vec3<gse::length>(outer_half * 2.f, outer_half * 2.f, thickness),
		},
	};

	for (const auto& wall : walls) {
		build_box(
			&s,
			std::format("Tumbler Wall {}", wall.suffix),
			center + wall.local_offset,
			wall.size,
			gse::kilograms(10000.f)
		)
			.with<gs::tumbler::component>({
				.center = center,
				.axis = rotation_axis,
				.angular_speed = angular_speed,
				.local_offset = wall.local_offset,
			})
			.configure([](gse::physics::motion_component& mc) {
				mc.affected_by_gravity = false;
				mc.position_locked = true;
				mc.update_orientation = true;
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
				build_box(
					&s,
					std::format("Tumbler Cube {}", content_id++),
					gse::vec3<gse::position>(cx + fx, cy + fy, cz + fz),
					gse::vec3<gse::length>(gse::meters(content_size)),
					gse::kilograms(1.f)
				);
			}
		}
	}
}

auto gs::build_box_grid(gse::scene& s) -> void {
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
				build_box(
					&s,
					std::format("Grid L{}R{}C{}", layer, ix, iz),
					gse::vec3<gse::position>(x, y, z),
					gse::vec3<gse::length>(gse::meters(1.f)),
					gse::kilograms(20.f)
				);
			}
		}
	}
}

auto gs::physics_stress_test_scene_setup(gse::scene& s) -> void {
	const auto floor_pos = gse::vec3<gse::position>(0.f, -0.5f, 0.f);
	build_static_box(&s, "Floor", floor_pos, gse::vec3<gse::length>(60.f, 1.f, 60.f));

	build_inverted_mass_pyramid(s);
	build_domino_chain(s);
	build_funnel(s);
	build_slope_friction_test(s);
	build_high_speed_impact_target(s);
	build_box_grid(s);
	build_spring_tests(s);
	build_tumbler(s);

	build_sphere(
		&s,
		"Bouncy Sphere",
		gse::vec3<gse::position>(-15.f, 8.f, 0.f),
		gse::meters(1.f)
	);

	s.build("Player")
		.with<gs::player::component>({
			.initial_position = gse::vec3<gse::position>(0.f, 10.f, 0.f),
		});

	s.build("Scene Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(0.f, 20.f, 40.f),
		});

	build_sphere_light(
		&s,
		"Scene Light",
		gse::vec3<gse::position>(0.f, 25.f, 0.f),
		gse::meters(0.5f),
		12,
		8
	);
}
