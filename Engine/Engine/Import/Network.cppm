export module gse.network;

import std;

import gse.core;
import gse.containers;
import gse.math;
import gse.meta;
import gse.time;
import gse.log;
import gse.concurrency;
import gse.ecs;
import gse.assets;
import gse.os;

export import :actions;
export import :remote_peer;
export import :socket;
export import :config;
export import :endpoint;
export import :dispatch;
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
}

export namespace gse::network {
	struct connection_options {
		address addr;
		std::optional<address> local_bind;
		time timeout{ seconds(5.f) };
		time retry{ seconds(1.f) };
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
		time timeout = milliseconds(350.f);
	};

	struct refresh_server_info_request {};

	struct ping_request {
		std::uint32_t sequence = 0;
	};

	struct remember_server_request {
		std::string entry;
	};

	struct forget_server_request {
		std::string entry;
	};

	struct [[= system_state<"Network">{}, = settings::category<"Network">{}]] data {
		[[= settings::describe<"Servers remembered by the Network screen, comma separated host:port entries.">{}]]
		std::string saved_servers;
		std::string saved_servers_seen;
		std::shared_ptr<wan_directory_provider> saved_provider;
		[[= shared]] client::state connection_state = client::state::disconnected;
		[[= shared]] std::string connection_status;
		[[= shared]] std::vector<discovery_result> available_servers;
		[[= shared]] std::uint8_t connected_players = 0;
		[[= shared]] std::uint8_t connected_max_players = 0;
		std::unique_ptr<client> client_ptr;
		id accepted_controller{};
		id announced_scene{};
		std::vector<std::shared_ptr<discovery_provider>> providers;
		std::vector<std::move_only_function<void(context&)>> deferred;
		bool auto_connect_pending = true;
		bool auto_connect_rejected = false;
		interval_timer<> auto_connect_timer{ seconds(2.f) };
		interval_timer<> stats_timer{ seconds(2.f) };
		std::uint32_t stat_messages = 0;
		std::uint32_t stat_upserts = 0;
		std::uint32_t stat_removes = 0;
		std::unordered_map<std::string_view, std::uint32_t> stat_upserts_by_type;
	};

	template <typename MessagePack, typename... Components>
	[[= system_run<>{}]]
	auto run(
		context& ctx,
		shared_view<asset::data> assets_d,
		data& d,
		const config& net_cfg,
		outbound_channel_t<MessagePack, connect_request, disconnect_request, add_provider_request, clear_providers_request, refresh_servers_request, refresh_server_info_request, ping_request, remember_server_request, forget_server_request> requests_in,
		channel_write<set_networked_request, set_authoritative_request, set_local_controller_id_request, deactivate_active_scene_request, activate_scene_request> requests_out,
		inbound_channel_t<MessagePack> messages_out,
		entities ents,
		structural<Components>... auths
	) -> async::task<>;

