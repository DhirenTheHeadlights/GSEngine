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

	auto physics_parity_world_setup(
		gse::engine& e,
		std::size_t n_envs
	) -> gse::scene*;
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
