export module gs:network_screen;

import std;
import gse;

export namespace gs {
	class network_screen : public gse::gui::screen {
	public:
		network_screen(
			const gse::network::data& net,
			gse::channel_writer channels
		);

		auto build(
			gse::gui::builder& ui,
			gse::gui::nav& n
		) -> void override;

		auto title(
		) const -> std::string_view override;
	private:
		const gse::network::data* m_net;
		gse::channel_writer m_channels;
		int m_selected = -1;
		std::uint32_t m_ping_seq = 0;
		gse::clock m_refresh_clock;
		gse::interval_timer<> m_server_info_timer{ gse::seconds(10.f) };
	};
}

gs::network_screen::network_screen(const gse::network::data& net, gse::channel_writer channels)
	: m_net(&net), m_channels(std::move(channels)) {}

auto gs::network_screen::build(gse::gui::builder& ui, gse::gui::nav& n) -> void {
	const auto& net = *m_net;

	const auto send_message = [this](auto m) {
		m_channels.push<gse::network::send_request>({
			.action = [m = std::move(m)](gse::network::client& c) {
				c.send(m);
			},
		});
	};

	if (m_refresh_clock.elapsed<std::uint32_t>() > gse::seconds(1000u)) {
		m_channels.push<gse::network::refresh_servers_request>({
			.timeout = gse::milliseconds(150),
		});
		m_refresh_clock.reset();
	}

	if (net.connection_state == gse::network::client::state::connected && m_server_info_timer.tick()) {
		send_message(gse::network::server_info_request{});
	}

	switch (net.connection_state) {
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
				.content = std::format("Status: Connected ({}/{})", net.connected_players, net.connected_max_players),
			});
			break;
		default:
			break;
	}

	if (ui.draw<gse::gui::button>({
			.text = "Refresh",
		})) {
		m_channels.push<gse::network::refresh_servers_request>({
			.timeout = gse::milliseconds(150),
		});
	}

	const auto& list = net.available_servers;
	ui.draw<gse::gui::text>({
		.content = std::format("Found: {}", list.size()),
	});

	for (std::size_t idx = 0; idx < list.size(); ++idx) {
		const auto& sv = list[idx];
		const bool picked = (m_selected == static_cast<int>(idx));
		if (ui.draw<gse::gui::selectable>({
				.text = std::format("{}  {}:{}  {}/{}  v{}", sv.name, sv.addr.ip, sv.addr.port, sv.players, sv.max_players, sv.build),
				.selected = picked,
			})) {
			m_selected = static_cast<int>(idx);
		}
	}

	if (ui.draw<gse::gui::button>({
			.text = "Connect",
		}) && m_selected >= 0 && m_selected < static_cast<int>(list.size())) {
		const auto& pick = list[static_cast<std::size_t>(m_selected)];
		m_channels.push<gse::network::connect_request>({
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
		}) && net.connection_state == gse::network::client::state::connected) {
		send_message(gse::network::ping{
			.sequence = ++m_ping_seq,
		});
	}
}

auto gs::network_screen::title() const -> std::string_view {
	return "Network";
}