	[[= system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;
}

auto gse::network::shutdown(data& d) -> void {
	d.client_ptr.reset();
}

namespace gse::network {
	auto saved_server_entries(
		std::string_view csv
	) -> std::vector<std::string>;

	auto join_saved_servers(
		std::span<const std::string> entries
	) -> std::string;
}

auto gse::network::saved_server_entries(const std::string_view csv) -> std::vector<std::string> {
	std::vector<std::string> out;
	for (const auto part : csv | std::views::split(',')) {
		std::string_view entry(part.begin(), part.end());
		while (!entry.empty() && entry.front() == ' ') {
			entry.remove_prefix(1);
		}
		while (!entry.empty() && entry.back() == ' ') {
			entry.remove_suffix(1);
		}
		if (!entry.empty()) {
			out.emplace_back(entry);
		}
	}
	return out;
}

auto gse::network::join_saved_servers(const std::span<const std::string> entries) -> std::string {
	std::string out;
	for (const auto& entry : entries) {
		if (!out.empty()) {
			out += ',';
		}
		out += entry;
	}
	return out;
}

template <typename MessagePack, typename... Components>
auto gse::network::run(context& ctx, const shared_view<asset::data> assets_d, data& d, const config& net_cfg, const outbound_channel_t<MessagePack, connect_request, disconnect_request, add_provider_request, clear_providers_request, refresh_servers_request, refresh_server_info_request, ping_request, remember_server_request, forget_server_request> requests_in, const channel_write<set_networked_request, set_authoritative_request, set_local_controller_id_request, deactivate_active_scene_request, activate_scene_request> requests_out, const inbound_channel_t<MessagePack> messages_out, entities ents, structural<Components>... auths) -> async::task<> {
	((void)auths, ...);
	(ctx.template ensure_storage<Components>(), ...);

	if (!net_cfg.connect.empty() && !d.auto_connect_rejected) {
		const bool retry_due = d.auto_connect_timer.tick();
		const bool idle = !d.client_ptr || d.client_ptr->current_state() == client::state::disconnected;

		if (idle && (d.auto_connect_pending || retry_due)) {
			d.auto_connect_pending = false;

			const auto parsed = parse_address(net_cfg.connect, default_port);
			const auto addr = parsed ? resolve_address(*parsed) : std::nullopt;
			if (addr) {
				const time timeout = seconds(5.f);
				const time retry = seconds(1.f);

				if (!d.client_ptr) {
					d.client_ptr = std::make_unique<client>(
						address{
							.ip = "0.0.0.0",
							.port = 0,
						},
						*addr
					);
				}
				if (d.client_ptr->connect(timeout, retry)) {
					d.connection_status = std::format("Connecting to {}:{}...", addr->ip, addr->port);
				}
			}
			else {
				d.auto_connect_rejected = true;
				if (parsed) {
					log::println(
						log::level::error,
						log::category::network,
						"net connect target '{}' could not be resolved",
						net_cfg.connect
					);
				}
				else {
					log::println(
						log::level::error,
						log::category::network,
						"net connect target '{}' is not a valid address: {}",
						net_cfg.connect,
						parsed.error()
					);
				}
			}
		}
	}

	for (const auto& req : requests_in.template of<connect_request>()) {
		if (!d.client_ptr || d.client_ptr->server_address() != req.options.addr) {
			const address bind = req.options.local_bind.value_or(address{
				.ip = "0.0.0.0",
				.port = 0,
			});
			d.client_ptr = std::make_unique<client>(bind, req.options.addr);
			d.accepted_controller = {};
			d.announced_scene = {};
		}
		const bool started = d.client_ptr->connect(req.options.timeout, req.options.retry);
		if (started) {
			const auto& target = d.client_ptr->server_address();
			d.connection_status = std::format("Connecting to {}:{}...", target.ip, target.port);
		}
		req.promise.fulfill(started);
	}

	for (const auto& _ : requests_in.template of<disconnect_request>()) {
		d.client_ptr.reset();
		d.accepted_controller = {};
		d.announced_scene = {};
		d.connection_status = "Disconnected";
	}

	for (const auto& _ : requests_in.template of<clear_providers_request>()) {
		d.providers.clear();
		d.available_servers.clear();
	}

	for (const auto& req : requests_in.template of<add_provider_request>()) {
		d.providers.emplace_back(req.provider);
	}

	for (const auto& req : requests_in.template of<remember_server_request>()) {
		const auto parsed = parse_address(req.entry, default_port);
		if (!parsed) {
			log::println(log::level::warning, log::category::network, "cannot remember server '{}': {}", req.entry, parsed.error());
			continue;
		}
		auto entries = saved_server_entries(d.saved_servers);
		const std::string entry = std::format("{}:{}", parsed->ip, parsed->port);
		if (std::ranges::find(entries, entry) == entries.end()) {
			entries.push_back(entry);
			d.saved_servers = join_saved_servers(entries);
		}
	}

	for (const auto& req : requests_in.template of<forget_server_request>()) {
		auto entries = saved_server_entries(d.saved_servers);
		if (std::erase(entries, req.entry) > 0) {
			d.saved_servers = join_saved_servers(entries);
		}
	}

	if (d.saved_servers != d.saved_servers_seen) {
		d.saved_servers_seen = d.saved_servers;
		std::vector<discovery_result> seed;
		for (const auto& entry : saved_server_entries(d.saved_servers)) {
			if (const auto parsed = parse_address(entry, default_port)) {
				seed.push_back({
					.addr = *parsed,
					.name = entry,
					.max_players = net_cfg.max_players,
				});
			}
		}
		d.saved_provider = seed.empty() ? nullptr : std::make_shared<wan_directory_provider>(std::move(seed));
	}

	for (const auto& req : requests_in.template of<refresh_servers_request>()) {
		std::unordered_map<address, discovery_result> dedup;
		const auto gather = [&](discovery_provider& p) {
			p.refresh(req.timeout);
			for (const auto& result : p.results()) {
				if (auto it = dedup.find(result.addr); it == dedup.end()) {
					dedup.emplace(result.addr, result);
				}
				else if (result.build >= it->second.build) {
					it->second = result;
				}
			}
		};
		for (const auto& p : d.providers) {
			gather(*p);
		}
		if (d.saved_provider) {
			gather(*d.saved_provider);
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

	for (const auto& _ : requests_in.template of<refresh_server_info_request>()) {
		d.client_ptr->send(server_info_request{});
	}

	for (const auto& req : requests_in.template of<ping_request>()) {
		d.client_ptr->send(ping{
			.sequence = req.sequence,
		});
	}

	drain_outbound<MessagePack>(
		requests_in,
		[&d](const auto& msg, const std::optional<address>&, const bool reliable) {
			d.client_ptr->send(msg, reliable);
		}
	);

	d.client_ptr->poll([&ctx, &d, &assets_d, &ents, requests_out, messages_out](inbound_message& msg) {
		read_bitstream stream(msg.payload);
		++d.stat_messages;

		const bool is_component = match_and_apply_components<type_pack<Components...>>(
			stream,
			msg.id,
			[&]<typename T>(const component_upsert<T>& m) {
				++d.stat_upserts;
				++d.stat_upserts_by_type[type_tag<T>()];
				d.deferred.push_back([entity = m.owner_id, payload = m.data, assets_d, ents](context& ctx) {
					ents.ensure_active(entity);
					auto* c = ctx.add_component<T>(entity);
					apply_networked(*c, payload);
					asset::resolve_handles(*c, assets_d);
				});
			},
			[&]<typename T>(const component_remove<T>& m) {
				++d.stat_removes;
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
						if (ents.exists(entity) && !ents.has_components(entity)) {
							ents.remove(entity);
						}
					}
				});
			}
		);

		if (is_component) {
			return;
		}

		const bool handled = try_decode<connection_accepted>(
			stream,
			msg.id,
			[&](const auto& m) {
				if (m.controller_id == d.accepted_controller) {
					return;
				}
				d.accepted_controller = m.controller_id;
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
						if (m.scene_id != d.announced_scene) {
							d.announced_scene = m.scene_id;
							log::println(log::category::network, "Server switched us to scene {}", m.scene_id);
						}
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

		if (!handled) {
			route_inbound<MessagePack>(stream, msg, messages_out);
		}
	});

	for (auto& def : d.deferred) {
		def(ctx);
	}
	d.deferred.clear();

	d.client_ptr->tick();
	const client::state previous_state = d.connection_state;
	d.connection_state = d.client_ptr->current_state();

	if (previous_state != d.connection_state) {
		const auto& target = d.client_ptr->server_address();
		if (d.connection_state == client::state::connected) {
			d.connection_status = std::format("Connected to {}:{}", target.ip, target.port);
		}
		else if (previous_state == client::state::connecting) {
			d.connection_status = std::format("Connection to {}:{} timed out: no reply from the server", target.ip, target.port);
		}
	}

	if (d.stats_timer.tick() && d.connection_state == client::state::connected) {
		d.client_ptr->send(server_info_request{});
		std::string by_type;
		for (const auto& [name, count] : d.stat_upserts_by_type) {
			by_type += std::format("{} {}, ", name, count);
		}
		log::println(
			log::category::network,
			"client net: {} messages, {} component upserts ({}) {} component removes in the last window, {} packets dropped at the socket so far",
			d.stat_messages,
			d.stat_upserts,
			by_type,
			d.stat_removes,
			d.client_ptr->dropped()
		);
		d.stat_messages = 0;
		d.stat_upserts = 0;
		d.stat_removes = 0;
		d.stat_upserts_by_type.clear();
	}

	return {};
}