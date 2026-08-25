export module sandbox:sandbox_scene;

import std;
import gse;

import :dev_spawn_system;
import :entity_builders;
import :runtime_spawns;

export namespace sandbox::player {
	struct [[= gse::system_state<"Player">{}]] data {
		bool character_requested = false;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::channel_read<gse::world_system::possess_player_request> possess_in,
		gse::channel_write<sandbox::spawn_character_request> spawn_out,
		gse::shared_view<gse::network::data> net_d,
		const gse::network::config& net_cfg,
		gse::structural<gse::free_camera::component> cameras
	) -> gse::async::task<>;
}

export namespace sandbox {
	auto sandbox_scene_setup(
		gse::scene& s
	) -> void;

	auto pyramid_scene_setup(
		gse::scene& s
	) -> void;

	auto sky_scene_setup(
		gse::scene& s
	) -> void;

	auto tumbler_scene_setup(
		gse::scene& s
	) -> void;

	auto physics_parity_world_setup(
		gse::engine& e,
		std::size_t n_envs
	) -> gse::scene*;

	auto parity_drop_scene_setup(
		gse::scene& s
	) -> void;

	auto parity_pair_scene_setup(
		gse::scene& s
	) -> void;

	auto parity_stack_scene_setup(
		gse::scene& s
	) -> void;

	auto parity_cluster_scene_setup(
		gse::scene& s
	) -> void;

	auto parity_heap_scene_setup(
		gse::scene& s
	) -> void;

	auto parity_mound_scene_setup(
		gse::scene& s
	) -> void;

	auto parity_pile_scene_setup(
		gse::scene& s
	) -> void;

	auto parity_hull_pile_scene_setup(
		gse::scene& s
	) -> void;

	auto parity_overlap_scene_setup(
		gse::scene& s
	) -> void;

	auto parity_shapes_scene_setup(
		gse::scene& s
	) -> void;

	auto parity_domino_scene_setup(
		gse::scene& s
	) -> void;
}

export namespace sandbox::physics_parity {
	struct [[= gse::system_state<"Physics Parity">{}]] data {
		int steps_run = 0;
		int max_steps = 200;
		std::uint64_t hash = 1469598103934665603ull;
		std::optional<gse::id> scene_id;
		bool done = false;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::channel_write<gse::activate_scene_request> scene_out,
		gse::shared_view<gse::world_system::data> world_d,
		gse::read<gse::physics::transform_component> transforms,
		gse::read<gse::physics::motion_component> motions
	) -> gse::async::task<>;
}

namespace sandbox {
	auto scatter_rocks(
		gse::scene& s,
		std::string_view prefix,
		int count,
		gse::length inner,
		gse::length outer,
		std::uint32_t seed
	) -> void;
}

auto sandbox::player::run(gse::context& ctx, data& d, const gse::channel_read<gse::world_system::possess_player_request> possess_in, const gse::channel_write<sandbox::spawn_character_request> spawn_out, const gse::shared_view<gse::network::data> net_d, const gse::network::config& net_cfg, gse::structural<gse::free_camera::component> cameras) -> gse::async::task<> {
	const bool server_owns_player = !net_cfg.connect.empty();

	if (server_owns_player) {
		if (!d.character_requested && net_d.connection_state == gse::network::client::state::connected) {
			d.character_requested = true;
			spawn_out.push<sandbox::spawn_character_request>({
				.count = 1,
			});
		}
		return {};
	}

	for (const auto& request : possess_in.of<gse::world_system::possess_player_request>()) {
		cameras.add(
			request.entity,
			{
				.initial_position = gse::vec3<gse::position>(0.f, 2.f, 0.f),
			}
		);
	}

	return {};
}

auto sandbox::sandbox_scene_setup(gse::scene& s) -> void {
	constexpr auto floor_size = gse::vec3<gse::length>(gse::meters(1000.f), gse::meters(1.f), gse::meters(1000.f));
	s.spawn(
		"Floor",
		static_disc_floor(
			gse::vec3<gse::position>(0.f, -0.501f, 0.f),
			floor_size,
			gse::meters(1200.f)
		)
	);

	s.spawn(
		"Mountains",
		mountain_ring(
			gse::meters(1200.f),
			gse::meters(6500.f),
			gse::meters(920.f)
		)
	);

	scatter_rocks(s, "Rock", 26, gse::meters(130.f), gse::meters(450.f), 4127u);

	s.build("Scene Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(0.f, 5.f, 10.f),
		});
}

