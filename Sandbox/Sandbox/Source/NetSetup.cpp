module sandbox:net_setup_impl;

import gse;
import gse.server;

import :net_setup;
import :world_loader;

auto sandbox::server_setup(gse::engine& e) -> void {
	gse::server_app_setup(e, networked_components{}, network_messages{});
	world_loader_setup(e);
}
