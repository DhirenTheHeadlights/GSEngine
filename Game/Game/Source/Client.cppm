export module gs:client;

import std;
import gse;

import :player;
import :tumbler;

export namespace gs {
	struct client_system {
		struct data {
			std::uint32_t ping_seq = 0;
			int selected = -1;
			gse::clock refresh_clock;
			gse::interval_timer<> server_info_timer{ gse::seconds(10.f) };
		};

		static auto run(
			gse::run_context& ctx,
			data& d,
			const gse::network::data& net_d
		) -> gse::async::task<>;
	};
}

auto gs::client_system::run(gse::run_context& ctx, data& d, const gse::network::data& net_d) -> gse::async::task<> {
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

	const auto send_message = [&](auto m) {
		ctx.channels.push<gse::network::send_request>({
			.action = [m = std::move(m)](gse::network::client& c) {
				c.send(m);
			},
		});
	};

	while (true) {
		if (d.refresh_clock.elapsed<std::uint32_t>() > gse::seconds(1000u)) {
			ctx.channels.push<gse::network::refresh_servers_request>({
				.timeout = gse::milliseconds(150),
			});
			d.refresh_clock.reset();
		}

		if (net_d.connection_state == gse::network::client::state::connected && d.server_info_timer.tick()) {
			send_message(gse::network::server_info_request{});
		}

		ctx.channels.push<gse::gui::menu_content>({
			.menu = "Network",
			.build = [&](gse::gui::builder& ui) {
			switch (net_d.connection_state) {
				case gse::network::client::state::disconnected:
					ui.draw<gse::gui::text>({
						.content = "Status: Disconnected",
					});
					break;
				case gse::network::client::state::connecting:
					ui.draw<gse::gui::text>({
						.content = "Status: Connecting...",
					});
					break;
				case gse::network::client::state::connected:
					ui.draw<gse::gui::text>({
						.content = std::format("Status: Connected ({}/{})", net_d.connected_players, net_d.connected_max_players),
					});
					break;
				default:
					break;
			}

			if (ui.draw<gse::gui::button>({
				.text = "Refresh",
			})) {
				ctx.channels.push<gse::network::refresh_servers_request>({
					.timeout = gse::milliseconds(150),
				});
			}

			const auto& list = net_d.available_servers;
			ui.draw<gse::gui::text>({
				.content = std::format("Found: {}", list.size()),
			});

			for (std::size_t idx = 0; idx < list.size(); ++idx) {
				const auto& sv = list[idx];
				const bool picked = (d.selected == static_cast<int>(idx));
				if (ui.draw<gse::gui::selectable>({
					.text = std::format("{}  {}:{}  {}/{}  v{}", sv.name, sv.addr.ip, sv.addr.port, sv.players, sv.max_players, sv.build),
					.selected = picked,
				})) {
					d.selected = static_cast<int>(idx);
				}
			}

			if (ui.draw<gse::gui::button>({
				.text = "Connect",
			}) && d.selected >= 0 && d.selected < static_cast<int>(list.size())) {
				const auto& pick = list[static_cast<std::size_t>(d.selected)];
				ctx.channels.push<gse::network::connect_request>({
					.options = {
						.addr = pick.addr,
						.local_bind = gse::network::address{ .ip = "0.0.0.0", .port = 0 },
						.timeout = gse::seconds(5),
						.retry = gse::seconds(1),
					},
				});
			}

			if (ui.draw<gse::gui::button>({
				.text = "Send Ping",
			}) && net_d.connection_state == gse::network::client::state::connected) {
				send_message(gse::network::ping{
					.sequence = ++d.ping_seq,
				});
			}
			},
		});

		co_await ctx.next_tick();
	}
}
