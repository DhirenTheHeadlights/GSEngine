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
	auto broadcast_component_deltas_to(
		auto& send_fn,
		registry& reg,
		const std::unordered_map<address, remote_peer>& peers
	) -> void;

	auto replicate_all(
		auto& send_fn,
		registry& reg,
		const std::unordered_map<address, remote_peer>& peers
	) -> void;
}

template <typename T>
auto gse::network::broadcast_component_deltas_to(auto& send_fn, registry& reg, const std::unordered_map<address, remote_peer>& peers) -> void {
	for (const auto& eid : reg.drain_component_adds<T>()) {
		component_upsert<T> m{ .owner_id = eid.number(), .data = reg.linked_object_read<T>(eid) };
		for (const auto& addr : peers | std::views::keys) send_fn(m, addr);
	}
	for (const auto& eid : reg.drain_component_updates<T>()) {
		component_upsert<T> m{ .owner_id = eid.number(), .data = reg.linked_object_read<T>(eid) };
		for (const auto& addr : peers | std::views::keys) send_fn(m, addr);
	}
	for (const auto& eid : reg.drain_component_removes<T>()) {
		component_remove<T> m{ .owner_id = eid.number() };
		for (const auto& addr : peers | std::views::keys) send_fn(m, addr);
	}
}

auto gse::network::replicate_all(auto& send_fn, registry& reg, const std::unordered_map<address, remote_peer>& peers) -> void {
	for_each_networked_component([&]<typename C>() {
		broadcast_component_deltas_to<C>(send_fn, reg, peers);
	});
}
