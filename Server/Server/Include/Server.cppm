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

export namespace gse {
	struct client_data {
		id controller_id;
		actions::state latest_input;
		std::uint32_t last_input_sequence = 0;
		gse::angle camera_yaw{};
		bool have_seq = false;
	};

	struct incoming_packet {
		network::address from;
		std::size_t size = 0;
		std::array<std::byte, max_packet_size> buffer{};
	};

	struct outgoing_packet {
		network::address to;
		std::size_t size = 0;
		std::array<std::byte, max_packet_size> buffer{};
	};

	template <typename... Components>
	class server {
	public:
		explicit server(
			std::uint16_t port
		);

		auto initialize(
		) -> void;

		auto update(
			world& w,
			channel_writer& channels,
			const actions::system::data& actions_s
		) -> void;

		template <typename T>
		auto send(
			const T& msg,
			const network::address& to,
			bool reliable = false
		) -> void;

		template <typename T>
		auto send_reliable(
			const T& msg,
			const network::address& to
		) -> void;

		auto peers(
		) const -> const std::unordered_map<network::address, network::remote_peer>&;

		auto clients(
		) const -> const std::unordered_map<network::address, client_data>&;

		auto host_entity(
		) const -> std::optional<id>;

		auto host_address(
		) const -> std::optional<network::address>;
	private:
		auto accept_connection(
			world& w,
			const network::address& addr
		) -> void;

		std::uint16_t m_port;
		network::udp_socket m_socket;
		std::unordered_map<network::address, network::remote_peer> m_peers;
		std::unordered_map<network::address, client_data> m_clients;
		std::unordered_set<network::address> m_pending_snapshots;
		std::optional<id> m_host_entity;
		std::optional<network::address> m_host_addr;
		spsc_ring_buffer<incoming_packet, 1024> m_incoming;
		mpsc_ring_buffer<outgoing_packet, 1024> m_outgoing;
		std::jthread m_thread;

		auto resend_reliable_messages(
		) -> void;

		static constexpr std::uint32_t reliable_retry_interval_ms = 200;
	};
}

template <typename... Components>
gse::server<Components...>::server(const std::uint16_t port) : m_port(port) {}

template <typename... Components>
auto gse::server<Components...>::initialize() -> void {
	if (!m_socket.bind(network::address{
		.ip = "0.0.0.0",
		.port = m_port
	})) {
		std::println(std::cerr, "Server: Failed to bind socket to port {}", m_port);
		return;
	}

	if (const auto local = m_socket.local_address()) {
		std::println("Server: Listening on port {}", local->port);
	}

	m_thread = std::jthread([this](const std::stop_token& st) {
		std::array<std::byte, max_packet_size> buffer;
		constexpr time_t<std::uint32_t> max_sleep = milliseconds(8);

		while (!st.stop_requested()) {
			(void)m_socket.wait_readable(max_sleep);

			while (const auto received = m_socket.receive_data(buffer)) {
				incoming_packet pkt;
				pkt.from = received->from;
				pkt.size = received->bytes_read;
				std::memcpy(pkt.buffer.data(), buffer.data(), received->bytes_read);
				if (!m_incoming.push(pkt)) {
					break;
				}
			}

			outgoing_packet out_pkt;
			while (m_outgoing.pop(out_pkt)) {
				const network::packet out{
					.data = reinterpret_cast<std::uint8_t*>(out_pkt.buffer.data()),
					.size = out_pkt.size
				};

				m_socket.send_data(out, out_pkt.to);
			}
		}
	});
}

template <typename... Components>
template <typename T>
auto gse::server<Components...>::send_reliable(const T& msg, const network::address& to) -> void {
	send(msg, to, true);
}

