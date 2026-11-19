export module gse.network:replication;

import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.time;
import std;

import :bitstream;
import :message;
import :registry_sync;
import :remote_peer;
import :socket;

export namespace gse::network {
	template <typename T>
	using replicated_access_t = std::conditional_t<meta::has_written_by(^^T), gse::read<T>, gse::write<T>>;

	template <typename... C>
	consteval auto owner_events_channel() -> std::meta::info;

	template <typename T, access_mode M>
	auto broadcast_component_deltas(
		auto& send_fn,
		const access<T, M>& components,
		const auto& owner_events,
		const std::unordered_map<address, remote_peer>& peers
	) -> void;

	template <typename... C, access_mode... M>
	auto replicate_deltas(
		auto& send_fn,
		const std::unordered_map<address, remote_peer>& peers,
		const auto& owner_events,
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

template <typename... C>
consteval auto gse::network::owner_events_channel() -> std::meta::info {
	std::vector<std::meta::info> events;
	for (const auto component : std::vector<std::meta::info>{ ^^C... }) {
		if (meta::has_written_by(component)) {
			events.push_back(std::meta::substitute(^^component_events, { component }));
		}
	}
	return std::meta::substitute(^^channel_read, events);
}

export namespace gse::network {
	template <typename... C>
	using owner_events_read = [:owner_events_channel<C...>():];
}

template <typename T, gse::access_mode M>
auto gse::network::broadcast_component_deltas(auto& send_fn, const access<T, M>& components, const auto& owner_events, const std::unordered_map<address, remote_peer>& peers) -> void {
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

	const auto remove = [&](const id eid) {
		broadcast(component_remove<T>{
			.owner_id = eid
		});
	};

	if constexpr (meta::has_written_by(^^T)) {
		for (const auto& events : owner_events.template of<component_events<T>>()) {
			std::ranges::for_each(events.added, upsert);
			std::ranges::for_each(events.updated, upsert);
			std::ranges::for_each(events.removed, remove);
		}
	}
	else {
		std::ranges::for_each(components.drain(component_event::added), upsert);
		std::ranges::for_each(components.drain(component_event::updated), upsert);
		std::ranges::for_each(components.drain(component_event::removed), remove);
	}
}

template <typename... C, gse::access_mode... M>
auto gse::network::replicate_deltas(auto& send_fn, const std::unordered_map<address, remote_peer>& peers, const auto& owner_events, const access<C, M>&... components) -> void {
	if (peers.empty()) {
		return;
	}

	(broadcast_component_deltas(send_fn, components, owner_events, peers), ...);
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