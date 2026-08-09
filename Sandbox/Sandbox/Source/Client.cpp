module sandbox:client_impl;

import std;
import gse;
import gse.system_manifest;

import :client;
import :character_controller;
import :orbit_camera;
import :piston;
import :tumbler;
import :sandbox_scene;

auto sandbox::client_system::init(gse::context& ctx, const gse::channel_write<gse::network::clear_providers_request, gse::network::add_provider_request, gse::network::refresh_servers_request> net_out) -> gse::async::task<> {
	gse::system_manifest<
		^^sandbox::orbit_camera::data, ^^sandbox::orbit_camera::attach, ^^sandbox::orbit_camera::update,
		^^sandbox::player::data, ^^sandbox::player::run,
		^^sandbox::tumbler::data, ^^sandbox::tumbler::run,
		^^sandbox::piston::data, ^^sandbox::piston::run,
		^^sandbox::character_controller::data, ^^sandbox::character_controller::run
	>{}.register_with(ctx);
	gse::register_systems<^^gse::free_camera::system>(ctx);

	net_out.push<gse::network::clear_providers_request>({});
	std::vector seed{
		gse::network::discovery_result{
			.addr =
				gse::network::address{
					.ip = "192.168.1.156",
					.port = 9000,
				},
			.name = "Sandbox Server",
			.map = "dev_map",
			.players = 0,
			.max_players = 8,
			.build = 1,
		},
		gse::network::discovery_result{
			.addr =
				gse::network::address{
					.ip = "127.0.0.1",
					.port = 9000,
				},
			.name = "Local",
			.map = "dev_map",
			.players = 0,
			.max_players = 8,
			.build = 1,
		},
	};
	net_out.push<gse::network::add_provider_request>({
		.provider = std::shared_ptr<gse::network::wan_directory_provider>(new gse::network::wan_directory_provider(std::move(seed))),
	});
	net_out.push<gse::network::refresh_servers_request>({
		.timeout = gse::milliseconds(200),
	});

	return {};
}
