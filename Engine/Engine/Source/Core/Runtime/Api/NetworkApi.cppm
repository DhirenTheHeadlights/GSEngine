export module gse.runtime:network_api;

import std;

import gse.utility;
import gse.network;

import :core_api;

export namespace gse::network {
	auto add_discovery_provider(
		std::unique_ptr<discovery_provider> p
	) -> void {
		state_of<system_state>().add_discovery_provider(std::move(p));
	}

	auto clear_discovery_providers(
	) -> void {
		state_of<system_state>().clear_discovery_providers();
	}

	auto refresh_servers(
		const time_t<std::uint32_t> timeout = milliseconds(350)
	) -> void {
		state_of<system_state>().refresh_servers(timeout);
	}

	auto servers(
	) -> std::span<const discovery_result> {
		return state_of<system_state>().servers();
	}

	auto connect(
		const connection_options& options
	) -> bool {
		return state_of<system_state>().connect(options);
	}

	auto drain(
		const std::function<void(inbox_message&)>& handler,
		const time_t<std::uint32_t> timeout = seconds(5)
	) -> void {
		state_of<system_state>().drain(handler, timeout);
	}

	auto connect(
		const discovery_result& pick,
		const std::optional<address>& local = std::nullopt,
		const time_t<std::uint32_t> timeout = seconds(5),
		const time_t<std::uint32_t> retry = seconds(1)
	) -> bool {
		return state_of<system_state>().connect(pick, local, timeout, retry);
	}

	auto disconnect(
	) -> void {
		state_of<system_state>().disconnect();
	}

	auto state(
	) -> client::state {
		return state_of<system_state>().current_state();
	}

	template <typename T>
	auto send(
		const T& m
	) -> void {
		state_of<system_state>().send(m);
	}
}