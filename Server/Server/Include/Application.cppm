export module gse.server;

import :server;

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
		std::unique_ptr<server> m_server;
		std::uint32_t m_tick_count = 0;
		interval_timer<> m_timer{ seconds(5.f) };
	};
}

auto gse::server_app::initialize() -> void {
	m_server = std::make_unique<server>(9000);
	renderer::set_ui_focus(true);
}

auto gse::server_app::update() -> void {
	m_server->update(m_owner->world);

	std::optional<id> scene_requested_id;

	if (auto* cur = m_owner->world.current_scene()) {
		for (const auto& [scene_id, condition] : m_owner->world.triggers()) {
			for (const auto& cd : m_server->clients() | std::views::values) {
				evaluation_context ctx{
					.client_id = cd.entity_id,
					.input = &cd.latest_input,
					.registry = &cur->registry()
				};
				if (condition(ctx)) {
					scene_requested_id = scene_id;
				}
			}
		}
	}

	if (scene_requested_id) {
		m_owner->world.activate(*scene_requested_id);

		if (const auto* active = m_owner->world.current_scene()) {
			const network::notify_scene_change msg{
				.scene_id = active->id()
			};

			for (const auto& addr : m_server->clients() | std::views::keys) {
				m_server->send(msg, addr);
			}
		}
	}

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
