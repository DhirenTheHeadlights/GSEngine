module sandbox:client_impl;

import std;
import gse;
import gse.system_manifest;

import :client;
import :character_controller;
import :orbit_camera;
import :piston;
import :sidearm;
import :tumbler;
import :sandbox_scene;

auto sandbox::client_system::init(gse::context& ctx, const gse::network::config& net_cfg, const gse::channel_write<gse::network::clear_providers_request, gse::network::add_provider_request, gse::network::refresh_servers_request> net_out) -> gse::async::task<> {
	gse::system_manifest<
		^^sandbox::orbit_camera,
		^^sandbox::player,
		^^sandbox::tumbler,
		^^sandbox::piston,
		^^sandbox::character_controller,
		^^sandbox::sidearm
	>{}.register_with(ctx);
	gse::register_systems<^^gse::free_camera::system>(ctx);

	net_out.push<gse::network::clear_providers_request>({});

	const auto configured = gse::network::parse_address(net_cfg.connect, gse::network::default_port);
	const gse::network::address listed = configured.value_or(gse::network::address{
		.ip = "127.0.0.1",
		.port = gse::network::default_port,
	});

	std::vector seed{
		gse::network::discovery_result{
			.addr = listed,
			.name = configured ? "Configured Server" : "Local",
			.players = 0,
			.max_players = net_cfg.max_players,
			.build = 1,
		},
	};
	net_out.push<gse::network::add_provider_request>({
		.provider = std::shared_ptr<gse::network::wan_directory_provider>(new gse::network::wan_directory_provider(std::move(seed))),
	});
	net_out.push<gse::network::refresh_servers_request>({
		.timeout = gse::milliseconds(200.f),
	});

	return {};
}