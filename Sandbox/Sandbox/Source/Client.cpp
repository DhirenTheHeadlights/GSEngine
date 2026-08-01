module sandbox:client_impl;

import std;
import gse;
import gse.system_manifest;

import :client;
import :orbit_camera;
import :piston;
import :tumbler;

auto sandbox::client_system::init(gse::context& ctx) -> gse::async::task<> {
	gse::system_manifest<
		^^sandbox::orbit_camera::data, ^^sandbox::orbit_camera::attach, ^^sandbox::orbit_camera::update,
		^^sandbox::tumbler::data, ^^sandbox::tumbler::run,
		^^sandbox::piston::data, ^^sandbox::piston::run
	>{}.register_with(ctx);
	gse::register_systems<^^gse::free_camera::system>(ctx);

	ctx.channels.push<gse::network::clear_providers_request>({});
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
	ctx.channels.push<gse::network::add_provider_request>({
		.provider = std::shared_ptr<gse::network::wan_directory_provider>(new gse::network::wan_directory_provider(std::move(seed))),
	});
	ctx.channels.push<gse::network::refresh_servers_request>({
		.timeout = gse::milliseconds(200),
	});

	return {};
}
