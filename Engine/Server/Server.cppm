export module gse.server:server;

import std;

import gse.assert;
import gse.network;
import gse.physics;
import gse.graphics;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.runtime;

export namespace gse::server {
	struct client_data {
		id controller_id;
		actions::state latest_input;
		std::uint32_t last_input_sequence = 0;
	};

	template <typename MessagePack, typename... Components>
	class host {
	public:
		explicit host(
			network::config cfg
		);

		auto initialize() -> void;

		auto update(
			shared_view<world_system::data> w,
			const structural<player_controller>& controller_auth,
			const entities& ents,
			channel_write<activate_scene_request> channels,
			const network::inbound_channel_t<MessagePack>& messages_out,
			shared_view<actions::data> actions_s,
			write<Components>&... comps
		) -> void;

		template <network::is_network_message T>
		auto send(
			const T& msg,
			const network::address& to,
			bool reliable = false
		) -> void;

		template <network::is_network_message T>
		auto send_reliable(
			const T& msg,
			const network::address& to
		) -> void;

		template <network::is_network_message T>
		auto broadcast(
			const T& msg,
			bool reliable = false
		) -> void;

		auto peers() const -> const std::unordered_map<network::address, network::remote_peer>&;

		auto clients() const -> const std::unordered_map<network::address, client_data>&;

		auto host_entity() const -> std::optional<id>;

	private:
		auto accept_connection(
			shared_view<world_system::data> w,
			const structural<player_controller>& controller_auth,
			const entities& ents,
			const network::address& addr
		) -> void;

		network::config m_config;
		network::endpoint m_endpoint;
		std::unordered_map<network::address, client_data> m_clients;
		std::unordered_set<network::address> m_pending_snapshots;
		std::optional<id> m_host_entity;
		std::optional<network::address> m_host_addr;
	};
}

template <typename MessagePack, typename... Components>
gse::server::host<MessagePack, Components...>::host(network::config cfg) : m_config(std::move(cfg)) {
}

template <typename MessagePack, typename... Components>
auto gse::server::host<MessagePack, Components...>::initialize() -> void {
	if (!m_endpoint.bind(network::address{
		.ip = "0.0.0.0",
		.port = m_config.listen_port
		})) {
		std::println(std::cerr, "Server: Failed to bind socket to port {}", m_config.listen_port);
		return;
	}

	if (const auto local = m_endpoint.local_address()) {
		std::println("Server: Listening on port {}", local->port);
	}
}

template <typename MessagePack, typename... Components>
template <gse::network::is_network_message T>
auto gse::server::host<MessagePack, Components...>::send_reliable(const T& msg, const network::address& to) -> void {
	send(msg, to, true);
}

template <typename MessagePack, typename... Components>
template <gse::network::is_network_message T>
auto gse::server::host<MessagePack, Components...>::send(const T& msg, const network::address& to, const bool reliable) -> void {
	m_endpoint.send(msg, to, reliable);
}

template <typename MessagePack, typename... Components>
template <gse::network::is_network_message T>
auto gse::server::host<MessagePack, Components...>::broadcast(const T& msg, const bool reliable) -> void {
	for (const auto& addr : m_clients | std::views::keys) {
		m_endpoint.send(msg, addr, reliable);
	}
}

