export module gse.runtime:network_api;

import std;

import gse.utility;
import gse.network;

import :core_api;

export namespace gse::network {
	auto add_discovery_provider(
		std::unique_ptr<discovery_provider> p
	) -> void {
		system_of<system>().add_discovery_provider(std::move(p));
	}

	auto clear_discovery_providers(
	) -> void {
		system_of<system>().clear_discovery_providers();
	}

	auto refresh_servers(
		const time_t<std::uint32_t> timeout = milliseconds(350)
	) -> void {
		system_of<system>().refresh_servers(timeout);
	}

	auto servers(
	) -> std::span<const discovery_result> {
		return system_of<system>().servers();
	}

	auto connect(
		const connection_options& options
	) -> bool {
		return system_of<system>().connect(options);
	}

	auto drain(
		const std::function<void(inbox_message&)>& handler,
		const time_t<std::uint32_t> timeout = seconds(5)
	) -> void {
		system_of<system>().drain(handler, timeout);
	}

	auto connect(
		const discovery_result& pick,
		const std::optional<address>& local = std::nullopt,
		const time_t<std::uint32_t> timeout = seconds(5),
		const time_t<std::uint32_t> retry = seconds(1)
	) -> bool {
		return system_of<system>().connect(pick, local, timeout, retry);
	}

	auto disconnect(
	) -> void {
		system_of<system>().disconnect();
	}

	auto state(
	) -> client::state {
		return system_of<system>().state();
	}

	template <typename T>
	auto send(
		const T& m
	) -> void {
		system_of<system>().send(m);
	}
}