export module gse.network:client;

import std;

import gse.assert;
import gse.utility;
import gse.physics.math;

import :socket;
import :remote_peer;
import :message;
import :packet_header;
import :bitstream;

export namespace gse::network {
	class client {
	public:
		enum struct state : std::uint8_t {
			disconnected,
			connecting,
			connected
		};

		client(
			const address& listen,
			const address& server
		);

		auto connect(
		) -> void;

		auto current_state(
		) const -> state;

		template <typename T>
		auto send(
			const T& msg
		) -> void;

		auto update(
			const std::function<void(message&)>& on_receive
		) -> void;
	private:
		udp_socket m_socket;
		remote_peer m_server;
		state m_state = state::disconnected;
		time m_last_request_time;
	};
}

gse::network::client::client(const address& listen, const address& server) : m_server(server) {
	m_socket.bind(listen);
}

auto gse::network::client::connect() -> void {
	if (m_state == state::disconnected) {
		send(connection_request_message{});
		m_state = state::connecting;
		m_last_request_time = system_clock::now();
	}
}

auto gse::network::client::current_state() const -> state {
	return m_state;
}

template <typename T>
auto gse::network::client::send(const T& msg) -> void {
	std::array<std::byte, max_packet_size> buffer;

	const packet_header header{
		.sequence = ++m_server.sequence(),
		.ack = m_server.remote_ack_sequence(),
		.ack_bits = m_server.remote_ack_bitfield()
	};

	bitstream stream(buffer);
	stream.write(header);
	stream.write(gse::network::message_type_v<T>);
	stream.write(msg);

	const packet pkt{
		.data = reinterpret_cast<std::uint8_t*>(buffer.data()),
		.size = stream.bytes_written()
	};

	assert(
		m_socket.send_data(pkt, m_server.addr()) != socket_state::error,
		std::source_location::current(),
		"Failed to send packet from client."
	);
}

auto gse::network::client::update(const std::function<void(message&)>& on_receive) -> void {
	if (m_state == state::connecting) {
		if (system_clock::now() - m_last_request_time > seconds(1.f)) {
			send(connection_request_message{});
			m_last_request_time = system_clock::now();
		}
	}

	std::array<std::byte, max_packet_size> buffer;
	while (const auto received = m_socket.receive_data(buffer)) {
		if (received->from.ip != m_server.addr().ip || received->from.port != m_server.addr().port) {
			continue; 
		}

		const std::span received_data(buffer.data(), received->bytes_read);
		bitstream stream(received_data);

		const auto header = stream.read<packet_header>();
		const auto type = stream.read<message_type>();

		if (header.sequence > m_server.remote_ack_sequence()) {
			if (const std::uint32_t diff = header.sequence - m_server.remote_ack_sequence(); diff < 32) {
				m_server.remote_ack_bitfield() <<= diff;
				m_server.remote_ack_bitfield() |= (1 << (diff - 1));
			}
			else {
				m_server.remote_ack_bitfield() = 0;
			}
			m_server.remote_ack_sequence() = header.sequence;
		}

		else if (header.sequence < m_server.remote_ack_sequence()) {
			if (const std::uint32_t diff = m_server.remote_ack_sequence() - header.sequence; diff < 32) {
				m_server.remote_ack_bitfield() |= (1 << (diff - 1));
			}
		}

		if (m_state == state::connecting && type == message_type::connection_accepted) {
			m_state = state::connected;
		}

		message msg;
		switch (type) {
		case message_type::ping:
			msg = stream.read<ping_message>();
			break;
		case message_type::pong:
			msg = stream.read<pong_message>();
			break;
		default: ;
		}

		on_receive(msg);
	}
}

