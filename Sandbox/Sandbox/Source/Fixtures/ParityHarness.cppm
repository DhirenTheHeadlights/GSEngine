export module sandbox:parity_harness;

import std;
import gse;

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
