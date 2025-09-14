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
	private:
		std::unique_ptr<server> m_server;
		std::uint32_t tick_count = 0;

		interval_timer m_timer{ seconds(5.f) };
	};
}

auto gse::server_app::initialize() -> void {
	m_server = std::make_unique<server>(9000);
}

auto gse::server_app::update() -> void {
	m_server->update([this](const network::address& from, network::message& msg) {
		match(msg)
			.if_is<network::ping_message>([&](const network::ping_message& ping) {
				std::println("Server Received: Ping {} from {}:{}", ping.sequence, from.ip, from.port);
				m_server->send(network::pong_message{ ping.sequence }, from);
			});
		});

	if (m_timer.tick()) {
		std::println("Server Tick: {}", ++tick_count);
	}
}
