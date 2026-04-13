export module gse.server;

import :server;
import :input_source;

import std;
import gse;

export namespace gse {
	class server_app : public hook<engine> {
	public:
		using hook::hook;

		auto initialize() -> void override;
		auto update() -> void override;

	private:
		server* m_server = nullptr;
		std::uint32_t m_tick_count = 0;
		interval_timer<> m_timer{ seconds(5.f) };
	};
}

auto gse::server_app::initialize() -> void {
	set_ui_focus(true);
	m_owner->set_networked(true);

	m_server = &m_owner->hook_world<server>(9000);
	m_owner->hook_world<networked_world<server_input_source>>(
		server_input_source{
			&m_server->clients()
		}
	);
	m_owner->hook_world<player_controller_hook>();
}

auto gse::server_app::update() -> void {
	if (m_timer.tick()) {
		++m_tick_count;
	}

	if (keyboard::pressed(key::escape)) {
		gse::shutdown();
	}

	gui::panel("Server Control", [&](gui::builder& ui) {
		ui.draw<gui::text>({
			.content = "This is a simple server application."
		});
		ui.draw<gui::value<std::uint32_t>>({
			.name = "Peers",
			.val = static_cast<std::uint32_t>(m_server->peers().size())
		});
		ui.draw<gui::value<std::uint32_t>>({
			.name = "Clients",
			.val = static_cast<std::uint32_t>(m_server->clients().size())
		});
		if (const auto h = m_server->host_entity()) {
			ui.draw<gui::text>({
				.content = std::format("Host entity: {}", *h)
			});
		} else {
			ui.draw<gui::text>({
				.content = "Host entity: <none>"
			});
		}
		for (const auto& [ip, port] : m_server->peers() | std::views::keys) {
			ui.draw<gui::text>({
				.content = std::format("Peer: {}:{}", ip, port)
			});
		}
		ui.draw<gui::value<std::uint32_t>>({
			.name = "Ticks",
			.val = m_tick_count
		});
	});
}
