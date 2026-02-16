export module gs:client;

import std;
import gse;

export namespace gs {
    class client final : public gse::hook<gse::engine> {
    public:
        using hook::hook;
        auto initialize() -> void override;
        auto update() -> void override;
        auto render() -> void override;
    private:
        std::uint32_t m_ping_seq = 0;
        int m_selected = -1;
        gse::clock m_refresh_clock;
    };
}

auto gs::client::initialize() -> void {
	gse::network::clear_discovery_providers();
    std::vector seed{
	    gse::network::discovery_result{
            .addr = gse::network::address{
            	.ip = "207.191.198.190",
                .port = 9000
            },
            .name = "GoonSquad Server",
            .map = "dev_map",
            .players = 0,
            .max_players = 8,
            .build = 1
        }
    };
	gse::network::add_discovery_provider(std::make_unique<gse::network::wan_directory_provider>(std::move(seed)));
	gse::network::refresh_servers(gse::milliseconds(200));

    m_owner->hook_world<gse::networked_world<gse::local_input_source>>();
    m_owner->hook_world<gse::player_controller_hook>();
}

auto gs::client::update() -> void {
    gse::network::drain([this](gse::network::inbox_message& m) {
        gse::match(m)
            .if_is([&](const gse::network::connection_accepted&) {
                m_owner->set_networked(true);
                m_owner->set_authoritative(false);
                if (const auto* scene = m_owner->current_scene()) {
                    m_owner->deactivate_scene(scene->id());
                }
            })
            .else_if_is([&](const gse::network::notify_scene_change& msg) {
                m_owner->activate_scene(msg.scene_id);
                std::println("Switched to scene: {}", msg.scene_id);
            });
    });

    if (m_refresh_clock.elapsed<std::uint32_t>() > 1000u) {
        gse::network::refresh_servers(gse::milliseconds(150));
        m_refresh_clock.reset();
    }
}

auto gs::client::render() -> void {
    gse::gui::start("Network", [&] {
        switch (gse::network::state()) {
            case gse::network::client::state::disconnected: gse::gui::text("Status: Disconnected"); break;
            case gse::network::client::state::connecting:   gse::gui::text("Status: Connecting..."); break;
            case gse::network::client::state::connected:    gse::gui::text("Status: Connected"); break;
            default: break;
        }

        if (gse::gui::button("Refresh")) {
            gse::network::refresh_servers(gse::milliseconds(150));
        }

        const auto list = gse::network::servers();
        gse::gui::text(std::format("Found: {}", list.size()));

        for (auto [idx, s] : list | std::views::enumerate) {
            if (const bool picked = (m_selected == static_cast<int>(idx)); gse::gui::selectable(std::format("{}  {}:{}  {}/{}  v{}", s.name, s.addr.ip, s.addr.port, s.players, s.max_players, s.build), picked)) {
                m_selected = static_cast<int>(idx);
            }
        }

        if (gse::gui::button("Connect") && m_selected >= 0 && m_selected < static_cast<int>(list.size())) {
            const auto& pick = list[static_cast<std::size_t>(m_selected)];
            gse::network::connect(pick, gse::network::address{ "0.0.0.0", 0 }, gse::seconds(5), gse::seconds(1));
        }

        if (gse::gui::button("Send Ping") && gse::network::state() == gse::network::client::state::connected) {
            gse::network::send(gse::network::ping{ ++m_ping_seq });
        }
    });
}
