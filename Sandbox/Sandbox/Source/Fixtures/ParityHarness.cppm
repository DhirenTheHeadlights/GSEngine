export module sandbox:parity_harness;

import std;
import gse;

import :archetypes;

export namespace sandbox {
	auto physics_parity_world_setup(
		gse::engine& e,
		std::size_t n_envs
	) -> gse::scene*;
}

export namespace sandbox::physics_parity {
	struct [[= gse::system_state<"Physics Parity">{}]]
	data {
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
	[[nodiscard]] auto physics_parity_scene_setup(
		std::size_t n_envs
	) -> gse::scene::setup_fn;
}

auto sandbox::physics_parity_scene_setup(const std::size_t n_envs) -> gse::scene::setup_fn {
	return [n_envs](gse::scene& s) -> void {
		const auto body_size = gse::vec3<gse::length>(gse::meters(0.4f), gse::meters(0.4f), gse::meters(0.4f));

		if (n_envs <= 1) {
			s.spawn(
				"Floor",
				static_box(
					gse::vec3<gse::position>(gse::meters(0.f, -0.5f, 0.f)),
					gse::vec3<gse::length>(gse::meters(50.f), gse::meters(1.f), gse::meters(50.f))
				)
			);

			s.spawn(
				"parity_body",
				box(
					gse::vec3<gse::position>(gse::meters(0.f), gse::meters(1.005f), gse::meters(0.f)),
					body_size
				)
			);
			return;
		}

		const auto grid_side = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(n_envs))));
		constexpr float spacing = 8.0f;
		const float floor_span = static_cast<float>(grid_side) * spacing + 20.f;

		s.spawn(
			"Floor",
			static_box(
				gse::vec3<gse::position>(gse::meters(0.f, -0.5f, 0.f)),
				gse::vec3<gse::length>(gse::meters(floor_span), gse::meters(1.f), gse::meters(floor_span))
			)
		);

		for (std::size_t i = 0; i < n_envs; ++i) {
			const auto row = static_cast<int>(i) / grid_side;
			const auto col = static_cast<int>(i) % grid_side;
			const auto x = (static_cast<float>(col) - static_cast<float>(grid_side - 1) * 0.5f) * spacing;
			const auto z = (static_cast<float>(row) - static_cast<float>(grid_side - 1) * 0.5f) * spacing;

			s.spawn(
				std::format("parity_body_{}", i),
				box(
					gse::vec3<gse::position>(gse::meters(x), gse::meters(1.005f), gse::meters(z)),
					body_size
				)
			);
		}
	};
}

auto sandbox::physics_parity_world_setup(gse::engine& e, const std::size_t n_envs) -> gse::scene* {
	return gse::add_scene(e.world(), e.registry(), "PhysicsParity", physics_parity_scene_setup(n_envs));
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

	const auto owners = motions.owner_ids();
	for (std::size_t i = 0; i < owners.size(); ++i) {
		const auto* tc = transforms.find(owners[i]);
		const auto* mc = motions.find(owners[i]);
		if (!tc || !mc) {
			continue;
		}
		d.hash = gse::hash_of(d.hash, tc->position, mc->current_velocity);
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
