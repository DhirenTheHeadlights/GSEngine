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

	class system_state {
	public:
		system_state() = default;

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

		auto current_state(
		) const -> client::state;

		template <typename T>
		auto send(
			const T& m
		) -> void;

		auto drain(
			const std::function<void(inbox_message&)>& handler,
			time_t<std::uint32_t> timeout = milliseconds(0)
		) -> void;

		std::unique_ptr<client> client_ptr;
		std::vector<std::unique_ptr<discovery_provider>> providers;
		std::vector<discovery_result> available_servers;
		std::mutex user_inbox_mutex;
		std::vector<inbox_message> user_inbox;
		std::vector<std::move_only_function<void(registry&)>> deferred;
	};

	struct system {
		static auto initialize(initialize_phase& phase, system_state& s) -> void;
		static auto update(update_phase& phase, system_state& s) -> void;
		static auto shutdown(shutdown_phase& phase, system_state& s) -> void;
	};
}

auto gse::network::system::initialize(initialize_phase&, system_state&) -> void {
	// WinSock initialization handled by static initializer in Socket.cppm
}

auto gse::network::system::shutdown(shutdown_phase&, system_state& s) -> void {
	s.client_ptr.reset();
	// WinSock cleanup handled by static initializer destructor in Socket.cppm
}

auto gse::network::system::update(update_phase& phase, system_state& s) -> void {
	if (!s.client_ptr) {
		return;
	}

	const auto* renderer_state = phase.try_state_of<renderer::state>();

	s.client_ptr->drain([&s, renderer_state](inbox_message& msg) {
		if (auto* rep = std::get_if<replication_message>(&msg)) {
			const std::span data(rep->payload);
			bitstream stream(data);

			match_and_apply_components(
				stream,
				rep->id,
				[&]<typename T>(const component_upsert<T>& m) {
					if constexpr (std::is_same_v<T, render_component>) {
						auto fixed_data = m.data;

						if (renderer_state) {
							for (std::uint32_t i = 0; i < fixed_data.model_count; ++i) {
								const auto res_id = fixed_data.models[i].id();
								fixed_data.models[i] = renderer_state->try_get<model>(res_id);
							}

							for (std::uint32_t i = 0; i < fixed_data.skinned_model_count; ++i) {
								const auto res_id = fixed_data.skinned_models[i].id();
								fixed_data.skinned_models[i] = renderer_state->try_get<skinned_model>(res_id);
							}
						}

						s.deferred.push_back([entity = m.owner_id, data = std::move(fixed_data)](registry& r) {
							r.ensure_exists(entity);
							r.add_deferred_action(entity, [entity, data](registry& reg) -> bool {
								if (!reg.active(entity)) {
									reg.ensure_active(entity);
									return false;
								}
								if (auto* c = reg.try_linked_object_write<T>(entity)) {
									c->networked_data() = data;
									return true;
								}
								reg.add_component<T>(entity, data);
								return true;
							});
						});
					}
					else {
						s.deferred.push_back([entity = m.owner_id, data = m.data](registry& r) {
							r.ensure_exists(entity);
							r.add_deferred_action(entity, [entity, data](registry& reg) -> bool {
								if (!reg.active(entity)) {
									reg.ensure_active(entity);
									return false;
								}
								if (auto* c = reg.try_linked_object_write<T>(entity)) {
									c->networked_data() = data;
									return true;
								}
								reg.add_component<T>(entity, data);
								return true;
							});
						});
					}
				},
				[&]<typename T>(const component_remove<T>& m) {
					if (!m.owner_id.exists()) {
						return;
					}
					s.deferred.push_back([entity = m.owner_id](registry& r) {
						r.add_deferred_action(entity, [entity](registry& reg) -> bool {
							reg.remove_link<T>(entity);
							return true;
						});
					});
				}
			);

			return;
		}

		{
			std::lock_guard lk(s.user_inbox_mutex);
			s.user_inbox.emplace_back(msg);
		}
	});

	for (auto& d : s.deferred) {
		d(*phase.registry.reg);
	}
	s.deferred.clear();

	if (s.client_ptr->current_state() == client::state::connected) {
		if (const auto* actions_state = phase.try_state_of<actions::system_state>()) {
			s.client_ptr->push_input(actions_state->current_state(), actions_state->axis1_ids(), actions_state->axis2_ids());
		}
	}
}

auto gse::network::system_state::add_discovery_provider(std::unique_ptr<discovery_provider> p) -> void {
	providers.emplace_back(std::move(p));
}

auto gse::network::system_state::clear_discovery_providers() -> void {
	providers.clear();
	available_servers.clear();
}

auto gse::network::system_state::refresh_servers(const time_t<std::uint32_t> timeout) -> void {
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

auto gse::network::system_state::servers() -> std::span<const discovery_result> {
	return available_servers;
}

auto gse::network::system_state::connect(const connection_options& options) -> bool {
	if (!client_ptr) {
		const address bind = options.local_bind.value_or(address{
			.ip = "0.0.0.0",
			.port = 0
		});
		client_ptr = std::make_unique<client>(bind, options.addr);
	}
	return client_ptr->connect(options.timeout, options.retry);
}

auto gse::network::system_state::connect(const discovery_result& pick, const std::optional<address>& local, const time_t<std::uint32_t> timeout, const time_t<std::uint32_t> retry) -> bool {
	const connection_options co{
		.addr = pick.addr,
		.local_bind = local,
		.timeout = timeout,
		.retry = retry
	};
	return connect(co);
}

auto gse::network::system_state::disconnect() -> void {
	client_ptr.reset();
}

auto gse::network::system_state::current_state() const -> client::state {
	if (client_ptr) {
		return client_ptr->current_state();
	}
	return client::state::disconnected;
}

auto gse::network::system_state::drain(const std::function<void(inbox_message&)>& handler, time_t<std::uint32_t>) -> void {
	std::vector<inbox_message> batch;

	{
		std::lock_guard lk(user_inbox_mutex);
		if (user_inbox.empty()) {
			return;
		}
		batch.swap(user_inbox);
	}

	for (auto& m : batch) {
		handler(m);
	}
}

template <typename T>
auto gse::network::system_state::send(const T& m) -> void {
	if (client_ptr) {
		client_ptr->send(m);
	}
}
