export module gs:client;

import std;
import gse;

import :player;
import :tumbler;

export namespace gs {
	struct client_state {
		std::uint32_t ping_seq = 0;
		int selected = -1;
		gse::clock refresh_clock;
		gse::interval_timer<> server_info_timer{ gse::seconds(10.f) };
		std::uint8_t connected_players = 0;
		std::uint8_t connected_max_players = 0;
	};

	struct client_system {
		static auto initialize(
			gse::init_context& phase,
			client_state& s
		) -> void;

		static auto update(
			gse::update_context& ctx,
			client_state& s
		) -> gse::async::task<>;
	};
}

auto gs::client_system::initialize(gse::init_context&, client_state&) -> void {
	gse::add_system<gs::player::system, gs::player::state>();
	gse::add_system<gs::tumbler::system, gs::tumbler::state>();
	gse::add_system<gse::free_camera::system, gse::free_camera::state>();

	gse::set_input_sampler(&gse::local_input_sampler);

	gse::network::clear_discovery_providers();
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
	gse::network::add_discovery_provider(std::make_shared<gse::network::wan_directory_provider>(std::move(seed)));
	gse::network::refresh_servers(gse::milliseconds(200));
}

auto gs::client_system::update(gse::update_context&, client_state& s) -> gse::async::task<> {
	gse::network::drain([&](const gse::network::inbox_message& m) {
		gse::match(m)
			.if_is([&](const gse::network::connection_accepted& msg) {
				gse::set_networked(true);
				gse::set_authoritative(false);
				gse::set_local_controller_id(msg.controller_id);
				if (const auto* scene = gse::current_scene()) {
					gse::deactivate_scene(scene->id());
				}
				gse::network::send(gse::network::server_info_request{});
			})
			.else_if_is([&](const gse::network::notify_scene_change& msg) {
				gse::activate_scene(msg.scene_id);
				std::println("Switched to scene: {}", msg.scene_id);
			})
			.else_if_is([&](const gse::network::server_info_response& msg) {
				s.connected_players = msg.players;
				s.connected_max_players = msg.max_players;
			});
	});

	if (s.refresh_clock.elapsed<std::uint32_t>() > gse::seconds(1000u)) {
		gse::network::refresh_servers(gse::milliseconds(150));
		s.refresh_clock.reset();
	}

	if (gse::network::connection_state() == gse::network::client::state::connected && s.server_info_timer.tick()) {
		gse::network::send(gse::network::server_info_request{});
	}

	gse::gui::panel("Network", [&](gse::gui::builder& ui) {
		switch (gse::network::connection_state()) {
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
			gse::network::refresh_servers(gse::milliseconds(150));
		}

		const auto list = gse::network::servers();
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
			gse::network::connect(pick, gse::network::address{ .ip = "0.0.0.0", .port = 0 }, gse::seconds(5), gse::seconds(1));
		}

		if (ui.draw<gse::gui::button>({
			.text = "Send Ping",
		}) && gse::network::connection_state() == gse::network::client::state::connected) {
			gse::network::send(gse::network::ping{
				.sequence = ++s.ping_seq,
			});
		}
	});

	co_return;
}
