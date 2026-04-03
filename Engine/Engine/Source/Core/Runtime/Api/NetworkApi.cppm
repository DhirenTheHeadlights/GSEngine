export module gse.runtime:network_api;

import std;

import gse.utility;
import gse.network;

import :core_api;

export namespace gse::network {
	auto add_discovery_provider(
		std::unique_ptr<discovery_provider> p
	) -> void;

	auto clear_discovery_providers(
	) -> void;

	auto refresh_servers(
		const time_t<std::uint32_t> timeout = milliseconds(350)
	) -> void;

	auto servers(
	) -> std::span<const discovery_result>;

	auto connect(
		const connection_options& options
	) -> void;

	auto drain(
		const std::function<void(inbox_message&)>& handler,
		const time_t<std::uint32_t> timeout = seconds(5)
	) -> void;

	auto connect(
		const discovery_result& pick,
		const std::optional<address>& local = std::nullopt,
		const time_t<std::uint32_t> timeout = seconds(5),
		const time_t<std::uint32_t> retry = seconds(1)
	) -> void;

	auto disconnect(
	) -> void;

	auto state(
	) -> client::state;

	template <typename T>
	auto send(
		const T& m
	) -> void;
}

auto gse::network::add_discovery_provider(std::unique_ptr<discovery_provider> p) -> void {
	defer([p = std::move(p)](system_state& s) mutable {
		s.add_discovery_provider(std::move(p));
	});
}

auto gse::network::clear_discovery_providers() -> void {
	defer([](system_state& s) {
		s.clear_discovery_providers();
	});
}

auto gse::network::refresh_servers(const time_t<std::uint32_t> timeout) -> void {
	defer([timeout](system_state& s) {
		s.refresh_servers(timeout);
	});
}

auto gse::network::servers() -> std::span<const discovery_result> {
	return state_of<system_state>().servers();
}

auto gse::network::connect(const connection_options& options) -> void {
	defer([options](system_state& s) {
		s.connect(options);
	});
}

auto gse::network::drain(const std::function<void(inbox_message&)>& handler, const time_t<std::uint32_t> timeout) -> void {
	defer([handler, timeout](system_state& s) {
		s.drain(handler, timeout);
	});
}

auto gse::network::connect(const discovery_result& pick, const std::optional<address>& local, const time_t<std::uint32_t> timeout, const time_t<std::uint32_t> retry) -> void {
	defer([pick, local, timeout, retry](system_state& s) {
		s.connect(pick, local, timeout, retry);
	});
}

auto gse::network::disconnect() -> void {
	defer([](system_state& s) {
		s.disconnect();
	});
}

auto gse::network::state() -> client::state {
	return state_of<system_state>().current_state();
}

template <typename T>
auto gse::network::send(const T& m) -> void {
	defer([m](system_state& s) {
		s.send(m);
	});
}