auto sandbox::tumbler_scene_setup(gse::scene& s) -> void {
	constexpr auto floor_size = gse::vec3<gse::length>(gse::meters(1000.f), gse::meters(1.f), gse::meters(1000.f));
	s.spawn(
		"TumblerFloor",
		static_disc_floor(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			floor_size,
			gse::meters(1200.f)
		)
	);

	s.spawn(
		"TumblerMountains",
		mountain_ring(
			gse::meters(1200.f),
			gse::meters(6500.f),
			gse::meters(920.f)
		)
	);

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

auto sandbox::pyramid_scene_setup(gse::scene& s) -> void {
	constexpr auto floor_size = gse::vec3<gse::length>(gse::meters(1000.f), gse::meters(1.f), gse::meters(1000.f));
	s.spawn(
		"PyramidFloor",
		static_disc_floor(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			floor_size,
			gse::meters(700.f)
		)
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

auto sandbox::sky_scene_setup(gse::scene& s) -> void {
	constexpr auto floor_size = gse::vec3<gse::length>(gse::kilometers(40.f), gse::meters(1.f), gse::kilometers(40.f));
	s.spawn(
		"SkyFloor",
		static_disc_floor(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			floor_size,
			gse::meters(1200.f)
		)
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

	s.spawn(
		"SkyMountains",
		mountain_ring(
			gse::meters(1200.f),
			gse::meters(6000.f),
			gse::meters(1030.f),
			gse::vec3f(0.19f, 0.21f, 0.26f),
			0.92f,
			8821u
		)
	);

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


namespace sandbox {
	inline std::size_t g_physics_parity_n_envs = 1;

	auto spawn_parity_grid(
		gse::scene& s,
		int side,
		float spacing,
		std::string_view prefix
	) -> void;
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

auto physics_parity_scene_setup(gse::scene& s) -> void {
	const auto n = sandbox::g_physics_parity_n_envs;

	if (n <= 1) {
		constexpr auto floor_size = gse::vec3<gse::length>(gse::meters(50.f), gse::meters(1.f), gse::meters(50.f));
		s.spawn(
			"Floor",
			sandbox::static_box(
				gse::vec3<gse::position>(0.f, -0.5f, 0.f),
				floor_size
			)
		);

		s.spawn(
			"parity_body",
			sandbox::box(
				gse::vec3<gse::position>(gse::meters(0.f), gse::meters(1.005f), gse::meters(0.f)),
				gse::vec3<gse::length>(gse::meters(0.4f), gse::meters(0.4f), gse::meters(0.4f))
			)
		);
		return;
	}

	const auto grid_side = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(n))));
	const auto spacing = 8.0f;
	const auto floor_span = static_cast<float>(grid_side) * spacing + 20.f;
	const auto floor_size = gse::vec3<gse::length>(gse::meters(floor_span), gse::meters(1.f), gse::meters(floor_span));
	s.spawn(
		"Floor",
		sandbox::static_box(
			gse::vec3<gse::position>(0.f, -0.5f, 0.f),
			floor_size
		)
	);

	for (std::size_t i = 0; i < n; ++i) {
		const auto row = static_cast<int>(i) / grid_side;
		const auto col = static_cast<int>(i) % grid_side;
		const auto x = (static_cast<float>(col) - static_cast<float>(grid_side - 1) * 0.5f) * spacing;
		const auto z = (static_cast<float>(row) - static_cast<float>(grid_side - 1) * 0.5f) * spacing;

		s.spawn(
			std::format("parity_body_{}", i),
			sandbox::box(
				gse::vec3<gse::position>(gse::meters(x), gse::meters(1.005f), gse::meters(z)),
				gse::vec3<gse::length>(gse::meters(0.4f), gse::meters(0.4f), gse::meters(0.4f))
			)
		);
	}
}

auto sandbox::parity_drop_scene_setup(gse::scene& s) -> void {
	s.spawn(
		"ParityFloor",
		static_box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(20.f), gse::meters(1.f), gse::meters(20.f))
		)
	);

	s.spawn(
		"ParityFaller",
		box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(2.f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(0.5f), gse::meters(0.5f), gse::meters(0.5f))
		)
	);
}

auto sandbox::parity_pair_scene_setup(gse::scene& s) -> void {
	s.spawn(
		"ParityFloor",
		static_box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(20.f), gse::meters(1.f), gse::meters(20.f))
		)
	);

	s.spawn(
		"ParityResting",
		box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(0.5f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(0.5f), gse::meters(0.5f), gse::meters(0.5f))
		)
	);

	s.spawn(
		"ParityFaller",
		box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(3.f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(0.5f), gse::meters(0.5f), gse::meters(0.5f))
		)
	);
}

