export module gs:client;

import std;
import gse;

import :player;
import :tumbler;

export namespace gs {
	struct client_system {
		struct state {
			std::uint32_t ping_seq = 0;
			int selected = -1;
			gse::clock refresh_clock;
			gse::interval_timer<> server_info_timer{ gse::seconds(10.f) };
			std::uint8_t connected_players = 0;
			std::uint8_t connected_max_players = 0;
		};

		static auto run(
			gse::run_context& ctx,
			state& s,
			const gse::network::system::state& net_s
		) -> gse::async::task<>;
	};
}

auto gs::client_system::run(gse::run_context& ctx, state& s, const gse::network::system::state& net_s) -> gse::async::task<> {
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
		for (const auto& m : net_s.user_inbox) {
			gse::match(m)
				.if_is([&](const gse::network::connection_accepted& msg) {
					ctx.channels.push<gse::set_networked_request>({ .value = true });
					ctx.channels.push<gse::set_authoritative_request>({ .value = false });
					ctx.channels.push<gse::set_local_controller_id_request>({ .controller_id = msg.controller_id });
					ctx.channels.push<gse::deactivate_active_scene_request>({});
					send_message(gse::network::server_info_request{});
				})
				.else_if_is([&](const gse::network::notify_scene_change& msg) {
					ctx.channels.push<gse::activate_scene_request>({ .scene_id = msg.scene_id });
					std::println("Switched to scene: {}", msg.scene_id);
				})
				.else_if_is([&](const gse::network::server_info_response& msg) {
					s.connected_players = msg.players;
					s.connected_max_players = msg.max_players;
				});
		}

		if (s.refresh_clock.elapsed<std::uint32_t>() > gse::seconds(1000u)) {
			ctx.channels.push<gse::network::refresh_servers_request>({
				.timeout = gse::milliseconds(150),
			});
			s.refresh_clock.reset();
		}

		if (net_s.connection_state == gse::network::client::state::connected && s.server_info_timer.tick()) {
			send_message(gse::network::server_info_request{});
		}

		ctx.channels.push<gse::gui::menu_content>({
			.menu = "Network",
			.build = [&](gse::gui::builder& ui) {
			switch (net_s.connection_state) {
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
						.content = std::format("Status: Connected ({}/{})", s.connected_players, s.connected_max_players),
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

			const auto& list = net_s.available_servers;
			ui.draw<gse::gui::text>({
				.content = std::format("Found: {}", list.size()),
			});

			for (std::size_t idx = 0; idx < list.size(); ++idx) {
				const auto& sv = list[idx];
				const bool picked = (s.selected == static_cast<int>(idx));
				if (ui.draw<gse::gui::selectable>({
					.text = std::format("{}  {}:{}  {}/{}  v{}", sv.name, sv.addr.ip, sv.addr.port, sv.players, sv.max_players, sv.build),
					.selected = picked,
				})) {
					s.selected = static_cast<int>(idx);
				}
			}

			if (ui.draw<gse::gui::button>({
				.text = "Connect",
			}) && s.selected >= 0 && s.selected < static_cast<int>(list.size())) {
				const auto& pick = list[static_cast<std::size_t>(s.selected)];
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
			}) && net_s.connection_state == gse::network::client::state::connected) {
				send_message(gse::network::ping{
					.sequence = ++s.ping_seq,
				});
			}
			},
		});

		co_await ctx.next_tick();
	}
}
