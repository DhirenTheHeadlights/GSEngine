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
	m_server->update(
		m_owner->world.value(),
		[this](const network::address& from, network::message& msg) {
		match(msg)
			.if_is([&](const network::ping_message& ping) {
				std::println("Server Received: Ping {} from {}:{}", ping.sequence, from.ip, from.port);
				m_server->send(
					network::pong_message{
						.sequence = ping.sequence
					}, 
					from
				);
			});
	});

	std::optional<id> scene_requested_id;

	for (const auto& [scene_id, condition] : m_owner->world->triggers()) {
		for (const auto& [entity_id, latest_input] : m_server->clients() | std::views::values) {
			evaluation_context ctx{
				.client_id = entity_id,
				.input = latest_input,
				.registry = m_owner->world->current_scene()->registry()
			};

			if (condition(ctx)) {
				scene_requested_id = scene_id;
			}
		}
	}

	if (scene_requested_id.has_value()) {
		m_owner->world->activate(scene_requested_id.value());
	}

	for (const auto& addr : m_server->clients() | std::views::keys) {
		if (m_owner->world->current_scene() && m_owner->world->current_scene()->id() == scene_requested_id) {
			m_server->send(
				network::notify_scene_change_message{
					.scene_name = *m_owner->world->current_scene()->id().tag().data()
				},
				addr
			);
		}
	}

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