auto sandbox::parity_stack_scene_setup(gse::scene& s) -> void {
	s.spawn(
		"ParityFloor",
		static_box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(20.f), gse::meters(1.f), gse::meters(20.f))
		)
	);

	for (int i = 0; i < 8; ++i) {
		s.spawn(
			std::format("ParityStack_{}", i),
			box(
				gse::vec3<gse::position>(gse::meters(0.f), gse::meters(0.5f + static_cast<float>(i) * 1.02f), gse::meters(0.f)),
				gse::vec3<gse::length>(gse::meters(0.5f), gse::meters(0.5f), gse::meters(0.5f))
			)
		);
	}
}

auto sandbox::parity_shapes_scene_setup(gse::scene& s) -> void {
	s.spawn(
		"ParityFloor",
		static_box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(60.f), gse::meters(1.f), gse::meters(20.f))
		)
	);

	const auto axis_along_z = gse::quat(gse::axis_x, gse::degrees(90.f));
	const auto axis_along_x = gse::quat(gse::axis_z, gse::degrees(90.f));

	const auto unit_box = gse::vec3<gse::length>(gse::meters(1.f), gse::meters(1.f), gse::meters(1.f));
	const auto sphere_radius = gse::meters(0.5f);
	const auto capsule_radius = gse::meters(0.35f);
	const auto capsule_half_height = gse::meters(0.5f);

	s.spawn(
		"ParityShapesBoxBox_support",
		static_box(gse::vec3<gse::position>(gse::meters(-15.f), gse::meters(0.5f), gse::meters(0.f)), unit_box)
	);
	s.spawn(
		"ParityShapesBoxBox_body",
		box(gse::vec3<gse::position>(gse::meters(-15.f), gse::meters(1.55f), gse::meters(0.f)), unit_box)
	);

	s.spawn(
		"ParityShapesHullBox_support",
		static_box(gse::vec3<gse::position>(gse::meters(-21.f), gse::meters(0.5f), gse::meters(0.f)), unit_box)
	);
	s.spawn(
		"ParityShapesHullBox_body",
		box_as_hull(gse::vec3<gse::position>(gse::meters(-21.f), gse::meters(1.55f), gse::meters(0.f)), unit_box)
	);

	s.spawn(
		"ParityShapesBoxSphere_support",
		static_box(gse::vec3<gse::position>(gse::meters(-9.f), gse::meters(0.5f), gse::meters(0.f)), unit_box)
	);
	s.spawn(
		"ParityShapesBoxSphere_body",
		sphere(gse::vec3<gse::position>(gse::meters(-9.f), gse::meters(1.55f), gse::meters(0.f)), sphere_radius)
	);

	s.spawn(
		"ParityShapesBoxCapsule_support",
		static_box(
			gse::vec3<gse::position>(gse::meters(-3.f), gse::meters(0.5f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(1.6f), gse::meters(1.f), gse::meters(1.6f))
		)
	);
	s.spawn(
		"ParityShapesBoxCapsule_body",
		capsule(
			gse::vec3<gse::position>(gse::meters(-3.f), gse::meters(1.4f), gse::meters(0.f)),
			capsule_radius,
			capsule_half_height,
			gse::kilograms(1000.f),
			axis_along_z
		)
	);

	s.spawn(
		"ParityShapesSphereSphere_support_a",
		static_sphere(gse::vec3<gse::position>(gse::meters(2.45f), gse::meters(0.5f), gse::meters(0.f)), sphere_radius)
	);
	s.spawn(
		"ParityShapesSphereSphere_support_b",
		static_sphere(gse::vec3<gse::position>(gse::meters(3.55f), gse::meters(0.5f), gse::meters(0.f)), sphere_radius)
	);
	s.spawn(
		"ParityShapesSphereSphere_body",
		sphere(gse::vec3<gse::position>(gse::meters(3.f), gse::meters(1.4f), gse::meters(0.f)), sphere_radius)
	);

	s.spawn(
		"ParityShapesSphereCapsule_support_a",
		static_capsule(
			gse::vec3<gse::position>(gse::meters(8.55f), gse::meters(0.5f), gse::meters(0.f)),
			capsule_radius,
			capsule_half_height,
			axis_along_z
		)
	);
	s.spawn(
		"ParityShapesSphereCapsule_support_b",
		static_capsule(
			gse::vec3<gse::position>(gse::meters(9.45f), gse::meters(0.5f), gse::meters(0.f)),
			capsule_radius,
			capsule_half_height,
			axis_along_z
		)
	);
	s.spawn(
		"ParityShapesSphereCapsule_body",
		sphere(gse::vec3<gse::position>(gse::meters(9.f), gse::meters(1.28f), gse::meters(0.f)), sphere_radius)
	);

	s.spawn(
		"ParityShapesCapsuleCapsule_support_a",
		static_capsule(
			gse::vec3<gse::position>(gse::meters(14.55f), gse::meters(0.5f), gse::meters(0.f)),
			capsule_radius,
			capsule_half_height,
			axis_along_z
		)
	);
	s.spawn(
		"ParityShapesCapsuleCapsule_support_b",
		static_capsule(
			gse::vec3<gse::position>(gse::meters(15.45f), gse::meters(0.5f), gse::meters(0.f)),
			capsule_radius,
			capsule_half_height,
			axis_along_z
		)
	);
	s.spawn(
		"ParityShapesCapsuleCapsule_body",
		capsule(
			gse::vec3<gse::position>(gse::meters(15.f), gse::meters(1.26f), gse::meters(0.f)),
			capsule_radius,
			capsule_half_height,
			gse::kilograms(1000.f),
			axis_along_x
		)
	);
}

