export module sandbox:worlds;

import std;
import gse;

import :archetypes;
import :joints;
import :machines;
import :spawn_params;
import :stacks;

export namespace sandbox {
	auto sandbox_scene_setup(
		gse::scene& s
	) -> void;

	auto pyramid_scene_setup(
		gse::scene& s
	) -> void;

	auto tumbler_scene_setup(
		gse::scene& s
	) -> void;

	auto carousel_scene_setup(
		gse::scene& s
	) -> void;

	auto machines_scene_setup(
		gse::scene& s
	) -> void;

	auto dominoes_scene_setup(
		gse::scene& s
	) -> void;

	auto swings_scene_setup(
		gse::scene& s
	) -> void;

	auto sky_scene_setup(
		gse::scene& s
	) -> void;

	auto light_hall_scene_setup(
		gse::scene& s
	) -> void;
}

namespace sandbox {
	struct ground_params {
		std::string_view prefix;
		gse::vec3<gse::length> floor_size = gse::vec3<gse::length>(gse::meters(1000.f), gse::meters(1.f), gse::meters(1000.f));
		gse::length floor_depth = gse::meters(-0.5f);
		gse::length floor_visual_radius = gse::meters(1200.f);
		bool mountains = true;
		gse::length mountain_outer_radius = gse::meters(6500.f);
		gse::length mountain_peak_height = gse::meters(920.f);
		gse::vec3f mountain_color = gse::vec3f(0.21f, 0.23f, 0.27f);
		float mountain_roughness = 0.92f;
		std::uint32_t mountain_seed = 1337;
	};

	auto spawn_outdoor_ground(
		gse::scene& s,
		const ground_params& params
	) -> void;

	auto scatter_rocks(
		gse::scene& s,
		std::string_view prefix,
		int count,
		gse::length inner,
		gse::length outer,
		std::uint32_t seed
	) -> void;
}

auto sandbox::spawn_outdoor_ground(gse::scene& s, const ground_params& params) -> void {
	s.spawn(
		std::format("{}Floor", params.prefix),
		static_disc_floor(
			gse::vec3<gse::position>(gse::meters(0.f), params.floor_depth, gse::meters(0.f)),
			params.floor_size,
			params.floor_visual_radius
		)
	);

	if (!params.mountains) {
		return;
	}

	s.spawn(
		std::format("{}Mountains", params.prefix),
		mountain_ring(
			gse::meters(1200.f),
			params.mountain_outer_radius,
			params.mountain_peak_height,
			params.mountain_color,
			params.mountain_roughness,
			params.mountain_seed
		)
	);
}

auto sandbox::scatter_rocks(gse::scene& s, const std::string_view prefix, const int count, const gse::length inner, const gse::length outer, const std::uint32_t seed) -> void {
	const auto rand01 = [](std::uint32_t x) {
		x = (x ^ 61u) ^ (x >> 16);
		x *= 9u;
		x = x ^ (x >> 4);
		x *= 0x27d4eb2du;
		x = x ^ (x >> 15);
		return static_cast<float>(x) * 0x1p-32f;
	};

	for (int i = 0; i < count; ++i) {
		const auto k = seed + static_cast<std::uint32_t>(i) * 2654435761u;
		const auto theta = gse::degrees(360.f) * rand01(k);
		const auto radius = inner + (outer - inner) * rand01(k + 17u);
		const auto width = gse::meters(6.f + 12.f * rand01(k + 41u));
		const auto height = gse::meters(4.f + 10.f * rand01(k + 73u));
		const auto depth = gse::meters(6.f + 12.f * rand01(k + 97u));
		const auto tilt = gse::quat(gse::axis_y, gse::degrees(360.f) * rand01(k + 131u))
			* gse::quat(gse::axis_x, gse::degrees(-18.f) + gse::degrees(36.f) * rand01(k + 157u));

		s.spawn(
			std::format("{}_{}", prefix, i),
			scatter_rock(
				gse::vec3<gse::position>(radius * gse::sin(theta), height * 0.35f, radius * gse::cos(theta)),
				gse::vec3<gse::length>(width, height, depth),
				tilt
			)
		);
	}
}

auto sandbox::sandbox_scene_setup(gse::scene& s) -> void {
	spawn_outdoor_ground(
		s,
		{
			.prefix = "",
			.floor_depth = gse::meters(-0.501f),
		}
	);

	scatter_rocks(s, "Rock", 26, gse::meters(130.f), gse::meters(450.f), 4127u);

	s.build("Scene Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(gse::meters(0.f, 5.f, 10.f)),
		});
}

