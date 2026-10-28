module sandbox:net_setup_impl;

import gse.runtime;
import gse.server;

import :game_systems;
import :net_setup;
import :world_loader;

auto sandbox::server_setup(gse::engine& e) -> void {
	gse::server_app_setup(e, networked_components{}, network_messages{});
	register_server_systems(e);
	world_loader_setup(e);
}