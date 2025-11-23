export module gse.network:api;

import std;

import gse.utility;

import :client;
import :message;
import :discovery;

export namespace gse::network {
    struct connection_options {
        address addr;
        std::optional<address> local_bind;
        time_t<std::uint32_t> timeout{ seconds(5) };
        time_t<std::uint32_t> retry{ seconds(1) };
        bool allow_handoff = false;
    };

    auto add_discovery_provider(
        std::unique_ptr<discovery_provider> p
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
    ) -> bool;

    auto connect(
        const discovery_result& pick,
        const std::optional<address>& local = std::nullopt,
        time_t<std::uint32_t> timeout = seconds(5),
        time_t<std::uint32_t> retry = seconds(1)
    ) -> bool;

    auto update_registry(
		registry& reg
	) -> void;

    auto disconnect(
    ) -> void;

    auto drain(
        const std::function<void(inbox_message&)>& on_receive
    ) -> void;

    auto state(
    ) -> client::state;

    template <typename T>
    auto send(
        const T& m
    ) -> void;
}

namespace gse::network {
    std::unique_ptr<client> networked_client;
    std::vector<std::unique_ptr<discovery_provider>> providers;
    std::vector<discovery_result> available_servers;

    struct key_hash {
        auto operator()(const address& a) const noexcept -> size_t {
            return std::hash<std::string>{}(a.ip) ^ (static_cast<size_t>(a.port) << 1);
        }
    };

    struct key_eq {
        auto operator()(const address& a, const address& b) const noexcept -> bool {
            return a.ip == b.ip && a.port == b.port;
        }
    };
}

auto gse::network::connect(const connection_options& options) -> bool {
    if (!networked_client) {
        const address bind = options.local_bind.value_or({
            .ip = "0.0.0.0",
        	.port = 0
        });
        networked_client = std::make_unique<client>(bind, options.addr);
    }
    return networked_client->connect(options.timeout, options.retry);
}

auto gse::network::connect(const discovery_result& pick, const std::optional<address>& local, const time_t<std::uint32_t> timeout, const time_t<std::uint32_t> retry) -> bool {
    const connection_options co {
        .addr = pick.addr,
        .local_bind = local,
        .timeout = timeout,
        .retry = retry
    };
    return connect(co);
}

auto gse::network::update_registry(registry& reg) -> void {
	if (networked_client) {
        networked_client->set_registry(reg);
    }
}

auto gse::network::disconnect() -> void {
    if (networked_client) {
	    networked_client.reset();
    }
}

auto gse::network::drain(const std::function<void(inbox_message&)>& on_receive) -> void {
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

auto gse::network::add_discovery_provider(std::unique_ptr<discovery_provider> p) -> void {
    providers.emplace_back(std::move(p));
}

auto gse::network::clear_discovery_providers() -> void {
    providers.clear();
    available_servers.clear();
}

auto gse::network::refresh_servers(const time_t<std::uint32_t> timeout) -> void {
    std::unordered_map<address, discovery_result, key_hash, key_eq> dedup;
    for (const auto& p : providers) {
        p->refresh(timeout);
        for (const auto& r : p->results()) {
	        if (auto it = dedup.find(r.addr); it == dedup.end()) {
		        dedup.emplace(r.addr, r);
	        }
            else if (r.build >= it->second.build) {
	            it->second = r;
            }
        }
    }
    available_servers.clear();
    available_servers.reserve(dedup.size());
    for (auto& v : dedup | std::views::values) {
	    available_servers.push_back(std::move(v));
    }
    std::ranges::sort(available_servers, [](const discovery_result& a, const discovery_result& b) {
        if (a.name != b.name) {
	        return a.name < b.name;
        }
        return a.addr.port < b.addr.port;
    });
}

auto gse::network::servers() -> std::span<const discovery_result> {
    return available_servers;
}

template <typename T>
auto gse::network::send(const T& m) -> void {
    if (networked_client) networked_client->send(m);
}