auto sandbox::parity_domino_scene_setup(gse::scene& s) -> void {
	s.spawn(
		"ParityFloor",
		static_box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(20.f), gse::meters(1.f), gse::meters(20.f))
		)
	);

	spawn_domino_chain(s, gse::vec3<gse::position>(-5.f, 0.f, 0.f));
}

auto sandbox::spawn_parity_grid(gse::scene& s, const int side, const float spacing, const std::string_view prefix) -> void {
	for (int x = 0; x < side; ++x) {
		for (int y = 0; y < side; ++y) {
			for (int z = 0; z < side; ++z) {
				const float px = (static_cast<float>(x) - static_cast<float>(side - 1) * 0.5f) * spacing;
				const float pz = (static_cast<float>(z) - static_cast<float>(side - 1) * 0.5f) * spacing;
				const float py = 0.5f + static_cast<float>(y) * spacing;
				s.spawn(
					std::format("{}_{}_{}_{}", prefix, x, y, z),
					box(
						gse::vec3<gse::position>(gse::meters(px), gse::meters(py), gse::meters(pz)),
						gse::vec3<gse::length>(gse::meters(0.5f), gse::meters(0.5f), gse::meters(0.5f))
					)
				);
			}
		}
	}
}

auto sandbox::parity_cluster_scene_setup(gse::scene& s) -> void {
	s.spawn(
		"ParityFloor",
		static_box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(40.f), gse::meters(1.f), gse::meters(40.f))
		)
	);

	spawn_parity_grid(s, 3, 0.75f, "ParityCluster");
}

auto sandbox::parity_heap_scene_setup(gse::scene& s) -> void {
	s.spawn(
		"ParityFloor",
		static_box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(40.f), gse::meters(1.f), gse::meters(40.f))
		)
	);

	spawn_parity_grid(s, 4, 0.75f, "ParityHeap");
}

auto sandbox::parity_mound_scene_setup(gse::scene& s) -> void {
	s.spawn(
		"ParityFloor",
		static_box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(40.f), gse::meters(1.f), gse::meters(40.f))
		)
	);

	spawn_parity_grid(s, 5, 0.75f, "ParityMound");
}

auto sandbox::parity_pile_scene_setup(gse::scene& s) -> void {
	s.spawn(
		"ParityFloor",
		static_box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(40.f), gse::meters(1.f), gse::meters(40.f))
		)
	);

	constexpr int side = 6;
	constexpr float spacing = 0.75f;

	for (int x = 0; x < side; ++x) {
		for (int y = 0; y < side; ++y) {
			for (int z = 0; z < side; ++z) {
				const float px = (static_cast<float>(x) - static_cast<float>(side - 1) * 0.5f) * spacing;
				const float pz = (static_cast<float>(z) - static_cast<float>(side - 1) * 0.5f) * spacing;
				const float py = 0.5f + static_cast<float>(y) * spacing;
				s.spawn(
					std::format("ParityPile_{}_{}_{}", x, y, z),
					box(
						gse::vec3<gse::position>(gse::meters(px), gse::meters(py), gse::meters(pz)),
						gse::vec3<gse::length>(gse::meters(0.5f), gse::meters(0.5f), gse::meters(0.5f))
					)
				);
			}
		}
	}
}

