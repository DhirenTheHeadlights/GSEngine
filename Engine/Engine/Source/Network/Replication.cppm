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
	template <typename T, access_mode M>
	auto broadcast_component_deltas(
		auto& send_fn,
		const access<T, M>& components,
		const std::unordered_map<address, remote_peer>& peers
	) -> void;

	template <typename... C, access_mode... M>
	auto replicate_deltas(
		auto& send_fn,
		const std::unordered_map<address, remote_peer>& peers,
		const access<C, M>&... components
	) -> void;

	template <typename T, access_mode M>
	auto snapshot_components_to(
		auto& send_fn,
		const access<T, M>& components,
		const address& addr
	) -> void;

	template <typename... C, access_mode... M>
	auto replicate_snapshot_to(
		auto& send_fn,
		const address& addr,
		const access<C, M>&... components
	) -> void;

	template <typename Pack>
	auto match_and_apply_components(
		read_bitstream& s,
		std::uint64_t id,
		auto&& on_upsert,
		auto&& on_remove
	) -> bool;
}

template <typename T, gse::access_mode M>
auto gse::network::broadcast_component_deltas(auto& send_fn, const access<T, M>& components, const std::unordered_map<address, remote_peer>& peers) -> void {
	const auto broadcast = [&](const auto& msg) {
		for (const auto& addr : peers | std::views::keys) {
			send_fn(msg, addr);
		}
	};

	const auto upsert = [&](const id eid) {
		if (const auto* c = components.find(eid)) {
			broadcast(component_upsert<T>{
				.owner_id = eid,
				.data = extract_networked(*c)
			});
		}
	};

	for (const auto eid : components.drain(component_event::added)) {
		upsert(eid);
	}
	for (const auto eid : components.drain(component_event::updated)) {
		upsert(eid);
	}
	for (const auto eid : components.drain(component_event::removed)) {
		broadcast(component_remove<T>{
			.owner_id = eid
		});
	}
}

template <typename... C, gse::access_mode... M>
auto gse::network::replicate_deltas(auto& send_fn, const std::unordered_map<address, remote_peer>& peers, const access<C, M>&... components) -> void {
	if (peers.empty()) {
		return;
	}

	(broadcast_component_deltas(send_fn, components, peers), ...);
}

template <typename T, gse::access_mode M>
auto gse::network::snapshot_components_to(auto& send_fn, const access<T, M>& components, const address& addr) -> void {
	const auto ids = components.owner_ids();
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

template <typename... C, gse::access_mode... M>
auto gse::network::replicate_snapshot_to(auto& send_fn, const address& addr, const access<C, M>&... components) -> void {
	(snapshot_components_to(send_fn, components, addr), ...);
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
