export module gse.network:client;

import std;

import gse.assert;
import gse.utility;
import gse.physics.math;
import gse.platform;

import :socket;
import :remote_peer;
import :message;
import :packet_header;
import :bitstream;
import :connection;
import :ping_pong;
import :notify_scene_change;
import :input_frame;

export namespace gse::network {
	using inbox_message = std::variant<
		connection_accepted,
		ping,
		pong,
		notify_scene_change
	>;

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

		auto tick(
		) -> void;

		auto current_state(
		) const -> state;

		template <typename T>
		auto send(
			const T& msg
		) -> void;

		auto drain(
			const std::function<void(inbox_message&)>& on_receive
		) -> void;
	private:
		auto send_input_frame(
		) -> void;

		udp_socket m_socket;
		remote_peer m_server;
		state m_state = state::disconnected;

		time_t<std::uint32_t> m_timeout;
		time_t<std::uint32_t> m_retry;

		clock m_connection_clock;

		std::mutex m_inbox_mutex;
		std::vector<inbox_message> m_inbox;

		std::jthread m_thread;
		std::atomic<bool> m_running{ false };

		std::uint32_t m_input_sequence = 0;
		clock m_input_clock;
	};
}

gse::network::client::client(const address& listen, const address& server) : m_server(server) {
	m_socket.bind(listen);

	m_running.store(true, std::memory_order_release);

	m_thread = std::jthread([this](const std::stop_token& st) {
		constexpr time_t<std::uint32_t> max_sleep = milliseconds(8);

		while (m_running.load(std::memory_order_acquire) && !st.stop_requested()) {

			auto wait = max_sleep;
			if (m_state == state::connecting) {
				const auto elapsed = m_connection_clock.elapsed<std::uint32_t>();
				const auto ms_to_retry = (elapsed >= m_retry) ? 0u : (m_retry - elapsed);
				const auto ms_to_timeout = (elapsed >= m_timeout) ? 0u : (m_timeout - elapsed);
				const auto ms = std::min(ms_to_retry, ms_to_timeout);
				wait = std::min(wait, ms == 0 ? 1 : ms);
			}

			(void)m_socket.wait_readable(wait);
			this->tick();
		}
	});
}

gse::network::client::~client() {
	m_running.store(false, std::memory_order_release);
}

auto gse::network::client::connect(const time_t<std::uint32_t> timeout, const time_t<std::uint32_t> retry) -> bool {
	if (m_state != state::disconnected) {
		return false;
	}

	send(connection_request{});

	m_state = state::connecting;
	m_timeout = timeout;
	m_retry = retry;

	m_connection_clock.reset();

	return true;
}

auto gse::network::client::tick() -> void {
	if (m_state == state::connecting) {
		if (m_connection_clock.elapsed<std::uint32_t>() > m_retry) {
			send(connection_request{});
			m_connection_clock.reset();
		}
		if (m_connection_clock.elapsed<std::uint32_t>() > m_timeout) {
			m_state = state::disconnected;
		}
	}

	std::array<std::byte, max_packet_size> buffer;
	while (const auto received = m_socket.receive_data(buffer)) {
		if (received->from.ip != m_server.addr().ip ||
			received->from.port != m_server.addr().port) {
			continue;
		}

		const std::span received_data(buffer.data(), received->bytes_read);
		bitstream stream(received_data);

		if (const auto header = stream.read<packet_header>(); header.sequence > m_server.remote_ack_sequence()) {
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

		const auto id = message_id(stream);

		match_message(stream, id)
			.if_is<connection_accepted>([&](const connection_accepted&) {
				m_state = state::connected;
				std::lock_guard lk(m_inbox_mutex);
				m_inbox.emplace_back(connection_accepted{});
			})
			.else_if_is<ping>([&](const ping& m) {
				send(pong{ .sequence = m.sequence });
				std::lock_guard lk(m_inbox_mutex);
				m_inbox.emplace_back(m);
			})
			.else_if_is<pong>([&](const pong& m) {
				std::lock_guard lk(m_inbox_mutex);
				m_inbox.emplace_back(m);
			})
			.else_if_is<notify_scene_change>([&](const notify_scene_change& m) {
				std::lock_guard lk(m_inbox_mutex);
				m_inbox.emplace_back(m);
			});
	}

	if (m_state == state::connected) {
		if (m_input_clock.elapsed<std::uint32_t>() > 16u) {
			send_input_frame();
			m_input_clock.reset();
		}
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
	write(stream, msg);
	
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

auto gse::network::client::drain(const std::function<void(inbox_message&)>& on_receive) -> void {
	std::vector<inbox_message> batch;
	{
		std::lock_guard lk(m_inbox_mutex);
		if (m_inbox.empty()) return;
		batch.swap(m_inbox);
	}
	for (auto& m : batch) {
		on_receive(m);
	}
}

auto gse::network::client::send_input_frame() -> void {
	const auto& st = actions::current_state();

	const auto pw = st.pressed_mask().words();
	const auto rw = st.released_mask().words();
	const std::uint16_t wc = static_cast<std::uint16_t>(std::max(pw.size(), rw.size()));

	const input_frame_header fh{
		.input_sequence = ++m_input_sequence,
		.action_word_count = wc,
		.axes1_count = 0u,
		.axes2_count = 0u
	};

	std::array<std::byte, max_packet_size> buffer;

	const packet_header header{
		.sequence = ++m_server.sequence(),
		.ack = m_server.remote_ack_sequence(),
		.ack_bits = m_server.remote_ack_bitfield()
	};

	bitstream stream(buffer);
	stream.write(header);
	write(stream, fh);

	for (std::size_t i = 0; i < wc; ++i) {
		const std::uint64_t w = (i < pw.size()) ? pw[i] : 0ull;
		stream.write(w);
	}
	for (std::size_t i = 0; i < wc; ++i) {
		const std::uint64_t w = (i < rw.size()) ? rw[i] : 0ull;
		stream.write(w);
	}

	const packet pkt{
		.data = reinterpret_cast<std::uint8_t*>(buffer.data()),
		.size = stream.bytes_written()
	};

	assert(
		m_socket.send_data(pkt, m_server.addr()) != socket_state::error,
		std::source_location::current(),
		"Failed to send input frame from client."
	);
}