auto sandbox::parity_hull_pile_scene_setup(gse::scene& s) -> void {
	s.spawn(
		"ParityFloor",
		static_box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(40.f), gse::meters(1.f), gse::meters(40.f))
		)
	);

	constexpr int side = 6;
	constexpr float spacing = 0.75f;

	for (int x = 0; x < side; ++x) {
		for (int y = 0; y < side; ++y) {
			for (int z = 0; z < side; ++z) {
				const float px = (static_cast<float>(x) - static_cast<float>(side - 1) * 0.5f) * spacing;
				const float pz = (static_cast<float>(z) - static_cast<float>(side - 1) * 0.5f) * spacing;
				const float py = 0.5f + static_cast<float>(y) * spacing;
				s.spawn(
					std::format("ParityHullPile_{}_{}_{}", x, y, z),
					box_as_hull(
						gse::vec3<gse::position>(gse::meters(px), gse::meters(py), gse::meters(pz)),
						gse::vec3<gse::length>(gse::meters(0.5f), gse::meters(0.5f), gse::meters(0.5f))
					)
				);
			}
		}
	}
}

auto sandbox::parity_overlap_scene_setup(gse::scene& s) -> void {
	s.spawn(
		"ParityFloor",
		static_box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(40.f), gse::meters(1.f), gse::meters(40.f))
		)
	);

	constexpr int side = 6;
	constexpr float spacing = 0.4f;

	for (int x = 0; x < side; ++x) {
		for (int y = 0; y < side; ++y) {
			for (int z = 0; z < side; ++z) {
				const float px = (static_cast<float>(x) - static_cast<float>(side - 1) * 0.5f) * spacing;
				const float pz = (static_cast<float>(z) - static_cast<float>(side - 1) * 0.5f) * spacing;
				const float py = 0.5f + static_cast<float>(y) * spacing;
				s.spawn(
					std::format("ParityOverlap_{}_{}_{}", x, y, z),
					box(
						gse::vec3<gse::position>(gse::meters(px), gse::meters(py), gse::meters(pz)),
						gse::vec3<gse::length>(gse::meters(0.5f), gse::meters(0.5f), gse::meters(0.5f))
					)
				);
			}
		}
	}
}

auto sandbox::physics_parity_world_setup(gse::engine& e, const std::size_t n_envs) -> gse::scene* {
	g_physics_parity_n_envs = n_envs;
	auto& w = e.world();
	auto& reg = e.registry();
	return gse::add_scene(w, reg, "PhysicsParity", &physics_parity_scene_setup);
}

auto sandbox::physics_parity::run(gse::context& ctx, data& d, const gse::channel_write<gse::activate_scene_request> scene_out, gse::shared_view<gse::world_system::data> world_d, gse::read<gse::physics::transform_component> transforms, gse::read<gse::physics::motion_component> motions) -> gse::async::task<> {
	if (d.done) {
		return {};
	}

	if (!d.scene_id.has_value()) {
		for (const auto& scene_id : world_d.scene_ids) {
			if (scene_id.tag() == std::string_view("PhysicsParity")) {
				d.scene_id = scene_id;
				break;
			}
		}
	}

	if (d.scene_id.has_value() && world_d.active_scene != d.scene_id) {
		scene_out.push<gse::activate_scene_request>({ .scene_id = *d.scene_id });
		return {};
	}

	if (motions.empty()) {
		return {};
	}

	const auto fold = [&](const auto& v) {
		d.hash = gse::hash_combine(d.hash, v);
	};

	const auto owners = motions.owner_ids();
	for (std::size_t i = 0; i < owners.size(); ++i) {
		const auto* tc = transforms.find(owners[i]);
		const auto* mc = motions.find(owners[i]);
		if (!tc || !mc) {
			continue;
		}
		fold(tc->position.x());
		fold(tc->position.y());
		fold(tc->position.z());
		fold(mc->current_velocity.x());
		fold(mc->current_velocity.y());
		fold(mc->current_velocity.z());
	}

	if (d.steps_run % 10 == 0 && owners.size() > 4) {
		const auto* t1 = transforms.find(owners[1]);
		const auto* t4 = transforms.find(owners[4]);
		if (t1 && t4) {
			gse::log::println("physics_parity: step={} body1={:.3f} body4={:.3f}", d.steps_run, t1->position, t4->position);
		}
	}

	++d.steps_run;

	if (d.steps_run >= d.max_steps) {
		gse::log::println("physics_parity: steps={} bodies={} hash={:016x}", d.steps_run, owners.size(), d.hash);
		for (std::size_t i = 0; i < owners.size(); ++i) {
			const auto* tc = transforms.find(owners[i]);
			if (!tc) {
				continue;
			}
			gse::log::println("physics_parity: body[{}] pos={:.5f}", i, tc->position);
		}
		d.done = true;
		gse::shutdown();
	}

	return {};
}
