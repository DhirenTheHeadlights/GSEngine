export module gs:runtime_spawns;

import std;
import gse;

import :entity_builders;
import :humanoid_skeleton;
import :piston;
import :skeleton_spawn;
import :test_skeletons;
import :tumbler;

export namespace gs {
	auto spawn_physics_stress(
		gse::scene& s
	) -> void;

	auto spawn_joint_test(
		gse::scene& s
	) -> void;
}

namespace gs {
	auto spawn_inverted_mass_pyramid(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_domino_chain(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_funnel(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_slope_friction_test(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_high_speed_impact_target(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_spring_tests(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_tumbler(
		gse::scene& s,
		int index,
		const gse::vec3<gse::position>& center,
		const gse::vec3f& rotation_axis,
		gse::angular_velocity angular_speed
	) -> void;

	auto spawn_box_grid(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_fixed_joint(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_distance_pendulum(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_hinge_door(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_slider_elevator(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_pendulum_chain(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_dangling_t(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_humanoid_test(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_sphere_stack(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_corner_drop(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_elevator(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_vise(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;
}

auto gs::spawn_inverted_mass_pyramid(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	s.spawn(
		"Pyramid Light Base",
		gs::box(
			origin + gse::vec3<gse::length>(0.f, 0.5f, 0.f),
			gse::vec3<gse::length>(gse::meters(1.f)),
			gse::kilograms(5.f)
		)
	);
	s.spawn(
		"Pyramid Mid",
		gs::box(
			origin + gse::vec3<gse::length>(0.f, 1.5f, 0.f),
			gse::vec3<gse::length>(gse::meters(1.f)),
			gse::kilograms(50.f)
		)
	);
	s.spawn(
		"Pyramid Heavy Top",
		gs::box(
			origin + gse::vec3<gse::length>(0.f, 2.5f, 0.f),
			gse::vec3<gse::length>(gse::meters(1.f)),
			gse::kilograms(500.f)
		)
	);
}

auto gs::spawn_domino_chain(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	constexpr float spacing = 0.9f;

	for (int i = 0; i < 12; ++i) {
		const float x = static_cast<float>(i) * spacing;
		const auto pos = origin + gse::vec3<gse::length>(x, (i == 0) ? 1.2f : 1.f, 0.f);
		const auto tilt = (i == 0) ? gse::quat(
			{ 0.f, 0.f, 1.f },
			gse::radians(-0.8f)
		)
								   : gse::quat();
		s.spawn(
			std::format("Domino {}", i + 1),
			gs::box(
				pos,
				gse::vec3<gse::length>(0.3f, 2.f, 1.f),
				gse::kilograms(30.f),
				tilt
			)
		);
	}
}

auto gs::spawn_funnel(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	constexpr float wall_len = 8.f;
	constexpr float wall_height = 4.f;
	constexpr float half_opening = 0.8f;
	constexpr float spread = 4.f;
	const float angle_deg = std::atan2(spread - half_opening, wall_len) * (180.f / 3.14159265f);

	const gse::quat left_rot(gse::axis_y, gse::degrees(angle_deg));
	const gse::quat right_rot(gse::axis_y, gse::degrees(-angle_deg));

	const float half_len = wall_len * 0.5f;
	const float mid_offset = (spread + half_opening) * 0.5f;

	s.spawn(
		"Funnel Left Wall",
		gs::static_box(
			origin + gse::vec3<gse::length>(-mid_offset, wall_height * 0.5f, 0.f),
			gse::vec3<gse::length>(wall_len, wall_height, 0.3f),
			left_rot
		)
	);
	s.spawn(
		"Funnel Right Wall",
		gs::static_box(
			origin + gse::vec3<gse::length>(mid_offset, wall_height * 0.5f, 0.f),
			gse::vec3<gse::length>(wall_len, wall_height, 0.3f),
			right_rot
		)
	);
	s.spawn(
		"Funnel Back Wall",
		gs::static_box(
			origin + gse::vec3<gse::length>(0.f, wall_height * 0.5f, -half_len),
			gse::vec3<gse::length>(spread * 2.f + 1.f, wall_height, 0.3f)
		)
	);

	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			const auto pos = origin +
				gse::vec3<gse::length>(
				-1.f + static_cast<float>(col) * 1.1f,
				0.5f + static_cast<float>(row) * 1.1f,
				-3.f
				);
			s.spawn(
				std::format("Funnel Box r{}c{}", row, col),
				gs::box(
					pos,
					gse::vec3<gse::length>(gse::meters(1.f)),
					gse::kilograms(40.f)
				)
			);
		}
	}
}

auto gs::spawn_slope_friction_test(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	const gse::vec3<gse::length> ramp_size(10.f, 0.5f, 4.f);
	const gse::vec3 box_size(gse::meters(1.f));

	const auto resting_offset_for = [&](const gse::quat& tilt) {
		return gse::rotate_vector(tilt, gse::vec3<gse::length>(0.f, ramp_size.y() * 0.5f + box_size.y() * 0.5f, 0.f));
	};

	const gse::quat ramp_tilt(gse::axis_z, gse::degrees(30.f));
	const auto ramp_position = origin + gse::vec3<gse::length>(0.f, 2.f, 0.f);
	s.spawn("Ramp 30deg", gs::static_box(ramp_position, ramp_size, ramp_tilt));
	s.spawn(
		"Ramp Box Should Hold",
		gs::box(
			ramp_position + resting_offset_for(ramp_tilt),
			box_size,
			gse::kilograms(50.f),
			ramp_tilt
		)
	);

	const gse::quat steep_tilt(gse::axis_z, gse::degrees(45.f));
	const auto steep_position = origin + gse::vec3<gse::length>(12.f, 2.f, 0.f);
	s.spawn("Steep Ramp 45deg", gs::static_box(steep_position, ramp_size, steep_tilt));
	s.spawn(
		"Steep Box Should Slide",
		gs::box(
			steep_position + resting_offset_for(steep_tilt),
			box_size,
			gse::kilograms(50.f),
			steep_tilt
		)
	);
}

auto gs::spawn_high_speed_impact_target(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			const auto pos = origin +
				gse::vec3<gse::length>(
				-1.1f + static_cast<float>(col) * 1.1f,
				0.5f + static_cast<float>(row) * 1.05f,
				0.f
				);
			s.spawn(
				std::format("Impact Wall r{}c{}", row, col),
				gs::box(
					pos,
					gse::vec3<gse::length>(gse::meters(1.f)),
					gse::kilograms(80.f)
				)
			);
		}
	}
}

auto gs::spawn_spring_tests(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	constexpr std::array compliances = {
		gse::per_kilograms(0.001f),
		gse::per_kilograms(0.01f),
		gse::per_kilograms(0.1f),
	};

	for (int i = 0; i < 3; ++i) {
		const std::array<std::string, 3> labels = { "Stiff", "Medium", "Soft" };
		const auto anchor_pos = origin + gse::vec3<gse::length>(static_cast<float>(i) * 5.f, 10.f, 0.f);

		const auto anchor_id = s.spawn(
			std::format("Spring {} Anchor", labels[i]),
			gs::static_box(anchor_pos, gse::vec3<gse::length>(0.5f, 0.5f, 0.5f))
		);

		const auto bob_id = s.spawn(
			std::format("Spring {} Bob", labels[i]),
			gs::sphere(
				anchor_pos + gse::vec3<gse::length>(2.f, 0.f, 0.f),
				gse::meters(0.5f),
				gse::sphere_lod::lo
			)
		);

		s.build(std::format("Spring {} Joint", labels[i]))
			.with<gse::physics::joint_spec>({
				.entity_a = anchor_id,
				.entity_b = bob_id,
				.config = gse::physics::spring_joint{
					.target = gse::meters(4.f),
					.compliance = compliances[i],
					.damping = 0.3f,
				},
			});
	}

	const auto chain_anchor_pos = origin + gse::vec3<gse::length>(18.f, 12.f, 0.f);
	const auto chain_anchor =
		s.spawn(
			"Spring Chain Anchor",
			gs::static_box(chain_anchor_pos, gse::vec3<gse::length>(0.5f, 0.5f, 0.5f))
		);

	auto prev_id = chain_anchor;
	for (int i = 0; i < 5; ++i) {
		const auto link_pos = chain_anchor_pos + gse::vec3<gse::length>(0.f, -static_cast<float>(i + 1) * 2.f, 0.f);
		const auto link_id = s.spawn(
			std::format("Spring Chain Link {}", i),
			gs::sphere(link_pos, gse::meters(0.4f), gse::sphere_lod::lo)
		);

		s.build(std::format("Spring Chain Joint {}", i))
			.with<gse::physics::joint_spec>({
				.entity_a = prev_id,
				.entity_b = link_id,
				.config = gse::physics::spring_joint{
					.target = gse::meters(1.5f),
					.compliance = gse::per_kilograms(0.02f),
					.damping = 0.5f,
				},
			});
		prev_id = link_id;
	}
}

auto gs::spawn_tumbler(gse::scene& s, const int index, const gse::vec3<gse::position>& center, const gse::vec3f& rotation_axis, const gse::angular_velocity angular_speed) -> void {
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
		auto wall_arch = gs::box(center + wall.local_offset, wall.size, gse::kilograms(10000.f));
		wall_arch.motion.body = gse::physics::kinematic_body{};
		const auto wall_id = s.spawn(std::format("Tumbler {} Wall {}", index, wall.suffix), std::move(wall_arch));
		s.registry().add_component<gs::tumbler::component>(
			wall_id,
			{
				.center = center,
				.axis = rotation_axis,
				.angular_speed = angular_speed,
				.local_offset = wall.local_offset,
			}
		);
	}

	constexpr int nx = 6;
	constexpr int ny = 6;
	constexpr int nz = 12;
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
				s.spawn(
					std::format("Tumbler {} Cube {}", index, content_id++),
					gs::box(
						center + gse::vec3<gse::length>(fx, fy, fz),
						gse::vec3<gse::length>(gse::meters(content_size)),
						gse::kilograms(1.f)
					)
				);
			}
		}
	}
}

auto gs::spawn_box_grid(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	constexpr int grid_x = 6;
	constexpr int grid_z = 6;
	constexpr int layers = 3;
	constexpr float spacing = 1.1f;

	for (int layer = 0; layer < layers; ++layer) {
		for (int ix = 0; ix < grid_x; ++ix) {
			for (int iz = 0; iz < grid_z; ++iz) {
				const auto pos = origin +
					gse::vec3<gse::length>(
					static_cast<float>(ix) * spacing,
					0.5f + static_cast<float>(layer) * 1.05f,
					static_cast<float>(iz) * spacing
					);
				s.spawn(
					std::format("Grid L{}R{}C{}", layer, ix, iz),
					gs::box(
						pos,
						gse::vec3<gse::length>(gse::meters(1.f)),
						gse::kilograms(20.f)
					)
				);
			}
		}
	}
}

auto gs::spawn_fixed_joint(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	const auto anchor_pos = origin + gse::vec3<gse::length>(0.f, 5.f, 0.f);
	const auto anchor_id = s.spawn("Fixed Anchor", gs::static_box(anchor_pos, gse::vec3<gse::length>(gse::meters(1.f))));

	const auto hanging_id = s.spawn(
		"Fixed Hanging Box",
		gs::box(
			anchor_pos + gse::vec3<gse::length>(0.f, -1.5f, 0.f),
			gse::vec3<gse::length>(gse::meters(1.f)),
			gse::kilograms(20.f)
		)
	);

	s.build("Fixed Joint")
		.with<gse::physics::joint_spec>({
			.entity_a = anchor_id,
			.entity_b = hanging_id,
			.config = gse::physics::fixed_joint{
				.anchor_a = gse::vec3<gse::displacement>(0.f, -0.5f, 0.f),
				.anchor_b = gse::vec3<gse::displacement>(0.f, 0.5f, 0.f),
			},
		});
}

auto gs::spawn_distance_pendulum(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	const auto pivot_pos = origin + gse::vec3<gse::length>(0.f, 8.f, 0.f);
	const auto pivot_id = s.spawn("Distance Pivot", gs::static_box(pivot_pos, gse::vec3<gse::length>(0.5f, 0.5f, 0.5f)));

	const auto bob_id = s.spawn(
		"Distance Bob",
		gs::box(
			pivot_pos + gse::vec3<gse::length>(3.f, -3.f, 0.f),
			gse::vec3<gse::length>(gse::meters(1.f)),
			gse::kilograms(30.f)
		)
	);

	s.build("Distance Joint")
		.with<gse::physics::joint_spec>({
			.entity_a = pivot_id,
			.entity_b = bob_id,
			.config = gse::physics::distance_joint{
				.target = gse::meters(4.f),
			},
		});
}

auto gs::spawn_hinge_door(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	const auto frame_pos = origin + gse::vec3<gse::length>(0.f, 2.f, 0.f);
	const auto frame_id = s.spawn("Hinge Frame", gs::static_box(frame_pos, gse::vec3<gse::length>(0.3f, 4.f, 0.3f)));

	auto door = gs::box(
		frame_pos + gse::vec3<gse::length>(1.5f, 0.f, 0.f),
		gse::vec3<gse::length>(3.f, 3.5f, 0.2f),
		gse::kilograms(40.f)
	);
	if (auto* d = std::get_if<gse::physics::dynamic_body>(&door.motion.body)) {
		d->affected_by_gravity = false;
	}
	const auto door_id = s.spawn("Hinge Door", std::move(door));

	s.build("Hinge Joint")
		.with<gse::physics::joint_spec>({
			.entity_a = frame_id,
			.entity_b = door_id,
			.config = gse::physics::hinge_joint{
				.anchor_a = gse::vec3<gse::displacement>(0.15f, 0.f, 0.f),
				.anchor_b = gse::vec3<gse::displacement>(-1.5f, 0.f, 0.f),
				.axis = { 0.f, 1.f, 0.f },
				.limits = std::pair{ gse::radians(-1.57f), gse::radians(1.57f) },
			},
		});
}

auto gs::spawn_slider_elevator(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	const auto rail_pos = origin + gse::vec3<gse::length>(0.f, 4.f, 0.f);
	const auto rail_id = s.spawn("Slider Rail", gs::static_box(rail_pos, gse::vec3<gse::length>(0.3f, 8.f, 0.3f)));

	const auto platform_id = s.spawn(
		"Slider Platform",
		gs::box(
			rail_pos + gse::vec3<gse::length>(0.f, 2.f, 0.f),
			gse::vec3<gse::length>(2.f, 0.3f, 2.f),
			gse::kilograms(30.f)
		)
	);

	s.build("Slider Joint")
		.with<gse::physics::joint_spec>({
			.entity_a = rail_id,
			.entity_b = platform_id,
			.config = gse::physics::slider_joint{
				.axis = { 0.f, 1.f, 0.f },
			},
		});
}

auto gs::spawn_pendulum_chain(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	constexpr int chain_length = 5;
	constexpr float link_spacing = 1.5f;

	const auto ceiling_pos = origin + gse::vec3<gse::length>(0.f, 12.f, 0.f);
	const auto ceiling_id = s.spawn("Chain Ceiling", gs::static_box(ceiling_pos, gse::vec3<gse::length>(1.f, 0.5f, 1.f)));

	std::vector<gse::id> link_ids;
	link_ids.push_back(ceiling_id);

	for (int i = 0; i < chain_length; ++i) {
		const float y = 12.f - (static_cast<float>(i) + 1.f) * link_spacing;
		const float x_offset = (i == chain_length - 1) ? 2.f : 0.f;
		const auto link_pos = origin + gse::vec3<gse::length>(x_offset, y, 0.f);
		const auto link_id = s.spawn(
			std::format("Chain Link {}", i),
			gs::box(
				link_pos,
				gse::vec3<gse::length>(0.6f, 0.6f, 0.6f),
				gse::kilograms(15.f)
			)
		);
		link_ids.push_back(link_id);
	}

	for (std::size_t i = 0; i + 1 < link_ids.size(); ++i) {
		s.build(std::format("Chain Joint {}", i))
			.with<gse::physics::joint_spec>({
				.entity_a = link_ids[i],
				.entity_b = link_ids[i + 1],
				.config = gse::physics::distance_joint{
					.target = gse::meters(link_spacing),
				},
			});
	}
}

auto gs::spawn_sphere_stack(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	constexpr int stack_count = 8;
	constexpr float radius = 0.5f;
	constexpr float spacing = radius * 2.f + 0.02f;

	for (int i = 0; i < stack_count; ++i) {
		const auto pos = origin + gse::vec3<gse::length>(
			0.f,
			radius + spacing * static_cast<float>(i),
			0.f
		);
		s.spawn(
			std::format("Sphere Stack {}", i),
			gs::sphere(pos, gse::meters(radius), gse::sphere_lod::mid)
		);
	}
}

auto gs::spawn_corner_drop(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	s.spawn(
		"Corner Drop Plate",
		gs::static_box(
			origin + gse::vec3<gse::length>(0.f, 0.25f, 0.f),
			gse::vec3<gse::length>(6.f, 0.5f, 6.f)
		)
	);

	const auto tilt = gse::quat(gse::axis_x, gse::degrees(45.f)) * gse::quat(gse::axis_z, gse::degrees(35.264f));
	s.spawn(
		"Corner Drop Box",
		gs::box(
			origin + gse::vec3<gse::length>(0.f, 8.f, 0.f),
			gse::vec3<gse::length>(gse::meters(1.f)),
			gse::kilograms(20.f),
			tilt
		)
	);
}

auto gs::spawn_elevator(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	constexpr float platform_size = 3.f;
	constexpr float platform_height = 0.3f;
	constexpr float platform_y = 3.f;
	constexpr float oscillation_amplitude = 2.5f;
	constexpr float frequency_hz = 0.4f;
	const float omega_value = 2.f * std::numbers::pi_v<float> * frequency_hz;

	const auto platform_origin = origin + gse::vec3<gse::length>(0.f, platform_y, 0.f);
	auto platform_arch = gs::box(
		platform_origin,
		gse::vec3<gse::length>(platform_size, platform_height, platform_size),
		gse::kilograms(10000.f)
	);
	platform_arch.motion.body = gse::physics::kinematic_body{};

	const auto platform_id = s.spawn("Elevator Platform", std::move(platform_arch));
	s.registry().add_component<gs::piston::component>(
		platform_id,
		{
			.center = platform_origin,
			.amplitude = gse::vec3<gse::length>(0.f, oscillation_amplitude, 0.f),
			.omega = gse::radians_per_second(omega_value),
		}
	);

	constexpr int grid = 3;
	constexpr float box_size = 0.5f;
	constexpr float box_spacing = 0.7f;
	for (int i = 0; i < grid; ++i) {
		for (int j = 0; j < grid; ++j) {
			const auto pos = platform_origin + gse::vec3<gse::length>(
				-box_spacing + static_cast<float>(i) * box_spacing,
				platform_height * 0.5f + box_size * 0.5f + 0.05f,
				-box_spacing + static_cast<float>(j) * box_spacing
			);
			s.spawn(
				std::format("Elevator Box {}{}", i, j),
				gs::box(pos, gse::vec3<gse::length>(gse::meters(box_size)), gse::kilograms(2.f))
			);
		}
	}
}

auto gs::spawn_vise(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	constexpr float floor_thickness = 1.f;
	constexpr float wall_thickness = 0.3f;
	constexpr float wall_height = 3.f;
	constexpr float wall_depth = 2.f;
	constexpr float wall_center_offset = 1.0f;
	constexpr float wall_amplitude = 0.7f;
	constexpr float frequency_hz = 0.3f;
	const float omega_value = 2.f * std::numbers::pi_v<float> * frequency_hz;

	s.spawn(
		"Vise Floor",
		gs::static_box(
			origin + gse::vec3<gse::length>(0.f, floor_thickness * 0.5f, 0.f),
			gse::vec3<gse::length>(6.f, floor_thickness, wall_depth + 2.f)
		)
	);

	constexpr std::array<std::pair<std::string_view, float>, 2> wall_defs = {
		std::pair{ std::string_view{"Right"}, 1.f },
		std::pair{ std::string_view{"Left"}, -1.f },
	};

	for (const auto& [name, sign] : wall_defs) {
		const auto wall_center = origin + gse::vec3<gse::length>(
			sign * wall_center_offset,
			floor_thickness + wall_height * 0.5f,
			0.f
		);
		auto wall_arch = gs::box(
			wall_center,
			gse::vec3<gse::length>(wall_thickness, wall_height, wall_depth),
			gse::kilograms(10000.f)
		);
		wall_arch.motion.body = gse::physics::kinematic_body{};

		const auto wall_id = s.spawn(std::format("Vise {} Wall", name), std::move(wall_arch));
		s.registry().add_component<gs::piston::component>(
			wall_id,
			{
				.center = wall_center,
				.amplitude = gse::vec3<gse::length>(-sign * wall_amplitude, 0.f, 0.f),
				.omega = gse::radians_per_second(omega_value),
			}
		);
	}

	s.spawn(
		"Vise Box",
		gs::box(
			origin + gse::vec3<gse::length>(0.f, floor_thickness + 0.3f, 0.f),
			gse::vec3<gse::length>(gse::meters(0.5f)),
			gse::kilograms(5.f)
		)
	);
}

auto gs::spawn_physics_stress(gse::scene& s) -> void {
	spawn_inverted_mass_pyramid(s, gse::vec3<gse::position>(-15.f, 0.f, 0.f));
	spawn_domino_chain(s, gse::vec3<gse::position>(-8.f, 0.f, -10.f));
	spawn_funnel(s, gse::vec3<gse::position>(15.f, 0.f, 0.f));
	spawn_slope_friction_test(s, gse::vec3<gse::position>(0.f, 0.f, 15.f));
	spawn_high_speed_impact_target(s, gse::vec3<gse::position>(0.f, 0.f, -20.f));
	spawn_box_grid(s, gse::vec3<gse::position>(20.f, 0.f, -10.f));
	spawn_spring_tests(s, gse::vec3<gse::position>(-25.f, 0.f, -20.f));

	spawn_tumbler(s, 0, gse::vec3<gse::position>(-12.f, 10.f, 24.f), gse::axis_z, gse::radians_per_second(0.6f));
	spawn_tumbler(s, 1, gse::vec3<gse::position>(12.f, 10.f, 24.f), gse::axis_x, gse::radians_per_second(0.5f));

	spawn_sphere_stack(s, gse::vec3<gse::position>(30.f, 0.f, 5.f));
	spawn_corner_drop(s, gse::vec3<gse::position>(30.f, 0.f, 15.f));
	spawn_elevator(s, gse::vec3<gse::position>(-30.f, 0.f, 10.f));
	spawn_vise(s, gse::vec3<gse::position>(0.f, 0.f, 30.f));

	s.spawn("Bouncy Sphere", gs::sphere(gse::vec3<gse::position>(-15.f, 8.f, 0.f), gse::meters(1.f)));
}

auto gs::spawn_dangling_t(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	const auto anchor_pos = origin + gse::vec3<gse::length>(0.f, 8.f, 0.f);
	const auto anchor_id =
		s.spawn(
			"Skeleton Anchor",
			gs::static_box(anchor_pos, gse::vec3<gse::length>(gse::meters(0.5f)))
		);

	const auto torso_pos = anchor_pos + gse::vec3<gse::length>(0.f, -0.65f, 0.f);
	const auto skel = gs::test_skeleton_t();
	const auto handle = gs::spawn_skeleton(s, skel, torso_pos);

	s.build("Skeleton Anchor Joint")
		.with<gse::physics::joint_spec>({
			.entity_a = anchor_id,
			.entity_b = handle.bone_ids[0],
			.config = gse::physics::fixed_joint{
				.anchor_a = gse::vec3<gse::displacement>(0.f, -0.25f, 0.f),
				.anchor_b = gse::vec3<gse::displacement>(0.f, 0.4f, 0.f),
			},
		});
}

auto gs::spawn_humanoid_test(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	const auto anchor_pos = origin + gse::vec3<gse::length>(0.f, 4.f, 0.f);
	const auto anchor_id =
		s.spawn(
			"Humanoid Anchor",
			gs::static_box(anchor_pos, gse::vec3<gse::length>(gse::meters(0.5f)))
		);

	const auto pelvis_pos = anchor_pos + gse::vec3<gse::length>(0.f, -0.65f, 0.f);
	const auto handle = gs::spawn_humanoid(s, pelvis_pos);

	s.build("Humanoid Anchor Joint")
		.with<gse::physics::joint_spec>({
			.entity_a = anchor_id,
			.entity_b = handle.bone_ids[0],
			.config = gse::physics::fixed_joint{
				.anchor_a = gse::vec3<gse::displacement>(0.f, -0.25f, 0.f),
				.anchor_b = gse::vec3<gse::displacement>(0.f, 0.1f, 0.f),
			},
		});
}

auto gs::spawn_joint_test(gse::scene& s) -> void {
	spawn_fixed_joint(s, gse::vec3<gse::position>(-20.f, 0.f, 0.f));
	spawn_distance_pendulum(s, gse::vec3<gse::position>(-10.f, 0.f, 0.f));
	spawn_hinge_door(s, gse::vec3<gse::position>(0.f, 0.f, 0.f));
	spawn_slider_elevator(s, gse::vec3<gse::position>(10.f, 0.f, 0.f));
	spawn_pendulum_chain(s, gse::vec3<gse::position>(20.f, 0.f, 0.f));
	spawn_dangling_t(s, gse::vec3<gse::position>(0.f, 0.f, 12.f));
	spawn_humanoid_test(s, gse::vec3<gse::position>(-12.f, 0.f, 12.f));

	s.spawn("Joint Test Sphere", gs::sphere(gse::vec3<gse::position>(0.f, 6.f, -8.f), gse::meters(1.f)));
}
