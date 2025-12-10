export module gse.network:replication;

import std;

import gse.utility;

import :registry_sync;
import :socket;
import :message;
import :bitstream;
import :packet_header;
import :remote_peer;

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
    for (const auto& eid : reg.drain_component_adds<T>()) {
        if (auto* c = reg.try_linked_object_read<T>(eid)) {
            component_upsert<T> m{
            	.owner_id = eid,
            	.data = c->networked_data()
            };

            for (const auto& addr : peers | std::views::keys) {
                send_fn(m, addr);
            }
        }
    }

    for (const auto& eid : reg.drain_component_updates<T>()) {
        if (auto* c = reg.try_linked_object_read<T>(eid)) {
            component_upsert<T> m{ .owner_id = eid, .data = c->networked_data() };
            for (const auto& addr : peers | std::views::keys) {
                send_fn(m, addr);
            }
        }
    }

    for (const auto& eid : reg.drain_component_removes<T>()) {
        component_remove<T> m{
        	.owner_id = eid
        };
        for (const auto& addr : peers | std::views::keys) {
            send_fn(m, addr);
        }
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
    std::size_t count = 0;

    for (const auto& c : reg.linked_objects_read<T>()) {
        component_upsert<T> m{
            .owner_id = c.owner_id(),
            .data = c.networked_data()
        };
        send_fn(m, addr);
        ++count;
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
