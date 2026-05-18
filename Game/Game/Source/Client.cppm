export module gs:client;

import std;
import gse;

import :player;
import :tumbler;
import :network_screen;

export namespace gs {
	struct client_system {
		struct data {};

		static auto run(
			gse::run_context& ctx,
			data& d,
			const gse::network::data& net_d,
			const gse::gui::system::data& gui_d
		) -> gse::async::task<>;
	};
}

auto gs::client_system::run(gse::run_context& ctx, data& d, const gse::network::data& net_d, const gse::gui::system::data& gui_d) -> gse::async::task<> {
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
		const auto active = gui_d.menu_bar_state.active;
		const auto top_section = gui_d.menu_stack.top_section();
		const bool want = (active == gse::menu_bar::section::network);
		const bool on_top = (top_section == gse::menu_bar::section::network);

		if (want && !on_top && top_section == gse::menu_bar::section::none) {
			ctx.channels.push<gse::gui::push_screen_request>({
				.factory = [&net_d, channels = ctx.channels] {
					return std::make_unique<network_screen>(net_d, channels);
				},
			});
		}

		co_await ctx.next_tick();
	}
}
