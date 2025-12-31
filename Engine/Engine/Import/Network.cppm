module;

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

export module gse.network;

import std;
import gse.utility;
import gse.physics; 
import gse.graphics;

export import :actions;
export import :remote_peer;
export import :socket;
export import :bitstream;
export import :packet_header;
export import :message;
export import :connection;
export import :ping_pong;
export import :input_frame;
export import :notify_scene_change;
export import :client;
export import :discovery;
export import :registry_sync;
export import :replication;

export namespace gse::network {
	struct connection_options {
		address addr;
		std::optional<address> local_bind;
		time_t<std::uint32_t> timeout{ seconds(5) };
		time_t<std::uint32_t> retry{ seconds(1) };
		bool allow_handoff = false;
	};

	using net_read_set = read_set<physics::motion_component, render_component>;
	using net_write_set = write_set<physics::motion_component, render_component>;

	class system final : public ecs_system<net_read_set, net_write_set> {
	public:
		using schedule = system_schedule<
			system_stage<
				system_stage_kind::update,
				net_read_set,
				net_write_set
			>,
			system_stage<
				system_stage_kind::shutdown,
				gse::read_set<>,
				gse::write_set<>
			>
		>;

		using ecs_system::ecs_system;

		~system(
		) override = default;

		auto initialize(
		) -> void override;

		auto update(
		) -> void override;

		auto shutdown(
		) -> void override;

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

		auto disconnect(
		) -> void;

		auto state(
		) const -> client::state;

		template <typename T>
		auto send(
			const T& m
		) -> void;
	private:
		std::unique_ptr<client> m_client;
		std::vector<std::unique_ptr<discovery_provider>> m_providers;
		std::vector<discovery_result> m_available_servers;

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
	};
}

auto gse::network::system::initialize() -> void {
	WSADATA wsa_data;
	if (const auto result = WSAStartup(MAKEWORD(2, 2), &wsa_data); result != 0) {
		std::println("WSAStartup failed: {}", result);
	}
}

auto gse::network::system::shutdown() -> void {
	m_client.reset();
	if (const auto result = WSACleanup(); result != 0) {
		std::println("WSACleanup failed: {}", result);
	}
}

auto gse::network::system::update() -> void {
	if (!m_client) {
		return;
	}

	m_client->drain([&, this](const inbox_message& msg) {
		match(msg)
			.if_is<connection_accepted>([](const connection_accepted&) {
				std::println("Connected to server.");
			})
			.else_if_is<replication_message>([&](replication_message& rep) {
				const std::span data(rep.payload);
				bitstream stream(data);

				match_and_apply_components(
					stream, 
					rep.id,
					[&]<typename T>(const component_upsert<T>& m) {
						this->upsert_component<T>(m.owner_id, m.data);
					},
					[&]<typename T>(const component_remove<T>& m) {
						this->remove_component<T>(m.owner_id);
					}
				);
			});
	});
}

auto gse::network::system::add_discovery_provider(std::unique_ptr<discovery_provider> p) -> void {
	m_providers.emplace_back(std::move(p));
}

auto gse::network::system::clear_discovery_providers() -> void {
	m_providers.clear();
	m_available_servers.clear();
}

auto gse::network::system::refresh_servers(const time_t<std::uint32_t> timeout) -> void {
	std::unordered_map<address, discovery_result, key_hash, key_eq> dedup;
	for (const auto& p : m_providers) {
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
	m_available_servers.clear();
	m_available_servers.reserve(dedup.size());
	for (auto& v : dedup | std::views::values) {
		m_available_servers.push_back(std::move(v));
	}
	std::ranges::sort(m_available_servers, [](const discovery_result& a, const discovery_result& b) {
		if (a.name != b.name) {
			return a.name < b.name;
		}
		return a.addr.port < b.addr.port;
	});
}

auto gse::network::system::servers() -> std::span<const discovery_result> {
	return m_available_servers;
}

auto gse::network::system::connect(const connection_options& options) -> bool {
	if (!m_client) {
		const address bind = options.local_bind.value_or(address{
			.ip = "0.0.0.0",
			.port = 0
		});
		m_client = std::make_unique<client>(bind, options.addr);
	}
	return m_client->connect(options.timeout, options.retry);
}

auto gse::network::system::connect(const discovery_result& pick, const std::optional<address>& local, const time_t<std::uint32_t> timeout, const time_t<std::uint32_t> retry) -> bool {
	const connection_options co{
		.addr = pick.addr,
		.local_bind = local,
		.timeout = timeout,
		.retry = retry
	};
	return connect(co);
}

auto gse::network::system::disconnect() -> void {
	m_client.reset();
}

auto gse::network::system::state() const -> client::state {
	if (m_client) {
		return m_client->current_state();
	}
	return client::state::disconnected;
}

template <typename T>
auto gse::network::system::send(const T& m) -> void {
	if (m_client) {
		m_client->send(m);
	}
}