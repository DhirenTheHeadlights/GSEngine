export module gs:client;

import std;
import gse;

import :player;
import :tumbler;

export namespace gs {
	struct client_system {
		static auto run(
			gse::run_context& ctx
		) -> gse::async::task<>;
	};
}

auto gs::client_system::run(gse::run_context& ctx) -> gse::async::task<> {
	ctx.add_system<gs::player::system>();
	ctx.add_system<gs::tumbler::system>();
	ctx.add_system<gse::free_camera::system>();

	ctx.channels.push<gse::network::clear_providers_request>({});
	std::vector seed{
		gse::network::discovery_result{
			.addr = gse::network::address{
				.ip = "192.168.1.156",
				.port = 9000,
			},
			.name = "GoonSquad Server",
			.map = "dev_map",
			.players = 0,
			.max_players = 8,
			.build = 1,
		},
		gse::network::discovery_result{
			.addr = gse::network::address{
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
		.provider = std::make_shared<gse::network::wan_directory_provider>(std::move(seed)),
	});
	ctx.channels.push<gse::network::refresh_servers_request>({
		.timeout = gse::milliseconds(200),
	});

	while (true) {
		co_await ctx.next_tick();
	}
}
