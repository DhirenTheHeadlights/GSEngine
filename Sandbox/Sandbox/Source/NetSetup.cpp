module sandbox:net_setup_impl;

import gse;
import gse.server;
import gse.system_manifest;

import :character_controller;
import :net_setup;
import :player_spawner;
import :player_state_broadcast;
import :sidearm;
import :world_loader;

auto sandbox::server_setup(gse::engine& e) -> void {
	gse::server_app_setup(e, networked_components{}, network_messages{});
	gse::system_manifest<^^player_spawner::data, ^^player_spawner::run>{}.register_with(e);
	gse::system_manifest<^^character_controller::data, ^^character_controller::run>{}.register_with(e);
	gse::system_manifest<^^sidearm::data, ^^sidearm::run>{}.register_with(e);
	gse::system_manifest<^^player_state_broadcast::data, ^^player_state_broadcast::run>{}.register_with(e);
	world_loader_setup(e);
}
