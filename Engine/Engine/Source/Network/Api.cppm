export module gse.network:api;

import std;

import :client;
import :message;

export namespace gse::network {
	auto refresh_servers(
	) -> void;

	struct connection_options {
		address addr;
		std::optional<address> local_bind;
		time_t<std::uint32_t> timeout{ seconds(5) };
		time_t<std::uint32_t> retry{ seconds(1) };
		bool allow_handoff = false;
	};

	auto connect(
		const connection_options& options
	) -> bool;

	auto disconnect(
	) -> void;

	auto drain(
		const std::function<void(message&)>& on_receive
	) -> void;

	auto state(
	) -> client::state;
}

namespace gse::network {
	std::unique_ptr<client> networked_client;
}

auto gse::network::refresh_servers() -> void {
}

auto gse::network::connect(const connection_options& options) -> bool {
	if (!networked_client) {
		if (options.local_bind) {
			networked_client = std::make_unique<client>(options.local_bind.value(), options.addr);
		} else {
			const address any_address{ "0.0.0.0", 0 };
			networked_client = std::make_unique<client>(any_address, options.addr);
		}
	}
	return networked_client->connect(options.timeout);
}

auto gse::network::disconnect() -> void {
	if (networked_client) {
		networked_client.reset();
	}
}

auto gse::network::drain(const std::function<void(message&)>& on_receive) -> void {
	if (networked_client) {
		networked_client->drain(on_receive);
	}
}

auto gse::network::state() -> client::state {
	if (networked_client) {
		return networked_client->current_state();
	}
	return client::state::disconnected;
}
