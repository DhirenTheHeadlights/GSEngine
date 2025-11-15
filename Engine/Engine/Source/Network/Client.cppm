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
        std::array<char, 64> m_pending_scene{};
        bool m_has_pending = false;
    };
}

auto gs::client::initialize() -> void {
    using namespace gse::network;
    clear_discovery_providers();
    std::vector seed{
        discovery_result{
            .addr = address{.ip = "127.0.0.1", .port = 9000 },
            .name = "Local Server",
            .map = "dev_map",
            .players = 0,
            .max_players = 8,
            .build = 1
        }
    };
    add_discovery_provider(std::make_unique<wan_directory_provider>(std::move(seed)));
    refresh_servers(gse::milliseconds(200));
}

auto gs::client::update() -> void {
    gse::network::drain([this](gse::network::inbox_message& m) {
        gse::match(m)
            .if_is([&](const gse::network::connection_accepted&) {
            m_owner->world.set_networked(true);
            if (const auto* scene = m_owner->world.current_scene()) {
                m_owner->world.deactivate(scene->id());
            }
                })
            .else_if_is([&](const gse::network::notify_scene_change& msg) {
            std::ranges::fill(m_pending_scene, '\0');
            const auto* src = msg.scene_name.data();
            const auto src_len = std::char_traits<char>::length(src);
            const auto n = std::min(src_len, m_pending_scene.size() - 1);
            std::ranges::copy_n(src, n, m_pending_scene.begin());
            m_has_pending = true;

            if (const auto id = gse::find(std::string_view(m_pending_scene.data())); id.exists()) {
                m_owner->world.activate(id);
                m_has_pending = false;
                std::println("Switched to scene: {}", m_pending_scene.data());
            }
                });
        });

    if (m_has_pending) {
        if (const auto id = gse::find(std::string_view(m_pending_scene.data())); id.exists()) {
            m_owner->world.activate(id);
            m_has_pending = false;
            std::println("Switched to scene: {}", m_pending_scene.data());
        }
    }

    if (m_refresh_clock.elapsed<std::uint32_t>() > 1000u) {
        gse::network::refresh_servers(gse::milliseconds(150));
        m_refresh_clock.reset();
    }
}

auto gs::client::render() -> void {
    gse::gui::start("Network", [&] {
        using gse::network::client;
        switch (gse::network::state()) {
        case client::state::disconnected: gse::gui::text("Status: Disconnected"); break;
        case client::state::connecting:   gse::gui::text("Status: Connecting..."); break;
        case client::state::connected:    gse::gui::text("Status: Connected"); break;
        default: break;
        }

        if (gse::gui::button("Refresh")) {
            gse::network::refresh_servers(gse::milliseconds(150));
        }

        const auto list = gse::network::servers();
        gse::gui::text(std::format("Found: {}", list.size()));

        int idx = 0;
        for (const auto& s : list) {
            if (const bool picked = (m_selected == idx); gse::gui::selectable(std::format("{}  {}:{}  {}/{}  v{}", s.name, s.addr.ip, s.addr.port, s.players, s.max_players, s.build), picked)) {
                m_selected = idx;
            }
            ++idx;
        }

        if (gse::gui::button("Connect") && m_selected >= 0 && m_selected < static_cast<int>(list.size())) {
            const auto& pick = list[static_cast<std::size_t>(m_selected)];
            gse::network::connect(pick, gse::network::address{ "0.0.0.0", 0 }, gse::seconds(5), gse::seconds(1));
        }

        if (gse::gui::button("Send Ping") && gse::network::state() == client::state::connected) {
            gse::network::send(gse::network::ping{ ++m_ping_seq });
        }
        });
}
