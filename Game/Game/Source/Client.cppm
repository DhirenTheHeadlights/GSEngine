export module gs:client;

import std;
import gse;

export namespace gs {
	class client final : public gse::hook<gse::engine> {
	public:
		using hook::hook;

		auto initialize(
		) -> void override;

		auto update(
		) -> void override;

		auto render(
		) -> void override;
	private:
		std::unique_ptr<gse::network::client> m_client;
	};
}

auto gs::client::initialize() -> void {
	const gse::network::address server_address{ "127.0.0.1", 9000 };
	const gse::network::address client_address{ "127.0.0.1", 9001 };
	m_client = std::make_unique<gse::network::client>(client_address, server_address);
	m_client->connect();
}

auto gs::client::update() -> void {
	m_client->update([](gse::network::message& msg) {
		gse::match(msg)
			.if_is([&](const gse::network::pong_message& pong) {
				std::println("Client Received: Pong {}", pong.sequence);
			})
			.if_is([this](const gse::network::notify_scene_change_message& message) {
				m_owner->world->activate(
					gse::find(std::string_view(message.scene_name.data()))
				);
			});
		});
}

auto gs::client::render() -> void {
	gse::gui::start(
		"Network",
		[&] {
			if (!m_client) return;

			switch (m_client->current_state()) {
				case gse::network::client::state::disconnected:
					gse::gui::text("Status: Disconnected");
					break;
				case gse::network::client::state::connecting:
					gse::gui::text("Status: Connecting...");
					break;
				case gse::network::client::state::connected:
					gse::gui::text("Status: Connected");
					break;
				default: break;
			}

			if (gse::gui::button("Send Ping")) {
				if (m_client->current_state() == gse::network::client::state::connected) {
					static std::uint32_t ping_sequence = 0;
					m_client->send(gse::network::ping_message{ ++ping_sequence });
					std::println("Client Sent: Ping {}", ping_sequence);
				}
			}
		}
	);
}