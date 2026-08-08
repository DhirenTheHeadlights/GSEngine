export module sandbox:sandbox_scene;

import std;
import gse;

import :entity_builders;

export namespace sandbox::player {
	struct [[= gse::system_state<"Player">{}]] data {};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
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

	auto parity_pile_scene_setup(
		gse::scene& s
	) -> void;

	auto parity_overlap_scene_setup(
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
		gse::shared_view<gse::world_system::data> world_d,
		gse::read<gse::physics::transform_component> transforms,
		gse::read<gse::physics::motion_component> motions
	) -> gse::async::task<>;
}

auto sandbox::player::run(gse::context& ctx, data& d, gse::structural<gse::free_camera::component> cameras) -> gse::async::task<> {
	for (const auto& request : ctx.read_channel<gse::world_system::possess_player_request>()) {
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
		sandbox::static_box(
			gse::vec3<gse::position>(0.f, -0.501f, 0.f),
			floor_size,
			gse::quat(1.f, 0.f, 0.f, 0.f),
			gse::vec3f(0.08f, 0.08f, 0.09f),
			0.45f,
			0.0f
		)
	);

	s.build("Scene Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(0.f, 5.f, 10.f),
		});
}

auto sandbox::pyramid_scene_setup(gse::scene& s) -> void {
	constexpr int base_count = 50;
	const gse::length extent = gse::meters(1.f);
	const gse::length floor_span = extent * static_cast<float>(base_count) + gse::meters(24.f);

	s.spawn(
		"PyramidFloor",
		static_box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			gse::vec3<gse::length>(floor_span, gse::meters(1.f), floor_span),
			gse::quat(1.f, 0.f, 0.f, 0.f),
			gse::vec3f(0.08f, 0.08f, 0.09f),
			0.45f,
			0.0f
		)
	);

	int blocks = 0;
	for (int row = 0; row < base_count; ++row) {
		const int count = base_count - row;
		const gse::length py = extent * (static_cast<float>(row) + 0.5f);

		for (int col = 0; col < count; ++col) {
			const gse::length px = extent * (static_cast<float>(col) - static_cast<float>(count - 1) * 0.5f);
			s.spawn(
				std::format("PyramidBlock_{}_{}", row, col),
				box(
					gse::vec3<gse::position>(px, py, gse::meters(0.f)),
					gse::vec3<gse::length>(extent),
					gse::kilograms(20.f)
				)
			);
			++blocks;
		}
	}

	const gse::length apex = extent * static_cast<float>(base_count);

	s.build("Pyramid Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(gse::meters(0.f), apex * 0.75f, apex * 2.4f),
			.priority = 60,
			.speed = gse::meters_per_second(30.f),
			.yaw = gse::degrees(0.f),
			.pitch = gse::degrees(-14.f),
			.collide_with_geometry = false,
		});

	gse::log::println("pyramid: base={} blocks={} height={:.1f:m}", base_count, blocks, apex);
}


namespace sandbox {
	inline std::size_t g_physics_parity_n_envs = 1;
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

auto sandbox::physics_parity::run(gse::context& ctx, data& d, gse::shared_view<gse::world_system::data> world_d, gse::read<gse::physics::transform_component> transforms, gse::read<gse::physics::motion_component> motions) -> gse::async::task<> {
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
		ctx.channels.push<gse::activate_scene_request>({ .scene_id = *d.scene_id });
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
