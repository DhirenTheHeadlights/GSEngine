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
		auto render() -> void override;

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
}

auto gse::server_app::render() -> void {
	gui::start("Server Control", [&] {
		gui::text("This is a simple server application.");
		gui::value("Peers", static_cast<std::uint32_t>(m_server->peers().size()));
		gui::value("Clients", static_cast<std::uint32_t>(m_server->clients().size()));
		if (const auto h = m_server->host_entity()) {
			gui::text(std::format("Host entity: {}", *h));
		} else {
			gui::text("Host entity: <none>");
		}
		for (const auto& [ip, port] : m_server->peers() | std::views::keys) {
			gui::text(std::format("Peer: {}:{}", ip, port));
		}
		gui::value("Ticks", m_tick_count);
	});
}
