export module gse.server:server;

import std;

import gse.assert;
import gse.network;
import gse.physics;
import gse.graphics;
import gse.utility;
import gse.platform;
import gse.runtime;

template <>
struct std::hash<gse::network::address> {
	auto operator()(const gse::network::address& addr) const noexcept -> std::size_t {
		const std::size_t h1 = std::hash<std::string>{}(addr.ip);
		const std::size_t h2 = std::hash<std::uint16_t>{}(addr.port);
		return h1 ^ (h2 << 1);
	}
};

export namespace gse {
	struct client_data {
		id controller_id;
		actions::state latest_input;
		std::uint32_t last_input_sequence = 0;
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

	class server : public hook<world> {
	public:
		server(
			world* owner,
			std::uint16_t port
		);

		auto initialize(
		) -> void override;

		auto update(
		) -> void override;

		template <typename T>
		auto send(
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
		static auto process_header(
			network::bitstream& stream,
			network::remote_peer& peer
		) -> packet_header;

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
		std::uint32_t m_next_player_id = 0;
	};
}

gse::server::server(world* owner, const std::uint16_t port)
	: hook(owner), m_port(port) {}

auto gse::server::initialize() -> void {
	m_socket.bind(network::address{
		.ip = "0.0.0.0",
		.port = m_port
	});

	m_thread = std::jthread([this](const std::stop_token& st) {
		std::array<std::byte, max_packet_size> buffer;
		constexpr time_t<std::uint32_t> max_sleep = milliseconds(8);

		while (!st.stop_requested()) {
			(void)m_socket.wait_readable(max_sleep);

			if (const auto received = m_socket.receive_data(buffer)) {
				incoming_packet pkt;
				pkt.from = received->from;
				pkt.size = received->bytes_read;
				std::memcpy(pkt.buffer.data(), buffer.data(), received->bytes_read);
				m_incoming.push(pkt);
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

template <typename T>
auto gse::server::send(const T& msg, const network::address& to) -> void {
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

	network::bitstream stream(buffer);
	stream.write(header);
	network::write(stream, msg);

	outgoing_packet pkt;
	pkt.to = to;
	pkt.size = stream.bytes_written();
	std::memcpy(pkt.buffer.data(), buffer.data(), pkt.size);

	m_outgoing.push(pkt);
}

auto gse::server::update() -> void {
	if (!m_owner->current_scene()) {
		m_owner->activate(find("Default Scene"));
	}

	constexpr std::size_t max_packets_per_update = 256;
	std::size_t processed_packets = 0;

	incoming_packet pkt;

	while (processed_packets < max_packets_per_update && m_incoming.pop(pkt)) {
		++processed_packets;

		const std::span data(pkt.buffer.data(), pkt.size);
		network::bitstream stream(data);

		const auto it = m_peers.find(pkt.from);
		const auto header = stream.read<packet_header>();

		if (it == m_peers.end()) {
			const auto mid = network::message_id(stream);

			network::try_decode<network::connection_request>(stream, mid, [&](const auto&) {
				m_peers.emplace(pkt.from, network::remote_peer(pkt.from));

				if (auto* scene = m_owner->current_scene()) {
					const auto controller_name = std::format("PlayerController_{}", m_next_player_id++);
					auto controller_id = scene->registry().create(controller_name);
					scene->registry().add_component<player_controller>(controller_id, player_controller_data{});
					scene->registry().activate(controller_id);
					m_clients.emplace(pkt.from, client_data{ .controller_id = controller_id });

					if (!m_host_entity.has_value()) {
						m_host_entity = controller_id;
						m_host_addr = pkt.from;
					}
				}

				send(network::connection_accepted{}, pkt.from);

				if (const auto* active = m_owner->current_scene()) {
					const network::notify_scene_change msg{
						.scene_id = active->id()
					};
					send(msg, pkt.from);
				}

				m_pending_snapshots.insert(pkt.from);
			});

			continue;
		}

		if (auto& peer = it->second; header.sequence > peer.remote_ack_sequence()) {
			if (const std::uint32_t diff = header.sequence - peer.remote_ack_sequence(); diff < 32) {
				peer.remote_ack_bitfield() <<= diff;
				peer.remote_ack_bitfield() |= (1 << (diff - 1));
			}
			else {
				peer.remote_ack_bitfield() = 1;
			}
			peer.remote_ack_sequence() = header.sequence;
		}
		else if (header.sequence < peer.remote_ack_sequence()) {
			if (const std::uint32_t diff = peer.remote_ack_sequence() - header.sequence; diff < 32) {
				peer.remote_ack_bitfield() |= (1 << (diff - 1));
			}
		}

		const auto id = network::message_id(stream);

		network::match_message(stream, id)
			.if_is([&](const network::ping& m) {
				send(network::pong{ .sequence = m.sequence }, pkt.from);
			})
			.else_if_is([&](const network::pong&) {
			})
			.else_if_is([&](const network::input_frame_header& fh) {
				const std::size_t wc = fh.action_word_count;

				std::vector<std::uint64_t> pressed(wc), released(wc);

				for (std::size_t i = 0; i < wc; ++i) {
					pressed[i] = stream.read<std::uint64_t>();
				}

				for (std::size_t i = 0; i < wc; ++i) {
					released[i] = stream.read<std::uint64_t>();
				}

				std::vector<network::axes1_pair> a1(fh.axes1_count);
				for (auto& p : a1) {
					p = stream.read<network::axes1_pair>();
				}

				std::vector<network::axes2_pair> a2(fh.axes2_count);
				for (auto& p : a2) {
					p = stream.read<network::axes2_pair>();
				}

				auto& cd = m_clients[pkt.from];
				if (fh.input_sequence <= cd.last_input_sequence) {
					return;
				}

				cd.last_input_sequence = fh.input_sequence;

				cd.latest_input.begin_frame();
				cd.latest_input.ensure_capacity(fh.action_word_count * 64);
				cd.latest_input.load_transients(pressed, released);

				for (auto& [idv, value] : a1) {
					cd.latest_input.set_axis1(idv, value);
				}

				for (auto& [idv, x, y] : a2) {
					cd.latest_input.set_axis2(idv, actions::axis{ x, y });
				}
			});
	}

	std::optional<id> scene_requested_id;

	if (auto* sc = m_owner->current_scene()) {
		for (const auto& [scene_id, condition] : m_owner->triggers()) {
			for (const auto& cd : m_clients | std::views::values) {
				auto* pc = sc->registry().try_linked_object_read<player_controller>(cd.controller_id);
				const auto controlled_id = pc ? pc->controlled_entity_id : id{};

				evaluation_context ctx{
					.client_id = controlled_id,
					.input = &cd.latest_input,
					.registry = &sc->registry()
				};
				if (condition(ctx)) {
					scene_requested_id = scene_id;
				}
			}
		}
	}

	if (scene_requested_id) {
		m_owner->activate(*scene_requested_id);

		if (const auto* active = m_owner->current_scene()) {
			const network::notify_scene_change msg{
				.scene_id = active->id()
			};

			for (const auto& addr : m_clients | std::views::keys) {
				send(msg, addr);
			}
		}
	}

	if (auto* sc = m_owner->current_scene()) {
		auto send_all = [this](const auto& msg, const network::address& to) {
			this->send(msg, to);
		};

		if (!m_pending_snapshots.empty()) {
			for (const auto& addr : m_pending_snapshots) {
				network::replicate_snapshot_to(send_all, sc->registry(), addr);
			}
			m_pending_snapshots.clear();
		}

		network::replicate_deltas(send_all, sc->registry(), m_peers);
	}
}

auto gse::server::peers() const -> const std::unordered_map<network::address, network::remote_peer>& {
	return m_peers;
}

auto gse::server::clients() const -> const std::unordered_map<network::address, client_data>& {
	return m_clients;
}

auto gse::server::host_entity() const -> std::optional<id> {
	return m_host_entity;
}

auto gse::server::host_address() const -> std::optional<network::address> {
	return m_host_addr;
}

auto gse::server::process_header(network::bitstream& stream, network::remote_peer& peer) -> packet_header {
	const auto header = stream.read<packet_header>();

	if (header.sequence > peer.remote_ack_sequence()) {
		if (const std::uint32_t diff = header.sequence - peer.remote_ack_sequence(); diff < 32) {
			peer.remote_ack_bitfield() <<= diff;
			peer.remote_ack_bitfield() |= (1 << (diff - 1));
		}
		else {
			peer.remote_ack_bitfield() = 1;
		}
		peer.remote_ack_sequence() = header.sequence;
	}
	else if (header.sequence < peer.remote_ack_sequence()) {
		if (const std::uint32_t diff = peer.remote_ack_sequence() - header.sequence; diff < 32) {
			peer.remote_ack_bitfield() |= (1 << (diff - 1));
		}
	}

	return header;
}
