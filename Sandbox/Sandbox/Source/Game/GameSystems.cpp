module sandbox:game_systems_impl;

import gse;
import gse.system_manifest;

import :character_controller;
import :crosshair_system;
import :dev_spawn_menu;
import :dev_spawn_system;
import :game_systems;
import :hud;
import :net_directory;
import :orbit_camera;
import :pause_menu_system;
import :piston;
import :player_spawner;
import :player_state_broadcast;
import :player_reconcile;
import :player_sync;
import :possession;
import :sidearm;
import :telemetry;
import :tumbler;

namespace sandbox {
	using shared_systems = gse::system_manifest<
		^^character_controller::data,
		^^character_controller::run,
		^^sidearm::data,
		^^sidearm::run,
		^^sidearm::rounds::data,
		^^sidearm::rounds::run,
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
		^^player_sync::run,
		^^player_reconcile::data,
		^^player_reconcile::run,
		^^dev_spawn::data,
		^^dev_spawn::run
	>;

	using client_ui_systems = gse::system_manifest<
		^^crosshair::data,
		^^crosshair::draw_settings,
		^^hud::data,
		^^hud::run,
		^^telemetry::data,
		^^telemetry::run,
		^^dev_spawn_menu::data,
		^^dev_spawn_menu::run,
		^^pause_menu::data,
		^^pause_menu::run
	>;

	using server_systems = gse::system_manifest<
		^^player_spawner::data,
		^^player_spawner::run,
		^^player_state_broadcast::data,
		^^player_state_broadcast::run
	>;
}

auto sandbox::register_client_systems(gse::engine& e, const bool render) -> void {
	shared_systems{}.register_with(e);
	client_systems{}.register_with(e);
	gse::system_manifest<^^net_directory::init>{}.register_with(e);
	gse::register_systems<^^gse::free_camera::system>(e);

	if (render) {
		client_ui_systems{}.register_with(e);
		gse::register_systems<^^gse::gui::popout_system>(e);
	}
}

auto sandbox::register_server_systems(gse::engine& e) -> void {
	shared_systems{}.register_with(e);
	server_systems{}.register_with(e);
}