template <typename... Components>
template <typename T>
auto gse::server<Components...>::send(const T& msg, const network::address& to, const bool reliable) -> void {
	const auto it = m_peers.find(to);
	if (it == m_peers.end()) {
		return;
	}

	auto& peer = it->second;

	std::array<std::byte, max_packet_size> buffer;
	const packet_header header{
		.sequence = ++peer.sequence(),
		.ack = peer.remote_ack_sequence(),
		.ack_bits = peer.remote_ack_bitfield()
	};

	network::write_bitstream stream(buffer);
	stream.write(header);
	network::write(stream, msg);

	const std::size_t size = stream.bytes_written();

	if (reliable) {
		peer.queue_reliable(header.sequence, std::span(buffer.data(), size));
	}

	outgoing_packet pkt;
	pkt.to = to;
	pkt.size = size;
	std::memcpy(pkt.buffer.data(), buffer.data(), size);

	m_outgoing.push(pkt);
}

template <typename... Components>
auto gse::server<Components...>::resend_reliable_messages() -> void {
	for (auto& [addr, peer] : m_peers) {
		auto to_resend = peer.messages_to_resend(reliable_retry_interval_ms);

		for (auto* msg : to_resend) {
			std::array<std::byte, max_packet_size> buffer;

			const packet_header new_header{
				.sequence = ++peer.sequence(),
				.ack = peer.remote_ack_sequence(),
				.ack_bits = peer.remote_ack_bitfield()
			};

			network::write_bitstream stream(buffer);
			stream.write(new_header);

			constexpr std::size_t header_size = sizeof(packet_header);
			if (msg->data.size() > header_size) {
				stream.write_bytes(msg->data.data() + header_size, msg->data.size() - header_size);
			}

			outgoing_packet pkt;
			pkt.to = addr;
			pkt.size = stream.bytes_written();
			std::memcpy(pkt.buffer.data(), buffer.data(), pkt.size);

			m_outgoing.push(pkt);

			msg->sent_time_ms = network::current_time_ms();
			++msg->send_count;
		}
	}
}

