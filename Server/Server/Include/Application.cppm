export module gse.server;

import :server;

import std;
import gse;

export namespace gse {
	class server_app : public hook<engine> {
	public:
		using hook::hook;

		auto initialize(
		) -> void override;

		auto update(
		) -> void override;

		auto render(
		) -> void override;
	private:
		std::unique_ptr<server> m_server;
		std::uint32_t m_tick_count = 0;

		interval_timer m_timer{ seconds(5.f) };
	};
}

auto gse::server_app::initialize() -> void {
	m_server = std::make_unique<server>(9000);
	renderer::set_ui_focus(true);
}

auto gse::server_app::update() -> void {
	m_server->update([this](const network::address& from, network::message& msg) {
		match(msg)
			.if_is<network::ping_message>([&](const network::ping_message& ping) {
				std::println("Server Received: Ping {} from {}:{}", ping.sequence, from.ip, from.port);
				m_server->send(
					network::pong_message{
						.sequence = ping.sequence
					}, 
					from
				);
			});
	});

	if (m_timer.tick()) {
		m_tick_count++;
	}

	if (keyboard::pressed(key::escape)) {
		gse::shutdown();
	}
}

auto gse::server_app::render() -> void {
	gui::start(
		"Server Control",
		[&] {
			gui::text("This is a simple server application.");
			for (const auto& [ip, port] : m_server->peers() | std::views::keys) {
				gui::text(std::format("Client: {}:{}", ip, port));
			}
			gui::value("Ticks", m_tick_count);
		}
	);
}
