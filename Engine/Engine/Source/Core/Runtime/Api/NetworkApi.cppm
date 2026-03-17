export module gse.runtime:network_api;

import std;

import gse.utility;
import gse.network;

import :core_api;

export namespace gse::network {
	auto add_discovery_provider(
		std::unique_ptr<discovery_provider> p
	) -> void {
		defer<system_state>([p = std::move(p)](system_state& s) mutable {
			s.add_discovery_provider(std::move(p));
		});
	}

	auto clear_discovery_providers(
	) -> void {
		defer<system_state>([](system_state& s) {
			s.clear_discovery_providers();
		});
	}

	auto refresh_servers(
		const time_t<std::uint32_t> timeout = milliseconds(350)
	) -> void {
		defer<system_state>([timeout](system_state& s) {
			s.refresh_servers(timeout);
		});
	}

	auto servers(
	) -> std::span<const discovery_result> {
		return state_of<system_state>().servers();
	}

	auto connect(
		const connection_options& options
	) -> void {
		defer<system_state>([options](system_state& s) {
			s.connect(options);
		});
	}

	auto drain(
		const std::function<void(inbox_message&)>& handler,
		const time_t<std::uint32_t> timeout = seconds(5)
	) -> void {
		defer<system_state>([handler, timeout](system_state& s) {
			s.drain(handler, timeout);
		});
	}

	auto connect(
		const discovery_result& pick,
		const std::optional<address>& local = std::nullopt,
		const time_t<std::uint32_t> timeout = seconds(5),
		const time_t<std::uint32_t> retry = seconds(1)
	) -> void {
		defer<system_state>([pick, local, timeout, retry](system_state& s) {
			s.connect(pick, local, timeout, retry);
		});
	}

	auto disconnect(
	) -> void {
		defer<system_state>([](system_state& s) {
			s.disconnect();
		});
	}

	auto state(
	) -> client::state {
		return state_of<system_state>().current_state();
	}

	template <typename T>
	auto send(
		const T& m
	) -> void {
		defer<system_state>([m](system_state& s) {
			s.send(m);
		});
	}
}