template <typename... Components>
auto gse::server<Components...>::update(world& w, channel_writer& channels, const actions::system::data& actions_s) -> void {
	if (!w.current_scene()) {
		channels.push<activate_scene_request>({
			.scene_id = find("Default Scene"),
		});
	}

	constexpr std::size_t max_packets_per_update = 256;
	std::size_t processed_packets = 0;

	incoming_packet pkt;

	while (processed_packets < max_packets_per_update && m_incoming.pop(pkt)) {
		++processed_packets;

		const std::span data(pkt.buffer.data(), pkt.size);
		network::read_bitstream stream(data);

		const auto it = m_peers.find(pkt.from);
		const auto header = stream.read<packet_header>();
		const auto mid = stream.read<std::uint64_t>();

		if (it == m_peers.end()) {
			if (network::try_decode<network::server_info_request>(stream, mid, [&](const auto&) {
				std::array<std::byte, max_packet_size> buffer;
				network::write_bitstream out_stream(buffer);

				const packet_header header_out{};
				out_stream.write(header_out);
				network::write(out_stream, network::server_info_response{
					.players = static_cast<std::uint8_t>(m_clients.size()),
					.max_players = 8
				});

				outgoing_packet out_pkt;
				out_pkt.to = pkt.from;
				out_pkt.size = out_stream.bytes_written();
				std::memcpy(out_pkt.buffer.data(), buffer.data(), out_pkt.size);
				m_outgoing.push(out_pkt);
			})) {
				continue;
			}

			network::try_decode<network::connection_request>(stream, mid, [&](const auto&) {
				constexpr std::uint8_t max_players = 8;
				if (m_clients.size() >= max_players) {
					std::println("Client [{}:{}] failed to connect (server full: {}/{})",
						pkt.from.ip, pkt.from.port, m_clients.size(), max_players);
					return;
				}

				m_peers.emplace(pkt.from, network::remote_peer(pkt.from));
				accept_connection(w, pkt.from);
				std::println("Client [{}:{}] connected ({}/{})",
					pkt.from.ip, pkt.from.port, m_clients.size(), max_players);
			});

			continue;
		}

		if (network::try_decode<network::connection_request>(stream, mid, [&](const auto&) {
			if (auto client_it = m_clients.find(pkt.from); client_it != m_clients.end()) {
				std::println("Client [{}:{}] reconnecting", pkt.from.ip, pkt.from.port);
				if (w.current_scene()) {
					if (auto* pc = w.registry().try_component<player_controller>(client_it->second.controller_id)) {
						if (pc->controlled_entity_id.exists()) {
							w.registry().remove(pc->controlled_entity_id);
						}
					}
					w.registry().remove(client_it->second.controller_id);
				}
				m_clients.erase(client_it);
			}

			accept_connection(w, pkt.from);
		})) {
			continue;
		}

		auto& peer = it->second;

		peer.process_acks(header.ack, header.ack_bits);
		peer.ingest_packet_sequence(header.sequence);

		network::try_decode<network::ping>(stream, mid, [&](const auto& m) {
			send(
				network::pong{
					.sequence = m.sequence,
				},
				pkt.from
			);
		}) ||
		network::try_decode<network::server_info_request>(stream, mid, [&](const auto&) {
			send(
				network::server_info_response{
					.players = static_cast<std::uint8_t>(m_clients.size()),
					.max_players = 8,
				},
				pkt.from
			);
		}) ||
		network::try_decode<network::input_frame>(stream, mid, [&](const auto& m) {
			auto& cd = m_clients[pkt.from];
			if (m.input_sequence <= cd.last_input_sequence) {
				return;
			}

			cd.last_input_sequence = m.input_sequence;
			cd.camera_yaw = gse::radians(m.camera_yaw);
			network::apply_input_frame(cd.latest_input, m);
		});
	}

	std::optional<id> scene_requested_id;

	if (w.current_scene()) {
		for (const auto& [scene_id, condition] : w.triggers()) {
			for (const auto& cd : m_clients | std::views::values) {
				auto* pc = w.registry().try_component<player_controller>(cd.controller_id);
				const auto controlled_id = pc ? pc->controlled_entity_id : id{};

				evaluation_context ctx{
					.client_id = controlled_id,
					.input = &cd.latest_input,
					.actions_sys = &actions_s,
					.registry = &w.registry(),
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

	resend_reliable_messages();

	if (w.current_scene()) {
		auto send_all = [this](const auto& msg, const network::address& to) {
			this->send(msg, to);
		};

		if (!m_pending_snapshots.empty()) {
			for (const auto& addr : m_pending_snapshots) {
				network::replicate_snapshot_to<type_pack<Components...>>(send_all, w.registry(), addr);
			}
			m_pending_snapshots.clear();
		}

		network::replicate_deltas<type_pack<Components...>>(send_all, w.registry(), m_peers);
	}
}

template <typename... Components>
auto gse::server<Components...>::peers() const -> const std::unordered_map<network::address, network::remote_peer>& {
	return m_peers;
}

template <typename... Components>
auto gse::server<Components...>::clients() const -> const std::unordered_map<network::address, client_data>& {
	return m_clients;
}

template <typename... Components>
auto gse::server<Components...>::host_entity() const -> std::optional<id> {
	return m_host_entity;
}

template <typename... Components>
auto gse::server<Components...>::host_address() const -> std::optional<network::address> {
	return m_host_addr;
}

template <typename... Components>
auto gse::server<Components...>::accept_connection(world& w, const network::address& addr) -> void {
	id controller_id{};
	if (w.current_scene()) {
		const auto controller_name = std::format("PlayerController_{}:{}", addr.ip, addr.port);
		controller_id = w.registry().create(controller_name);
		w.registry().add_component<player_controller>(controller_id);
		w.registry().activate(controller_id);
		m_clients.emplace(addr, client_data{
			.controller_id = controller_id,
		});

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

	if (const auto* active = w.current_scene()) {
		send_reliable(
			network::notify_scene_change{
				.scene_id = active->id(),
			},
			addr
		);
	}

	m_pending_snapshots.insert(addr);
}
