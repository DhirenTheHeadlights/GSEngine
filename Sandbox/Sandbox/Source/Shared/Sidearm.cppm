export module sandbox:sidearm;

import std;
import gse;

import :character_controller;
import :orbit_camera;

export namespace sandbox::sidearm {
	struct component {
		gse::length muzzle_height = gse::meters(0.55f);
		gse::length muzzle_forward = gse::meters(0.55f);
		gse::length muzzle_side = gse::meters(0.22f);
		gse::length round_radius = gse::meters(0.09f);
		gse::mass round_mass = gse::kilograms(2.f);
		gse::velocity muzzle_speed = gse::meters_per_second(45.f);
		gse::time refire_delay = gse::seconds(0.18f);
		gse::time round_lifetime = gse::seconds(6.f);
		gse::time cooldown = gse::seconds(0.f);
	};

	struct live_round {
		gse::id entity;
		gse::time remaining;
	};

	struct [[= gse::system_state<"Sidearm">{}]] data {
		std::vector<live_round> rounds;
		std::uint64_t fired = 0;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::shared_view<gse::input::data> input_s,
		gse::shared_view<gse::camera::data> cam_s,
		gse::shared_view<gse::physics::data> phys_s,
		gse::shared_view<gse::world_system::data> world_d,
		gse::read<character_controller::component> characters,
		gse::read<orbit_camera::component> orbits,
		gse::write<component> sidearms,
		gse::structural<gse::physics::transform_component> round_transforms,
		gse::structural<gse::physics::motion_component>,
		gse::structural<gse::physics::collision_component>,
		gse::structural<gse::primitive_sphere_spec>
	) -> gse::async::task<>;
}
