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

		~client();

		auto connect(
			time_t<std::uint32_t> timeout = seconds(static_cast<std::uint32_t>(5)),
			time_t<std::uint32_t> retry = seconds(static_cast<std::uint32_t>(1))
		) -> bool;

		auto current_state(
		) const -> state;

		template <typename T>
		auto send(
			const T& msg
		) -> void;

		auto drain(
			const std::function<void(message&)>& on_receive
		) -> void;
	private:
		udp_socket m_socket;
		remote_peer m_server;
		state m_state = state::disconnected;

		time_t<std::uint32_t> m_timeout;
		time_t<std::uint32_t> m_retry;

		clock m_connection_clock;

		auto tick(
		) -> void;

		std::mutex m_inbox_mutex;
		std::vector<message> m_inbox;

		std::atomic<bool> m_running{ false };
		task::group m_net_group;
	};
}

gse::network::client::client(const address& listen, const address& server) : m_server(server), m_net_group(generate_id("Network::Client")) {
	m_socket.bind(listen);

	m_running.store(true, std::memory_order_release);

	//m_net_group.post(
	//	[this] {
	//		//while (m_running.load(std::memory_order_acquire)) {
	//		//  this->tick();
	//		//  // TODO: add more events to wait on
	//		//}
	//	},
	//	generate_id("network.client.worker")
	//);
}

gse::network::client::~client() {
	m_running.store(false, std::memory_order_release);
}

auto gse::network::client::connect(const time_t<std::uint32_t> timeout, const time_t<std::uint32_t> retry) -> bool {
	if (m_state != state::disconnected) {
		return false;
	}

	send(connection_request_message{});

	m_state = state::connecting;
	m_timeout = timeout;
	m_retry = retry;

	m_connection_clock.reset();

	return true;
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

auto gse::network::client::drain(const std::function<void(message&)>& on_receive) -> void {
	std::vector<message> batch;
	{
		std::lock_guard lk(m_inbox_mutex);
		if (m_inbox.empty()) return;
		batch.swap(m_inbox);
	}
	for (auto& m : batch) {
		on_receive(m);
	}
}

auto gse::network::client::tick() -> void {
	if (m_state == state::connecting) {
		if (m_connection_clock.elapsed<std::uint32_t>() > m_retry) {
			send(connection_request_message{});
			m_connection_clock.reset();
		}
		if (m_connection_clock.elapsed<std::uint32_t>() > m_timeout) {
			m_state = state::disconnected;
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
		bool ok = true;
		switch (type) {
			case message_type::ping:                msg = stream.read<ping_message>();                break;
			case message_type::pong:                msg = stream.read<pong_message>();                break;
			case message_type::notify_scene_change: msg = stream.read<notify_scene_change_message>(); break;
			default: ok = false; break;
		}

		if (!ok) {
			continue;
		}

		{
			std::lock_guard lk(m_inbox_mutex);
			m_inbox.push_back(std::move(msg));
		}
	}
}

