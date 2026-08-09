export module gse.network;

import std;

import gse.core;
import gse.containers;
import gse.math;
import gse.meta;
import gse.concurrency;
import gse.ecs;
import gse.assets;
import gse.os;

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

export namespace gse {
	struct set_networked_request {
		bool value = false;
	};

	struct set_authoritative_request {
		bool value = true;
	};

	struct set_local_controller_id_request {
		id controller_id;
	};

	struct activate_scene_request {
		id scene_id;
	};

	struct deactivate_active_scene_request {};

	struct camera_yaw_request {
		using result_type = angle;
		channel_promise<angle> promise;
	};
}

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

	struct [[= gse::system_state<"Network">{}]] data {
		[[= gse::shared]] client::state connection_state = client::state::disconnected;
		[[= gse::shared]] std::vector<discovery_result> available_servers;
		[[= gse::shared]] std::uint8_t connected_players = 0;
		[[= gse::shared]] std::uint8_t connected_max_players = 0;
		angle camera_yaw{};
		std::optional<channel_future<angle>> camera_yaw_future;
		std::unique_ptr<client> client_ptr;
		std::vector<std::shared_ptr<discovery_provider>> providers;
		std::vector<std::move_only_function<void(context&)>> deferred;
	};

	template <typename... Components>
	[[= gse::system_run<>{}]]
	auto run(
		context& ctx,
		shared_view<asset::data> assets_d,
		data& d,
		channel_read<connect_request, disconnect_request, add_provider_request, clear_providers_request, refresh_servers_request, send_request> requests_in,
		channel_write<camera_yaw_request, set_networked_request, set_authoritative_request, set_local_controller_id_request, deactivate_active_scene_request, activate_scene_request> requests_out,
		shared_view<actions::data> actions_d,
		entities ents,
		structural<Components>... auths
	) -> async::task<>;

	[[= gse::system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;
}

auto gse::network::shutdown(data& d) -> void {
	d.client_ptr.reset();
}

template <typename... Components>
auto gse::network::run(context& ctx, const shared_view<asset::data> assets_d, data& d, const channel_read<connect_request, disconnect_request, add_provider_request, clear_providers_request, refresh_servers_request, send_request> requests_in, const channel_write<camera_yaw_request, set_networked_request, set_authoritative_request, set_local_controller_id_request, deactivate_active_scene_request, activate_scene_request> requests_out, const shared_view<actions::data> actions_d, entities ents, structural<Components>... auths) -> async::task<> {
	((void)auths, ...);
	(ctx.template ensure_storage<Components>(), ...);

		if (d.camera_yaw_future && d.camera_yaw_future->ready()) {
			d.camera_yaw = d.camera_yaw_future->get();
		}
		d.camera_yaw_future = requests_out.push<camera_yaw_request>({});

		for (const auto& req : requests_in.of<connect_request>()) {
			if (!d.client_ptr) {
				const address bind = req.options.local_bind.value_or(address{
					.ip = "0.0.0.0",
					.port = 0,
				});
				d.client_ptr = std::make_unique<client>(bind, req.options.addr);
			}
			req.promise.fulfill(d.client_ptr->connect(req.options.timeout, req.options.retry));
		}

		for (const auto& _ : requests_in.of<disconnect_request>()) {
			d.client_ptr.reset();
		}

		for (const auto& req : requests_in.of<add_provider_request>()) {
			d.providers.emplace_back(req.provider);
		}

		for (const auto& _ : requests_in.of<clear_providers_request>()) {
			d.providers.clear();
			d.available_servers.clear();
		}

		for (const auto& req : requests_in.of<refresh_servers_request>()) {
			std::unordered_map<address, discovery_result> dedup;
			for (const auto& p : d.providers) {
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
			d.available_servers.clear();
			d.available_servers.reserve(dedup.size());
			for (auto& v : dedup | std::views::values) {
				d.available_servers.push_back(std::move(v));
			}
			std::ranges::sort(
				d.available_servers,
				[](const discovery_result& a, const discovery_result& b) {
					if (a.name != b.name) {
						return a.name < b.name;
					}
					return a.addr.port < b.addr.port;
				}
			);
		}

		if (!d.client_ptr) {
			d.connection_state = client::state::disconnected;
			return {};
		}

		for (const auto& req : requests_in.of<send_request>()) {
			req.action(*d.client_ptr);
		}

		d.client_ptr->drain([&ctx, &d, &assets_d, &ents, requests_out](raw_message& msg) {
			read_bitstream stream(msg.payload);

			const bool is_component = match_and_apply_components<type_pack<Components...>>(
				stream,
				msg.id,
				[&]<typename T>(const component_upsert<T>& m) {
					d.deferred.push_back([entity = m.owner_id, payload = m.data, assets_d, ents](context& ctx) {
						ents.ensure_active(entity);
						auto* c = ctx.add_component<T>(entity);
						apply_networked(*c, payload);
						asset::resolve_handles(*c, assets_d);
					});
				},
				[&]<typename T>(const component_remove<T>& m) {
					if (!m.owner_id.exists()) {
						return;
					}
					d.deferred.push_back([entity = m.owner_id, ents](context& ctx) {
						if constexpr (std::is_same_v<T, player_controller>) {
							if (ents.exists(entity)) {
								ents.remove(entity);
							}
						}
						else {
							ctx.remove_component<T>(entity);
						}
					});
				}
			);

			if (is_component) {
				return;
			}

			try_decode<connection_accepted>(
				stream,
				msg.id,
				[&](const auto& m) {
					requests_out.push<set_networked_request>({
						.value = true,
					});
					requests_out.push<set_authoritative_request>({
						.value = false,
					});
					requests_out.push<set_local_controller_id_request>({
						.controller_id = m.controller_id,
					});
					requests_out.push<deactivate_active_scene_request>({});
					d.client_ptr->send(server_info_request{});
					d.client_ptr->send(pong{
						.sequence = 0,
					});
				}
			) ||
				try_decode<notify_scene_change>(
					stream,
					msg.id,
					[&](const auto& m) {
						requests_out.push<activate_scene_request>({
							.scene_id = m.scene_id,
						});
						std::println("Switched to scene: {}", m.scene_id);
						d.client_ptr->send(pong{
							.sequence = 0,
						});
					}
				) ||
				try_decode<ping>(
					stream,
					msg.id,
					[&](const auto& m) {
						d.client_ptr->send(pong{
							.sequence = m.sequence,
						});
					}
				) ||
				try_decode<server_info_response>(
					stream,
					msg.id,
					[&](const auto& m) {
						d.connected_players = m.players;
						d.connected_max_players = m.max_players;
					}
				);
		});

		for (auto& def : d.deferred) {
			def(ctx);
		}
		d.deferred.clear();

		d.connection_state = d.client_ptr->current_state();

		if (d.connection_state == client::state::connected) {
			d.client_ptr->push_input(
				actions::current_state(actions_d),
				actions::axis1_ids(actions_d),
				actions::axis2_ids(actions_d),
				d.camera_yaw
			);
		}

	return {};
}
