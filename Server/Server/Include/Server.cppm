export module gse.server:server;

import std;

import gse.assert;
import gse.network;
import gse.utility;

template <>
struct std::hash<gse::network::address> {
	auto operator()(const gse::network::address& addr) const noexcept -> std::size_t {
		const std::size_t h1 = std::hash<std::string>{}(addr.ip);
		const std::size_t h2 = std::hash<std::uint16_t>{}(addr.port);
		return h1 ^ (h2 << 1);
	}
};

export namespace gse {
	class server {
	public:
		explicit server(
			std::uint16_t port
		);

		auto update(
			const std::function<void(const network::address&, network::message&)>& on_receive
		) -> void;

		template <typename T>
		auto send(
			const T& msg,
			const network::address& to
		) -> void;
	private:
		static auto process_header(
			network::bitstream& stream,
			network::remote_peer& peer
		) -> packet_header;

		network::udp_socket m_socket;
		std::unordered_map<network::address, network::remote_peer> m_peers;
	};
}

gse::server::server(const std::uint16_t port) {
	m_socket.bind(network::address{
		.ip = "0.0.0.0",
		.port = port
	});

	std::println("Server started on port {}", port);
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
	stream.write(gse::network::message_type_v<T>);
	stream.write(msg);

	const network::packet packet_to_send{
		.data = reinterpret_cast<std::uint8_t*>(buffer.data()),
		.size = stream.bytes_written()
	};

	assert(
		m_socket.send_data(packet_to_send, to) != network::socket_state::error, 
		"Failed to send packet from server."
	);
}

auto gse::server::update(const std::function<void(const network::address&, network::message&)>& on_receive) -> void {
	std::array<std::byte, max_packet_size> buffer;

	while (auto received = m_socket.receive_data(buffer)) {
		if (received->bytes_read < sizeof(packet_header) + sizeof(network::message_type)) {
			continue; 
		}

		const std::span received_data(buffer.data(), received->bytes_read);
		network::bitstream stream(received_data);

		auto it = m_peers.find(received->from);

		if (it == m_peers.end()) {
			stream.read<packet_header>();

			if (const auto type = stream.read<network::message_type>(); type == network::message_type::connection_request) {
				std::println("Server: Received ConnectionRequest from {}:{}", received->from.ip, received->from.port);
				m_peers.emplace(received->from, network::remote_peer(received->from));
				send(network::connection_accepted_message{}, received->from);
			}
			continue;
		}

		auto& peer = it->second;
		process_header(stream, peer); 

		const auto type = stream.read<network::message_type>();
		network::message msg;
		bool message_read = true;

		switch (type) {
		case network::message_type::ping:
			msg = stream.read<network::ping_message>();
			break;
		case network::message_type::pong:
			msg = stream.read<network::pong_message>();
			break;
		case network::message_type::connection_request:
		case network::message_type::connection_accepted:
		default:
			message_read = false;
			break;
		}

		if (message_read) {
			on_receive(received->from, msg);
		}
	}
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
