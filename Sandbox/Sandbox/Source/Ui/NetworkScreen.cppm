export module sandbox:network_screen;

import std;
import gse;

export namespace sandbox {
	class network_screen : public gse::gui::screen {
	public:
		network_screen(
			gse::shared_view<gse::network::data> net,
			gse::channel_write<gse::network::connect_request, gse::network::refresh_servers_request, gse::network::refresh_server_info_request, gse::network::ping_request, gse::network::remember_server_request, gse::network::forget_server_request> channels
		);

		auto build(
			gse::gui::builder& ui,
			gse::gui::nav& n
		) -> void override;

		auto title() const -> std::string_view override;

	private:
		gse::shared_view<gse::network::data> m_net;
		gse::channel_write<gse::network::connect_request, gse::network::refresh_servers_request, gse::network::refresh_server_info_request, gse::network::ping_request, gse::network::remember_server_request, gse::network::forget_server_request> m_channels;
		gse::network::address m_selected;
		std::string m_entry;
		gse::gui::text_input_state m_entry_state;
		std::uint32_t m_ping_seq = 0;
		gse::clock m_refresh_clock;
	};
}

sandbox::network_screen::network_screen(const gse::shared_view<gse::network::data> net, gse::channel_write<gse::network::connect_request, gse::network::refresh_servers_request, gse::network::refresh_server_info_request, gse::network::ping_request, gse::network::remember_server_request, gse::network::forget_server_request> channels)
	: m_net(net), m_channels(std::move(channels)) {
}

auto sandbox::network_screen::build(gse::gui::builder& ui, gse::gui::nav& n) -> void {
	const auto& net = m_net;

	if (m_refresh_clock.elapsed() > gse::seconds(1.f)) {
		m_channels.push<gse::network::refresh_servers_request>({
			.timeout = gse::seconds(1.f),
		});
		m_refresh_clock.reset();
	}

	ui.draw<gse::gui::text>({
		.content = net.connection_status.empty() ? std::string("Status: Disconnected") : std::format("Status: {}", net.connection_status),
	});

	if (net.connection_state == gse::network::client::state::connected) {
		ui.draw<gse::gui::text>({
			.content = std::format("Players: {}/{}", net.connected_players, net.connected_max_players),
		});
	}

	if (ui.draw<gse::gui::button>({
			.text = "Refresh",
		})) {
		m_channels.push<gse::network::refresh_servers_request>({
			.timeout = gse::seconds(1.f),
		});
	}

	ui.draw<gse::gui::text_input>({
		.name = "Server address",
		.buffer = m_entry,
		.state = m_entry_state,
	});

	if (ui.draw<gse::gui::button>({
			.text = "Add",
		}) && !m_entry.empty()) {
		m_channels.push<gse::network::remember_server_request>({
			.entry = m_entry,
		});
		m_entry.clear();
		m_entry_state = {};
	}

	const auto& list = net.available_servers;
	ui.draw<gse::gui::text>({
		.content = std::format("Found: {}", list.size()),
	});

	for (const auto& sv : list) {
		if (ui.draw<gse::gui::selectable>({
				.text = std::format("{}  {}:{}  {}/{}  v{}", sv.name, sv.addr.ip, sv.addr.port, sv.players, sv.max_players, sv.build),
				.key = std::format("{}:{}", sv.addr.ip, sv.addr.port),
				.selected = sv.addr == m_selected,
			})) {
			m_selected = sv.addr;
		}
	}

	const auto picked = std::ranges::find(list, m_selected, &gse::network::discovery_result::addr);

	if (ui.draw<gse::gui::button>({
			.text = "Connect",
		}) && picked != list.end()) {
		m_channels.push<gse::network::connect_request>({
			.options = {
				.addr = picked->addr,
				.local_bind = gse::network::address{
					.ip = "0.0.0.0",
					.port = 0
				},
				.timeout = gse::seconds(5.f),
				.retry = gse::seconds(1.f),
			},
		});
	}

	if (ui.draw<gse::gui::button>({
			.text = "Forget",
		}) && picked != list.end()) {
		m_channels.push<gse::network::forget_server_request>({
			.entry = picked->name,
		});
		m_selected = {};
	}

	if (ui.draw<gse::gui::button>({
			.text = "Send Ping",
		}) && net.connection_state == gse::network::client::state::connected) {
		m_channels.push<gse::network::ping_request>({
			.sequence = ++m_ping_seq,
		});
	}
}

auto sandbox::network_screen::title() const -> std::string_view {
	return "Network";
}
