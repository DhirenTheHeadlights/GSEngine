export module sandbox:machines;

import std;
import gse;

import :archetypes;
import :piston;
import :spawn_params;
import :tumbler;

export namespace sandbox {
	auto spawn_elevator(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_vise(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_slider_elevator(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_hinge_door(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_funnel(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_tumbler(
		gse::scene& s,
		int index,
		const gse::vec3<gse::position>& center,
		const gse::vec3f& rotation_axis,
		gse::angular_velocity angular_speed,
		const stress_scene_params& params
	) -> void;
}

auto sandbox::spawn_elevator(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	constexpr float platform_size = 3.f;
	constexpr float platform_height = 0.3f;
	constexpr float platform_y = 3.f;
	constexpr float oscillation_amplitude = 2.5f;
	constexpr float frequency_hz = 0.4f;
	const float omega_value = 2.f * std::numbers::pi_v<float> * frequency_hz;

	const auto platform_origin = origin + gse::meters(0.f, platform_y, 0.f);
	auto platform_arch = box(
		platform_origin,
		gse::meters(platform_size, platform_height, platform_size),
		gse::kilograms(10000.f)
	);
	platform_arch.motion.body = gse::physics::kinematic_body{};

	const auto platform_id = s.spawn("Elevator Platform", std::move(platform_arch));
	s.registry().add_component<piston::component>(
		platform_id,
		{
			.center = platform_origin,
			.amplitude = gse::meters(0.f, oscillation_amplitude, 0.f),
			.omega = gse::radians_per_second(omega_value),
		}
	);
	s.registry().add_component<gse::physics::kinematic_target_component>(
		platform_id,
		{
			.position = platform_origin,
		}
	);

	constexpr int grid = 3;
	constexpr float box_size = 0.5f;
	constexpr float box_spacing = 0.7f;
	for (int i = 0; i < grid; ++i) {
		for (int j = 0; j < grid; ++j) {
			const auto pos = platform_origin + gse::meters(
				-box_spacing + static_cast<float>(i) * box_spacing,
				platform_height * 0.5f + box_size * 0.5f + 0.05f,
				-box_spacing + static_cast<float>(j) * box_spacing
			);
			s.spawn(
				std::format("Elevator Box {}{}", i, j),
				box(pos, gse::vec3<gse::length>(gse::meters(box_size)), gse::kilograms(2.f))
			);
		}
	}
}

auto sandbox::spawn_vise(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
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
		static_box(
			origin + gse::meters(0.f, floor_thickness * 0.5f, 0.f),
			gse::meters(6.f, floor_thickness, wall_depth + 2.f)
		)
	);

	constexpr std::array<std::pair<std::string_view, float>, 2> wall_defs = {
		std::pair{ std::string_view{"Right"}, 1.f },
		std::pair{ std::string_view{"Left"}, -1.f },
	};

	for (const auto& [name, sign] : wall_defs) {
		const auto wall_center = origin + gse::meters(
			sign * wall_center_offset,
			floor_thickness + wall_height * 0.5f,
			0.f
		);
		auto wall_arch = box(
			wall_center,
			gse::meters(wall_thickness, wall_height, wall_depth),
			gse::kilograms(10000.f)
		);
		wall_arch.motion.body = gse::physics::kinematic_body{};

		const auto wall_id = s.spawn(std::format("Vise {} Wall", name), std::move(wall_arch));
		s.registry().add_component<piston::component>(
			wall_id,
			{
				.center = wall_center,
				.amplitude = gse::meters(-sign * wall_amplitude, 0.f, 0.f),
				.omega = gse::radians_per_second(omega_value),
			}
		);
		s.registry().add_component<gse::physics::kinematic_target_component>(
			wall_id,
			{
				.position = wall_center,
			}
		);
	}

	s.spawn(
		"Vise Box",
		box(
			origin + gse::meters(0.f, floor_thickness + 0.3f, 0.f),
			gse::vec3<gse::length>(gse::meters(0.5f)),
			gse::kilograms(5.f)
		)
	);
}

auto sandbox::spawn_slider_elevator(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	const auto rail_pos = origin + gse::meters(0.f, 4.f, 0.f);
	const auto rail_id = s.spawn("Slider Rail", static_box(rail_pos, gse::meters(0.3f, 8.f, 0.3f)));

	const auto platform_id = s.spawn(
		"Slider Platform",
		box(
			rail_pos + gse::meters(0.f, 2.f, 0.f),
			gse::meters(2.f, 0.3f, 2.f),
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

auto sandbox::spawn_hinge_door(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	const auto frame_pos = origin + gse::meters(0.f, 2.f, 0.f);
	const auto frame_id = s.spawn("Hinge Frame", static_box(frame_pos, gse::meters(0.3f, 4.f, 0.3f)));

	auto door = box(
		frame_pos + gse::meters(1.5f, 0.f, 0.f),
		gse::meters(3.f, 3.5f, 0.2f),
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
				.anchor_a = gse::vec3<gse::displacement>(gse::meters(0.15f, 0.f, 0.f)),
				.anchor_b = gse::vec3<gse::displacement>(gse::meters(-1.5f, 0.f, 0.f)),
				.axis = { 0.f, 1.f, 0.f },
				.limits = std::pair{ gse::radians(-1.57f), gse::radians(1.57f) },
			},
		});
}

auto sandbox::spawn_funnel(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	constexpr float wall_len = 8.f;
	constexpr float wall_height = 4.f;
	constexpr float half_opening = 0.8f;
	constexpr float spread = 4.f;
	const gse::angle wall_angle = gse::radians(std::atan2(spread - half_opening, wall_len));

	const gse::quat left_rot(gse::axis_y, wall_angle);
	const gse::quat right_rot(gse::axis_y, -wall_angle);

	const float half_len = wall_len * 0.5f;
	const float mid_offset = (spread + half_opening) * 0.5f;

	s.spawn(
		"Funnel Left Wall",
		static_box(
			origin + gse::meters(-mid_offset, wall_height * 0.5f, 0.f),
			gse::meters(wall_len, wall_height, 0.3f),
			left_rot
		)
	);
	s.spawn(
		"Funnel Right Wall",
		static_box(
			origin + gse::meters(mid_offset, wall_height * 0.5f, 0.f),
			gse::meters(wall_len, wall_height, 0.3f),
			right_rot
		)
	);
	s.spawn(
		"Funnel Back Wall",
		static_box(
			origin + gse::meters(0.f, wall_height * 0.5f, -half_len),
			gse::meters(spread * 2.f + 1.f, wall_height, 0.3f)
		)
	);

	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			const auto pos = origin +
				gse::meters(
					-1.f + static_cast<float>(col) * 1.1f,
					0.5f + static_cast<float>(row) * 1.1f,
					-3.f
				);
			s.spawn(
				std::format("Funnel Box r{}c{}", row, col),
				box(
					pos,
					gse::vec3<gse::length>(gse::meters(1.f)),
					gse::kilograms(40.f)
				)
			);
		}
	}
}

auto sandbox::spawn_tumbler(gse::scene& s, const int index, const gse::vec3<gse::position>& center, const gse::vec3f& rotation_axis, const gse::angular_velocity angular_speed, const stress_scene_params& params) -> void {
	const float interior_half = static_cast<float>(params.tumbler_radial_cubes) * 0.5f + 0.5f;
	const float length_half = static_cast<float>(params.tumbler_axial_cubes) * 0.5f;
	constexpr float thickness = 0.3f;
	const float outer_half = interior_half + thickness;
	const float wall_offset = interior_half + thickness * 0.5f;
	const float side_wall_length = (length_half + thickness) * 2.f;

	struct wall_def {
		std::string_view suffix;
		gse::vec3<gse::length> local_offset;
		gse::vec3<gse::length> size;
	};

	const std::array walls = {
		wall_def{
			.suffix = "Bottom",
			.local_offset = gse::meters(0.f, -wall_offset, 0.f),
			.size = gse::meters(outer_half * 2.f, thickness, side_wall_length),
		},
		wall_def{
			.suffix = "Top",
			.local_offset = gse::meters(0.f, wall_offset, 0.f),
			.size = gse::meters(outer_half * 2.f, thickness, side_wall_length),
		},
		wall_def{
			.suffix = "Left",
			.local_offset = gse::meters(-wall_offset, 0.f, 0.f),
			.size = gse::meters(thickness, outer_half * 2.f, side_wall_length),
		},
		wall_def{
			.suffix = "Right",
			.local_offset = gse::meters(wall_offset, 0.f, 0.f),
			.size = gse::meters(thickness, outer_half * 2.f, side_wall_length),
		},
		wall_def{
			.suffix = "Front",
			.local_offset = gse::meters(0.f, 0.f, length_half + thickness * 0.5f),
			.size = gse::meters(outer_half * 2.f, outer_half * 2.f, thickness),
		},
		wall_def{
			.suffix = "Back",
			.local_offset = gse::meters(0.f, 0.f, -(length_half + thickness * 0.5f)),
			.size = gse::meters(outer_half * 2.f, outer_half * 2.f, thickness),
		},
	};

	for (const auto& wall : walls) {
		auto wall_arch = box(center + wall.local_offset, wall.size, gse::kilograms(10000.f));
		wall_arch.motion.body = gse::physics::kinematic_body{};
		wall_arch.spec.material.opacity = 0.2f;
		const auto wall_id = s.spawn(std::format("Tumbler {} Wall {}", index, wall.suffix), std::move(wall_arch));
		s.registry().add_component<tumbler::component>(
			wall_id,
			{
				.center = center,
				.axis = rotation_axis,
				.angular_speed = angular_speed,
				.local_offset = wall.local_offset,
			}
		);
		s.registry().add_component<gse::physics::kinematic_target_component>(
			wall_id,
			{
				.position = center + wall.local_offset,
			}
		);
	}

	const int nx = params.tumbler_radial_cubes;
	const int ny = params.tumbler_radial_cubes;
	const int nz = params.tumbler_axial_cubes;
	constexpr float content_size = 0.5f;
	const float radial_span = interior_half - content_size;
	const float axial_span = length_half - content_size;

	int content_id = 0;
	for (int ix = 0; ix < nx; ++ix) {
		for (int iy = 0; iy < ny; ++iy) {
			for (int iz = 0; iz < nz; ++iz) {
				const float fx = -radial_span + (static_cast<float>(ix) + 0.5f) * (radial_span * 2.f / static_cast<float>(nx));
				const float fy = -radial_span + (static_cast<float>(iy) + 0.5f) * (radial_span * 2.f / static_cast<float>(ny));
				const float fz = -axial_span + (static_cast<float>(iz) + 0.5f) * (axial_span * 2.f / static_cast<float>(nz));
				s.spawn(
					std::format("Tumbler {} Cube {}", index, content_id++),
					box(
						center + gse::meters(fx, fy, fz),
						gse::vec3<gse::length>(gse::meters(content_size)),
						gse::kilograms(1.f)
					)
				);
			}
		}
	}
}