template <typename MessagePack, typename... Components>
auto gse::server::host<MessagePack, Components...>::update(const shared_view<world_system::data> w, const structural<player_controller>& controller_auth, const entities& ents, const channel_write<activate_scene_request> channels, const network::inbound_channel_t<MessagePack>& messages_out, const shared_view<actions::data> actions_s, write<Components>&... comps) -> void {
	auto& controllers = std::get<write<player_controller>&>(std::tie(comps...));
	const bool has_active_scene = w.active_scene.has_value() && std::ranges::find(w.scene_ids, *w.active_scene) != w.scene_ids.end();

	if (!has_active_scene && !w.scene_ids.empty()) {
		channels.push<activate_scene_request>({
			.scene_id = w.scene_ids.front(),
		});
	}

	m_endpoint.poll([&](network::inbound_message& msg) {
		network::read_bitstream stream(msg.payload);

		if (!m_endpoint.find_peer(msg.from)) {
			if (network::try_decode<network::server_info_request>(stream, msg.id, [&](const auto&) {
				m_endpoint.send(
					network::server_info_response{
						.players = static_cast<std::uint8_t>(m_clients.size()),
						.max_players = m_config.max_players,
					},
					msg.from
					);
			})) {
				return;
			}

			network::try_decode<network::connection_request>(
				stream,
				msg.id,
				[&](const auto&) {
					const std::uint8_t max_players = m_config.max_players;
					if (m_clients.size() >= max_players) {
						std::println(
							"Client [{}:{}] failed to connect (server full: {}/{})",
							msg.from.ip,
							msg.from.port,
							m_clients.size(),
							max_players
						);
						return;
					}

					m_endpoint.ensure_peer(msg.from);
					accept_connection(w, controller_auth, ents, msg.from);
					std::println(
						"Client [{}:{}] connected ({}/{})",
						msg.from.ip,
						msg.from.port,
						m_clients.size(),
						max_players
					);
				}
			);

			return;
		}

		if (network::try_decode<network::connection_request>(stream, msg.id, [&](const auto&) {
			if (auto client_it = m_clients.find(msg.from); client_it != m_clients.end()) {
				std::println("Client [{}:{}] reconnecting", msg.from.ip, msg.from.port);
				if (has_active_scene) {
					if (const auto* pc = controllers.find(client_it->second.controller_id)) {
						if (pc->controlled_entity_id.exists()) {
							ents.remove(pc->controlled_entity_id);
						}
					}
					ents.remove(client_it->second.controller_id);
				}
				m_clients.erase(client_it);
			}

			accept_connection(w, controller_auth, ents, msg.from);
		})) {
			return;
		}

		const bool handled = network::try_decode<network::ping>(
			stream,
			msg.id,
			[&](const auto& m) {
				send(
					network::pong{
						.sequence = m.sequence,
					},
					msg.from
				);
			}
		) ||
			network::try_decode<network::server_info_request>(
				stream,
				msg.id,
				[&](const auto&) {
					send(
						network::server_info_response{
							.players = static_cast<std::uint8_t>(m_clients.size()),
							.max_players = m_config.max_players,
						},
						msg.from
					);
				}
			) ||
			network::try_decode<network::input_frame>(
				stream,
				msg.id,
				[&](const auto& m) {
					auto& cd = m_clients[msg.from];
					if (m.input_sequence <= cd.last_input_sequence) {
						return;
					}

					cd.last_input_sequence = m.input_sequence;
					network::apply_input_frame(cd.latest_input, m);
				}
			);

		if (!handled) {
			network::route_inbound<MessagePack>(stream, msg, messages_out);
		}
	});

	std::optional<id> scene_requested_id;

	if (has_active_scene) {
		for (const auto& [scene_id, condition] : w.triggers) {
			for (const auto& cd : m_clients | std::views::values) {
				const auto* pc = controllers.find(cd.controller_id);
				const auto controlled_id = pc ? pc->controlled_entity_id : id{};

				evaluation_context ctx{
					.client_id = controlled_id,
					.input = &cd.latest_input,
				};
				if (condition(ctx)) {
					scene_requested_id = scene_id;
				}
			}
		}
	}

	if (scene_requested_id) {
		channels.push<activate_scene_request>({
			.scene_id = *scene_requested_id,
		});

		const network::notify_scene_change msg{
			.scene_id = *scene_requested_id,
		};

		for (const auto& addr : m_clients | std::views::keys) {
			send_reliable(msg, addr);
		}
	}

	m_endpoint.resend_reliable();

	if (has_active_scene) {
		auto send_all = [this](const auto& msg, const network::address& to) {
			this->send(msg, to);
		};

		if (!m_pending_snapshots.empty()) {
			for (const auto& addr : m_pending_snapshots) {
				network::replicate_snapshot_to(send_all, addr, comps...);
			}
			m_pending_snapshots.clear();
		}

		network::replicate_deltas(send_all, m_endpoint.peers(), comps...);
	}
}

template <typename MessagePack, typename... Components>
auto gse::server::host<MessagePack, Components...>::peers() const -> const std::unordered_map<network::address, network::remote_peer>& {
	return m_endpoint.peers();
}

template <typename MessagePack, typename... Components>
auto gse::server::host<MessagePack, Components...>::clients() const -> const std::unordered_map<network::address, client_data>& {
	return m_clients;
}

template <typename MessagePack, typename... Components>
auto gse::server::host<MessagePack, Components...>::host_entity() const -> std::optional<id> {
	return m_host_entity;
}

template <typename MessagePack, typename... Components>
auto gse::server::host<MessagePack, Components...>::accept_connection(const shared_view<world_system::data> w, const structural<player_controller>& controller_auth, const entities& ents, const network::address& addr) -> void {
	const bool has_active_scene = w.active_scene.has_value() && std::ranges::find(w.scene_ids, *w.active_scene) != w.scene_ids.end();

	id controller_id{};
	if (has_active_scene) {
		const auto controller_name = std::format("PlayerController_{}:{}", addr.ip, addr.port);
		controller_id = generate_id(controller_name);
		ents.ensure_active(controller_id);
		controller_auth.add(controller_id);
		m_clients.emplace(
			addr,
			client_data{
				.controller_id = controller_id,
			}
		);

		if (!m_host_entity.has_value()) {
			m_host_entity = controller_id;
			m_host_addr = addr;
		}
		else if (m_host_addr == addr) {
			m_host_entity = controller_id;
		}
	}

	send_reliable(
		network::connection_accepted{
			.controller_id = controller_id,
		},
		addr
	);

	if (has_active_scene) {
		send_reliable(
			network::notify_scene_change{
				.scene_id = *w.active_scene,
			},
			addr
		);
	}

	m_pending_snapshots.insert(addr);
}