auto sandbox::pyramid_scene_setup(gse::scene& s) -> void {
	spawn_outdoor_ground(
		s,
		{
			.prefix = "Pyramid",
			.floor_visual_radius = gse::meters(700.f),
			.mountains = false,
		}
	);

	s.build("Pyramid Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(37.5f), gse::meters(120.f)),
			.priority = 60,
			.speed = gse::meters_per_second(30.f),
			.yaw = gse::degrees(0.f),
			.pitch = gse::degrees(-14.f),
			.collide_with_geometry = false,
		});
}

auto sandbox::tumbler_scene_setup(gse::scene& s) -> void {
	spawn_outdoor_ground(s, { .prefix = "Tumbler" });

	const stress_scene_params params{};

	const auto drum_height = gse::meters(13.f);
	const std::array<gse::vec3<gse::position>, 4> centres = {
		gse::vec3<gse::position>(gse::meters(-14.f), drum_height, gse::meters(-16.f)),
		gse::vec3<gse::position>(gse::meters(14.f), drum_height, gse::meters(-16.f)),
		gse::vec3<gse::position>(gse::meters(-14.f), drum_height, gse::meters(16.f)),
		gse::vec3<gse::position>(gse::meters(14.f), drum_height, gse::meters(16.f)),
	};
	const std::array speeds = {
		gse::radians_per_second(0.60f),
		gse::radians_per_second(-0.48f),
		gse::radians_per_second(0.42f),
		gse::radians_per_second(-0.70f),
	};

	for (int i = 0; i < 4; ++i) {
		spawn_tumbler(s, i, centres[i], gse::axis_z, speeds[i], params);
	}

	s.build("Tumbler Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(50.f), gse::meters(90.f)),
			.priority = 60,
			.speed = gse::meters_per_second(30.f),
			.yaw = gse::degrees(0.f),
			.pitch = gse::degrees(-20.f),
			.collide_with_geometry = false,
		});
}

auto sandbox::carousel_scene_setup(gse::scene& s) -> void {
	spawn_outdoor_ground(s, { .prefix = "Carousel" });

	const stress_scene_params drum{
		.tumbler_radial_cubes = 10,
		.tumbler_axial_cubes = 20,
	};
	const stress_scene_params counter_drum{
		.tumbler_radial_cubes = 5,
		.tumbler_axial_cubes = 10,
	};

	spawn_tumbler(
		s,
		0,
		gse::vec3<gse::position>(gse::meters(0.f), gse::meters(7.f), gse::meters(0.f)),
		gse::axis_x,
		gse::radians_per_second(0.35f),
		drum
	);
	spawn_tumbler(
		s,
		1,
		gse::vec3<gse::position>(gse::meters(24.f), gse::meters(4.f), gse::meters(0.f)),
		gse::axis_z,
		gse::radians_per_second(-0.9f),
		counter_drum
	);

	s.build("Carousel Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(gse::meters(6.f), gse::meters(16.f), gse::meters(44.f)),
			.priority = 60,
			.speed = gse::meters_per_second(24.f),
			.yaw = gse::degrees(0.f),
			.pitch = gse::degrees(-12.f),
			.collide_with_geometry = false,
		});
}

auto sandbox::machines_scene_setup(gse::scene& s) -> void {
	spawn_outdoor_ground(s, { .prefix = "Machines" });

	spawn_elevator(s, gse::vec3<gse::position>(gse::meters(-18.f, 0.f, 0.f)));
	spawn_vise(s, gse::vec3<gse::position>(gse::meters(0.f, 0.f, -14.f)));
	spawn_slider_elevator(s, gse::vec3<gse::position>(gse::meters(14.f, 0.f, 0.f)));
	spawn_hinge_door(s, gse::vec3<gse::position>(gse::meters(0.f, 0.f, 8.f)));
	spawn_funnel(s, gse::vec3<gse::position>(gse::meters(26.f, 0.f, -16.f)));

	s.build("Machines Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(9.f), gse::meters(34.f)),
			.priority = 60,
			.speed = gse::meters_per_second(18.f),
			.yaw = gse::degrees(0.f),
			.pitch = gse::degrees(-10.f),
			.collide_with_geometry = false,
		});
}

auto sandbox::dominoes_scene_setup(gse::scene& s) -> void {
	spawn_outdoor_ground(s, { .prefix = "Dominoes" });

	constexpr int spokes = 6;
	const auto hub_radius = gse::meters(4.f);

	for (int i = 0; i < spokes; ++i) {
		const auto bearing = gse::degrees(360.f) * (static_cast<float>(i) / static_cast<float>(spokes));
		const auto origin = gse::vec3<gse::position>(
			hub_radius * gse::sin(bearing),
			gse::meters(0.f),
			hub_radius * gse::cos(bearing)
		);
		spawn_domino_chain(s, i + 1, origin, bearing - gse::degrees(90.f));
	}

	s.build("Dominoes Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(20.f), gse::meters(26.f)),
			.priority = 60,
			.speed = gse::meters_per_second(18.f),
			.yaw = gse::degrees(0.f),
			.pitch = gse::degrees(-34.f),
			.collide_with_geometry = false,
		});
}

