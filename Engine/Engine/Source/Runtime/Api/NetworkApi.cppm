export module gse.runtime:network_api;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.network;

import :core_api;

export namespace gse::network {
	auto add_discovery_provider(
		std::shared_ptr<discovery_provider> p
	) -> void;

	auto clear_discovery_providers(
	) -> void;

	auto refresh_servers(
		time_t<std::uint32_t> timeout = milliseconds(350)
	) -> void;

	auto servers(
	) -> std::span<const discovery_result>;

	auto connect(
		const connection_options& options
	) -> channel_future<bool>;

	auto connect(
		const discovery_result& pick,
		const std::optional<address>& local = std::nullopt,
		time_t<std::uint32_t> timeout = seconds(5),
		time_t<std::uint32_t> retry = seconds(1)
	) -> channel_future<bool>;

	auto disconnect(
	) -> void;

	auto connection_state(
	) -> client::state;

	auto drain(
		const std::function<void(const inbox_message&)>& handler
	) -> void;

	template <typename T>
	auto send(
		const T& m
	) -> void;
}

auto gse::network::add_discovery_provider(std::shared_ptr<discovery_provider> p) -> void {
	channel_add<add_provider_request>({
		.provider = std::move(p)
	});
}

auto gse::network::clear_discovery_providers() -> void {
	channel_add<clear_providers_request>({});
}

auto gse::network::refresh_servers(const time_t<std::uint32_t> timeout) -> void {
	channel_add<refresh_servers_request>({
		.timeout = timeout
	});
}

auto gse::network::servers() -> std::span<const discovery_result> {
	return state_of<system_state>().available_servers;
}

auto gse::network::connect(const connection_options& options) -> channel_future<bool> {
	return channel_add<connect_request>({
		.options = options
	});
}

auto gse::network::connect(const discovery_result& pick, const std::optional<address>& local, const time_t<std::uint32_t> timeout, const time_t<std::uint32_t> retry) -> channel_future<bool> {
	return channel_add<connect_request>({
		.options = {
			.addr = pick.addr,
			.local_bind = local,
			.timeout = timeout,
			.retry = retry
		}
	});
}

auto gse::network::disconnect() -> void {
	channel_add<disconnect_request>({});
}

auto gse::network::connection_state() -> client::state {
	return state_of<system_state>().connection_state;
}

auto gse::network::drain(const std::function<void(const inbox_message&)>& handler) -> void {
	for (auto& msg : state_of<system_state>().user_inbox) {
		handler(msg);
	}
}

template <typename T>
auto gse::network::send(const T& m) -> void {
	channel_add<send_request>({
		.action = [m](client& c) {
			c.send(m);
		}
	});
}
