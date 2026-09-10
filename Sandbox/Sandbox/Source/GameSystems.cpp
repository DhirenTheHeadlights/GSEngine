module sandbox:game_systems_impl;

import gse;
import gse.system_manifest;

import :character_controller;
import :game_systems;
import :orbit_camera;
import :piston;
import :player_spawner;
import :player_state_broadcast;
import :player_sync;
import :sandbox_scene;
import :sidearm;
import :tumbler;

namespace sandbox {
	using shared_systems = gse::system_manifest<
		^^character_controller::data,
		^^character_controller::run,
		^^sidearm::data,
		^^sidearm::run,
		^^tumbler::data,
		^^tumbler::run,
		^^piston::data,
		^^piston::run
	>;

	using client_systems = gse::system_manifest<
		^^orbit_camera::data,
		^^orbit_camera::attach,
		^^orbit_camera::update,
		^^player::data,
		^^player::run,
		^^player_sync::data,
		^^player_sync::run
	>;

	using server_systems = gse::system_manifest<
		^^player_spawner::data,
		^^player_spawner::run,
		^^player_state_broadcast::data,
		^^player_state_broadcast::run
	>;
}

auto sandbox::register_client_systems(gse::context& ctx) -> void {
	shared_systems{}.register_with(ctx);
	client_systems{}.register_with(ctx);
}

auto sandbox::register_server_systems(gse::engine& e) -> void {
	shared_systems{}.register_with(e);
	server_systems{}.register_with(e);
}