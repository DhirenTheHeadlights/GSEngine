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
export import :server_info;

export namespace gse::network {
	struct connection_options {
		address addr;
		std::optional<address> local_bind;
		time_t<std::uint32_t> timeout{ seconds(5) };
		time_t<std::uint32_t> retry{ seconds(1) };
		bool allow_handoff = false;
	};

	struct connect_request {
		using result_type = bool;
		connection_options options;
		channel_promise<bool> promise;
	};

	struct disconnect_request {};

	struct add_provider_request {
		std::shared_ptr<discovery_provider> provider;
	};

	struct clear_providers_request {};

	struct refresh_servers_request {
		time_t<std::uint32_t> timeout = milliseconds(350);
	};

	struct send_request {
		std::function<void(client&)> action;
	};

	struct key_hash {
		auto operator()(
			const address& a
		) const noexcept -> size_t;
	};

	struct key_eq {
		auto operator()(
			const address& a,
			const address& b
		) const noexcept -> bool;
	};

	struct system_state {
		client::state connection_state = client::state::disconnected;
		std::vector<discovery_result> available_servers;
		std::vector<inbox_message> user_inbox;
	};

	struct system {
		struct resources {
			std::unique_ptr<client> client_ptr;
			std::vector<std::shared_ptr<discovery_provider>> providers;
			std::vector<std::move_only_function<void(registry&)>> deferred;
		};

		static auto initialize(
			init_context& phase,
			resources& r,
			system_state& s
		) -> void;

		static auto update(
			update_context& ctx,
			resources& r,
			system_state& s
		) -> void;

		static auto shutdown(
			shutdown_context& phase,
			resources& r,
			system_state& s
		) -> void;
	};
}

auto gse::network::key_hash::operator()(const address& a) const noexcept -> size_t {
	return std::hash<std::string>{}(a.ip) ^ (static_cast<size_t>(a.port) << 1);
}

auto gse::network::key_eq::operator()(const address& a, const address& b) const noexcept -> bool {
	return a.ip == b.ip && a.port == b.port;
}

auto gse::network::system::initialize(init_context&, resources&, system_state&) -> void {}

auto gse::network::system::shutdown(shutdown_context&, resources& r, system_state&) -> void {
	r.client_ptr.reset();
}

auto gse::network::system::update(update_context& ctx, resources& r, system_state& s) -> void {
	for (const auto& req : ctx.read_channel<connect_request>()) {
		if (!r.client_ptr) {
			const address bind = req.options.local_bind.value_or(address{
				.ip = "0.0.0.0",
				.port = 0
			});
			r.client_ptr = std::make_unique<client>(bind, req.options.addr);
		}
		req.promise.fulfill(r.client_ptr->connect(req.options.timeout, req.options.retry));
	}

	for (const auto& _ : ctx.read_channel<disconnect_request>()) {
		r.client_ptr.reset();
	}

	for (const auto& req : ctx.read_channel<add_provider_request>()) {
		r.providers.emplace_back(req.provider);
	}

	for (const auto& _ : ctx.read_channel<clear_providers_request>()) {
		r.providers.clear();
		s.available_servers.clear();
	}

	for (const auto& req : ctx.read_channel<refresh_servers_request>()) {
		std::unordered_map<address, discovery_result, key_hash, key_eq> dedup;
		for (const auto& p : r.providers) {
			p->refresh(req.timeout);
			for (const auto& result : p->results()) {
				if (auto it = dedup.find(result.addr); it == dedup.end()) {
					dedup.emplace(result.addr, result);
				}
				else if (result.build >= it->second.build) {
					it->second = result;
				}
			}
		}
		s.available_servers.clear();
		s.available_servers.reserve(dedup.size());
		for (auto& v : dedup | std::views::values) {
			s.available_servers.push_back(std::move(v));
		}
		std::ranges::sort(s.available_servers, [](const discovery_result& a, const discovery_result& b) {
			if (a.name != b.name) {
				return a.name < b.name;
			}
			return a.addr.port < b.addr.port;
		});
	}

	if (!r.client_ptr) {
		s.connection_state = client::state::disconnected;
		return;
	}

	for (const auto& req : ctx.read_channel<send_request>()) {
		req.action(*r.client_ptr);
	}

	const auto* renderer_res = ctx.try_resources_of<renderer::system::resources>();

	s.user_inbox.clear();

	r.client_ptr->drain([&r, &s, renderer_res](inbox_message& msg) {
		if (auto* rep = std::get_if<replication_message>(&msg)) {
			const std::span data(rep->payload);
			bitstream stream(data);

			match_and_apply_components(
				stream,
				rep->id,
				[&]<typename T>(const component_upsert<T>& m) {
					if constexpr (std::is_same_v<T, render_component>) {
						auto fixed_data = m.data;

						if (renderer_res) {
							for (std::uint32_t i = 0; i < fixed_data.model_count; ++i) {
								const auto res_id = fixed_data.models[i].id();
								fixed_data.models[i] = renderer_res->try_get<model>(res_id);
							}

							for (std::uint32_t i = 0; i < fixed_data.skinned_model_count; ++i) {
								const auto res_id = fixed_data.skinned_models[i].id();
								fixed_data.skinned_models[i] = renderer_res->try_get<skinned_model>(res_id);
							}
						}

						r.deferred.push_back([entity = m.owner_id, data = std::move(fixed_data)](registry& reg) {
							reg.ensure_exists(entity);
							reg.add_deferred_action(entity, [entity, data](registry& reg) -> bool {
								if (!reg.active(entity)) {
									reg.ensure_active(entity);
									return false;
								}
								if (auto* c = reg.try_linked_object_write<T>(entity)) {
									c->networked_data() = data;
									return true;
								}
								auto* c = reg.add_component<T>(entity, data);
								c->networked_data() = data;
								return true;
							});
						});
					}
					else {
						r.deferred.push_back([entity = m.owner_id, data = m.data](registry& reg) {
							reg.ensure_exists(entity);
							reg.add_deferred_action(entity, [entity, data](registry& reg) -> bool {
								if (!reg.active(entity)) {
									reg.ensure_active(entity);
									return false;
								}
								if (auto* c = reg.try_linked_object_write<T>(entity)) {
									c->networked_data() = data;
									return true;
								}
								auto* c = reg.add_component<T>(entity, data);
								c->networked_data() = data;
								return true;
							});
						});
					}
				},
				[&]<typename T>(const component_remove<T>& m) {
					if (!m.owner_id.exists()) {
						return;
					}
					r.deferred.push_back([entity = m.owner_id](registry& reg) {
						reg.add_deferred_action(entity, [entity](registry& reg) -> bool {
							if constexpr (std::is_same_v<T, player_controller>) {
								if (reg.exists(entity)) {
									reg.remove(entity);
								}
							} else {
								reg.remove_link<T>(entity);
							}
							return true;
						});
					});
				}
			);

			return;
		}

		s.user_inbox.emplace_back(msg);
	});

	for (auto& d : r.deferred) {
		d(ctx.reg);
	}
	r.deferred.clear();

	s.connection_state = r.client_ptr->current_state();

	if (s.connection_state == client::state::connected) {
		if (const auto* actions_state = ctx.try_state_of<actions::system_state>()) {
			angle yaw;
			if (const auto* cam_state = ctx.try_state_of<camera::state>()) {
				yaw = cam_state->yaw;
			}
			r.client_ptr->push_input(
				actions_state->current_state(),
				actions_state->axis1_ids(),
				actions_state->axis2_ids(),
				yaw
			);
		}
	}
}
