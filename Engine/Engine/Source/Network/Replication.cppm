export module gse.network:replication;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.assets;
import :registry_sync;
import :socket;
import :message;
import :bitstream;
import :packet_header;
import :remote_peer;

namespace gse {
    template <typename>
    constexpr bool is_resource_handle_v = false;

    template <typename T>
    constexpr bool is_resource_handle_v<resource::handle<T>> = true;

    template <typename T>
    struct handle_target;

    template <typename T>
    struct handle_target<resource::handle<T>> {
        using type = T;
    };

    template <typename>
    constexpr bool is_handle_array_v = false;

    template <typename T, std::size_t N>
    constexpr bool is_handle_array_v<std::array<resource::handle<T>, N>> = true;

    template <typename>
    struct handle_array_target;

    template <typename T, std::size_t N>
    struct handle_array_target<std::array<resource::handle<T>, N>> {
        using type = T;
    };

    template <typename T>
    consteval auto has_networked_members(
    ) -> bool;
}

template <typename T>
consteval auto gse::has_networked_members() -> bool {
    if constexpr (std::is_class_v<T> || std::is_union_v<T>) {
        for (auto m : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())) {
            if (has_annotation<networked_tag>(m)) {
                return true;
            }
        }
    }
    return false;
}

export namespace gse {
    template <typename T>
    auto on_network_received(
        T& c,
        const asset::state& assets
    ) -> void;
}

template <typename T>
auto gse::on_network_received(T& c, const asset::state& assets) -> void {
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
        if constexpr (has_annotation<networked_tag>(m)) {
            using m_type = typename [: std::meta::type_of(m) :];

            if constexpr (is_resource_handle_v<m_type>) {
                using asset_type = typename handle_target<m_type>::type;
                if (c.[:m:].id().exists()) {
                    c.[:m:] = asset::try_get<asset_type>(assets, c.[:m:].id());
                }
            }
            else if constexpr (is_handle_array_v<m_type>) {
                using asset_type = typename handle_array_target<m_type>::type;
                for (auto& h : c.[:m:]) {
                    if (h.id().exists()) {
                        h = asset::try_get<asset_type>(assets, h.id());
                    }
                }
            }
            else if constexpr (has_networked_members<m_type>()) {
                on_network_received(c.[:m:], assets);
            }
        }
    }
}

export namespace gse::network {
    template <typename T>
    auto broadcast_component_deltas(
        auto& send_fn,
        registry& reg,
        const std::unordered_map<address, remote_peer>& peers
    ) -> void;

    auto replicate_deltas(
        auto& send_fn,
        registry& reg,
        const std::unordered_map<address, remote_peer>& peers
    ) -> void;

    template <typename T>
    auto snapshot_components_to(
        auto& send_fn,
        registry& reg,
        const address& addr
    ) -> void;

    auto replicate_snapshot_to(
        auto& send_fn,
        registry& reg,
        const address& addr
    ) -> void;
}

template <typename T>
auto gse::network::broadcast_component_deltas(auto& send_fn, registry& reg, const std::unordered_map<address, remote_peer>& peers) -> void {
    const auto broadcast = [&](const auto& msg) {
        for (const auto& addr : peers | std::views::keys) {
            send_fn(msg, addr);
        }
    };

    const auto upsert = [&](const id eid) {
        if (auto* c = reg.try_component<T>(eid)) {
            broadcast(component_upsert<T>{ .owner_id = eid, .data = extract_networked(*c) });
        }
    };

    for (const auto eid : reg.drain_component_adds<T>()) {
        upsert(eid);
    }
    for (const auto eid : reg.drain_component_updates<T>()) {
        upsert(eid);
    }
    for (const auto eid : reg.drain_component_removes<T>()) {
        broadcast(component_remove<T>{ .owner_id = eid });
    }
}

auto gse::network::replicate_deltas(auto& send_fn, registry& reg, const std::unordered_map<address, remote_peer>& peers) -> void {
    if (peers.empty()) {
        return;
    }

    task::group group;

    for_each_networked_component([&]<typename C>() {
        group.post([&send_fn, &reg, &peers] {
        	broadcast_component_deltas<C>(send_fn, reg, peers);
        });
    });
}

template <typename T>
auto gse::network::snapshot_components_to(auto& send_fn, registry& reg, const address& addr) -> void {
    for (const auto& c : reg.components<T>()) {
        send_fn(component_upsert<T>{ .owner_id = c.owner_id, .data = extract_networked(c) }, addr);
    }
}

auto gse::network::replicate_snapshot_to(auto& send_fn, registry& reg, const address& addr) -> void {
    task::group group;

    for_each_networked_component([&]<typename C>() {
        group.post([&] {
            snapshot_components_to<C>(send_fn, reg, addr);
        });
    });
}
