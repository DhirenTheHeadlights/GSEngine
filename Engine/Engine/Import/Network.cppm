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

	struct data {
		client::state connection_state = client::state::disconnected;
		std::vector<discovery_result> available_servers;
		std::uint8_t connected_players = 0;
		std::uint8_t connected_max_players = 0;
		angle camera_yaw{};
		std::unique_ptr<client> client_ptr;
		std::vector<std::shared_ptr<discovery_provider>> providers;
		std::vector<gse::move_only_function<void(run_context&)>> deferred;
	};

	template <typename... Components>
	struct system {
		using data = network::data;

		static auto run(
			run_context& ctx,
			const asset::data& assets_d,
			data& d,
			const actions::system::data& actions_d
		) -> async::task<>;

		static auto shutdown(
			shutdown_context& phase,
			data& d
		) -> void;
	};

	template <typename Pack>
	using system_for = typename Pack::template apply<system>;
}

template <typename... Components>
auto gse::network::system<Components...>::shutdown(shutdown_context&, data& d) -> void {
	d.client_ptr.reset();
}

template <typename... Components>
auto gse::network::system<Components...>::run(run_context& ctx, const asset::data& assets_d, data& d, const actions::system::data& actions_d) -> async::task<> {
	(ctx.template ensure_storage<Components>(), ...);

	while (true) {
		for (const auto& response : ctx.read_channel<camera_yaw_response>()) {
			d.camera_yaw = response.yaw;
		}
		ctx.channels.push<camera_yaw_request>({});

		for (const auto& req : ctx.read_channel<connect_request>()) {
			if (!d.client_ptr) {
				const address bind = req.options.local_bind.value_or(address{
					.ip = "0.0.0.0",
					.port = 0,
				});
				d.client_ptr = std::make_unique<client>(bind, req.options.addr);
			}
			req.promise.fulfill(d.client_ptr->connect(req.options.timeout, req.options.retry));
		}

		for (const auto& _ : ctx.read_channel<disconnect_request>()) {
			d.client_ptr.reset();
		}

		for (const auto& req : ctx.read_channel<add_provider_request>()) {
			d.providers.emplace_back(req.provider);
		}

		for (const auto& _ : ctx.read_channel<clear_providers_request>()) {
			d.providers.clear();
			d.available_servers.clear();
		}

		for (const auto& req : ctx.read_channel<refresh_servers_request>()) {
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
			co_await ctx.next_tick();
			continue;
		}

		for (const auto& req : ctx.read_channel<send_request>()) {
			req.action(*d.client_ptr);
		}

		d.client_ptr->drain([&ctx, &d, &assets_d](raw_message& msg) {
			read_bitstream stream(msg.payload);

			const bool is_component = match_and_apply_components<type_pack<Components...>>(
				stream,
				msg.id,
				[&]<typename T>(const component_upsert<T>& m) {
					d.deferred.push_back([entity = m.owner_id, payload = m.data, &assets_d](run_context& ctx) {
						ctx.ensure_active(entity);
						auto* c = ctx.add_component<T>(entity);
						apply_networked(*c, payload);
						asset::resolve_handles(*c, assets_d);
					});
				},
				[&]<typename T>(const component_remove<T>& m) {
					if (!m.owner_id.exists()) {
						return;
					}
					d.deferred.push_back([entity = m.owner_id](run_context& ctx) {
						if constexpr (std::is_same_v<T, player_controller>) {
							if (ctx.exists(entity)) {
								ctx.remove(entity);
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
					ctx.channels.push<set_networked_request>({
						.value = true,
					});
					ctx.channels.push<set_authoritative_request>({
						.value = false,
					});
					ctx.channels.push<set_local_controller_id_request>({
						.controller_id = m.controller_id,
					});
					ctx.channels.push<deactivate_active_scene_request>({});
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
						ctx.channels.push<activate_scene_request>({
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
				actions::system::current_state(actions_d),
				actions::system::axis1_ids(actions_d),
				actions::system::axis2_ids(actions_d),
				d.camera_yaw
			);
		}

		co_await ctx.next_tick();
	}
}