auto sandbox::swings_scene_setup(gse::scene& s) -> void {
	spawn_outdoor_ground(s, { .prefix = "Swings" });

	for (int i = 0; i < 3; ++i) {
		const auto origin = gse::vec3<gse::position>(
			gse::meters(static_cast<float>(i - 1) * 8.f),
			gse::meters(0.f),
			gse::meters(0.f)
		);
		spawn_pendulum_chain(s, i + 1, origin);
	}

	spawn_spring_tests(s, gse::vec3<gse::position>(gse::meters(-24.f, 0.f, -16.f)));
	spawn_high_speed_impact_target(s, gse::vec3<gse::position>(gse::meters(12.f, 0.f, 6.f)));

	s.build("Swings Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(11.f), gse::meters(30.f)),
			.priority = 60,
			.speed = gse::meters_per_second(18.f),
			.yaw = gse::degrees(0.f),
			.pitch = gse::degrees(-12.f),
			.collide_with_geometry = false,
		});
}

auto sandbox::sky_scene_setup(gse::scene& s) -> void {
	spawn_outdoor_ground(
		s,
		{
			.prefix = "Sky",
			.floor_size = gse::vec3<gse::length>(gse::kilometers(40.f), gse::meters(1.f), gse::kilometers(40.f)),
			.mountain_outer_radius = gse::meters(6000.f),
			.mountain_peak_height = gse::meters(1030.f),
			.mountain_color = gse::vec3f(0.19f, 0.21f, 0.26f),
			.mountain_seed = 8821u,
		}
	);

	constexpr int monolith_count = 9;
	const auto ring_radius = gse::meters(34.f);
	for (int i = 0; i < monolith_count; ++i) {
		const auto theta = gse::degrees(360.f) * (static_cast<float>(i) / static_cast<float>(monolith_count));
		const auto height = gse::meters(8.f + 4.f * static_cast<float>((i * 4) % 5));
		s.spawn(
			std::format("SkyMonolith_{}", i),
			static_box(
				gse::vec3<gse::position>(ring_radius * gse::sin(theta), height * 0.5f, ring_radius * gse::cos(theta)),
				gse::vec3<gse::length>(gse::meters(2.5f), height, gse::meters(2.5f)),
				gse::quat(1.f, 0.f, 0.f, 0.f),
				gse::vec3f(0.10f, 0.10f, 0.11f),
				0.6f,
				0.0f
			)
		);
	}

	scatter_rocks(s, "SkyRock", 26, gse::meters(130.f), gse::meters(450.f), 9043u);

	s.build("Sky Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(4.f), gse::meters(70.f)),
			.priority = 60,
			.speed = gse::meters_per_second(30.f),
			.yaw = gse::degrees(0.f),
			.pitch = gse::degrees(0.f),
			.collide_with_geometry = false,
		});
}

auto sandbox::light_hall_scene_setup(gse::scene& s) -> void {
	spawn_outdoor_ground(
		s,
		{
			.prefix = "LightHall",
			.mountain_outer_radius = gse::meters(6000.f),
			.mountain_peak_height = gse::meters(1030.f),
			.mountain_color = gse::vec3f(0.19f, 0.21f, 0.26f),
			.mountain_seed = 8821u,
		}
	);

	constexpr int columns = 7;
	const auto spacing = gse::meters(16.f);
	const auto pillar_width = gse::meters(3.f);
	const auto span = spacing * (static_cast<float>(columns - 1) * 0.5f);

	for (int x = 0; x < columns; ++x) {
		for (int z = 0; z < columns; ++z) {
			const auto height = gse::meters(24.f + 7.f * static_cast<float>((x * 3 + z * 5) % 4));
			s.spawn(
				std::format("HallPillar_{}_{}", x, z),
				static_box(
					gse::vec3<gse::position>(
						spacing * static_cast<float>(x) - span,
						height * 0.5f,
						spacing * static_cast<float>(z) - span
					),
					gse::vec3<gse::length>(pillar_width, height, pillar_width),
					gse::quat(1.f, 0.f, 0.f, 0.f),
					gse::vec3f(0.09f, 0.09f, 0.10f),
					0.65f,
					0.0f
				)
			);
		}
	}

	s.build("Light Hall Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(6.f), gse::meters(74.f)),
			.priority = 60,
			.speed = gse::meters_per_second(24.f),
			.yaw = gse::degrees(0.f),
			.pitch = gse::degrees(-4.f),
			.collide_with_geometry = false,
		});
}
