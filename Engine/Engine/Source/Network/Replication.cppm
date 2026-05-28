export module gse.network:replication;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
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

	template <typename Pack>
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

	template <typename Pack>
	auto replicate_snapshot_to(
		auto& send_fn,
		registry& reg,
		const address& addr
	) -> void;

	template <typename Pack>
	auto match_and_apply_components(
		read_bitstream& s,
		std::uint64_t id,
		auto&& on_upsert,
		auto&& on_remove
	) -> bool;
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
			broadcast(component_upsert<T>{
				.owner_id = eid,
				.data = extract_networked(*c)
			});
		}
	};

	for (const auto eid : reg.drain_component_adds<T>()) {
		upsert(eid);
	}
	for (const auto eid : reg.drain_component_updates<T>()) {
		upsert(eid);
	}
	for (const auto eid : reg.drain_component_removes<T>()) {
		broadcast(component_remove<T>{
			.owner_id = eid
		});
	}
}

template <typename Pack>
auto gse::network::replicate_deltas(auto& send_fn, registry& reg, const std::unordered_map<address, remote_peer>& peers) -> void {
	if (peers.empty()) {
		return;
	}

	[&]<typename... C>(type_pack<C...>) {
		(broadcast_component_deltas<C>(send_fn, reg, peers), ...);
	}(Pack{});
}

template <typename T>
auto gse::network::snapshot_components_to(auto& send_fn, registry& reg, const address& addr) -> void {
	const auto components = reg.components<T>();
	const auto ids = reg.owner_ids<T>();
	for (std::size_t i = 0; i < components.size(); ++i) {
		send_fn(
			component_upsert<T>{
				.owner_id = ids[i],
				.data = extract_networked(components[i])
			},
			addr
		);
	}
}

template <typename Pack>
auto gse::network::replicate_snapshot_to(auto& send_fn, registry& reg, const address& addr) -> void {
	[&]<typename... C>(type_pack<C...>) {
		(snapshot_components_to<C>(send_fn, reg, addr), ...);
	}(Pack{});
}

template <typename Pack>
auto gse::network::match_and_apply_components(read_bitstream& s, const std::uint64_t id, auto&& on_upsert, auto&& on_remove) -> bool {
	return [&]<typename... C>(type_pack<C...>) {
		return (
			(try_decode<component_upsert<C>>(s, id, on_upsert) ||
			 try_decode<component_remove<C>>(s, id, on_remove)) ||
			...
		);
	}(Pack{});
}
