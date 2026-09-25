export module sandbox:sidearm;

import std;
import gse;

import :character_controller;
import :orbit_camera;

export namespace sandbox::sidearm {
	struct [[= gse::network::network_message{}]] fire_request {
		std::uint32_t shot = 0;
		gse::angle yaw = {};
		gse::angle pitch = {};
	};

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
		std::uint32_t fired_seen = 0;
	};

	struct bindings {
		[[= gse::actions::mouse_bind<"Fire", gse::mouse_button::button_1>{}]]
		gse::actions::handle fire;
	};

	struct live_round {
		gse::id entity;
		gse::time remaining;
	};

	struct [[= gse::system_state<"Sidearm">{}]] data {
		[[= gse::actions::set{}]] bindings binds;
		std::vector<live_round> rounds;
		std::uint32_t shot = 0;
	};

	[[= gse::system_run<>{}]]
	[[= gse::runs_after_optional<^^orbit_camera::data>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::shared_view<gse::actions::data> as,
		gse::shared_view<gse::camera::data> cam_s,
		gse::shared_view<gse::physics::data> phys_s,
		gse::shared_view<gse::world_system::data> world_d,
		gse::channel_read<gse::network::received<fire_request>> fire_in,
		gse::channel_write<gse::network::send_request<fire_request>> fire_out,
		gse::read<character_controller::component> characters,
		gse::read<orbit_camera::component> orbits,
		gse::read<gse::player_controller> controllers,
		gse::write<component> sidearms,
		gse::structural<gse::physics::transform_component> round_transforms,
		gse::structural<gse::physics::motion_component>,
		gse::structural<gse::physics::collision_component>,
		gse::structural<gse::primitive_sphere_spec>
	) -> gse::async::task<>;
}
