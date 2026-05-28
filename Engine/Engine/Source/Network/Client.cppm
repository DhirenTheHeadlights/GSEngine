export module gse.network:client;

import std;

import gse.assert;
import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.os;
import gse.assets;
import gse.gpu;

import :actions;
import :socket;
import :remote_peer;
import :message;
import :packet_header;
import :bitstream;
import :connection;
import :ping_pong;
import :notify_scene_change;
import :input_frame;
import :server_info;

export namespace gse::network {
	struct raw_message {
		std::uint64_t id;
		std::vector<std::byte> payload;
	};

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
			time_t<std::uint32_t> timeout = seconds(5),
			time_t<std::uint32_t> retry = seconds(1)
		) -> bool;

		auto tick() -> void;

		auto current_state() const -> state;

		auto send(
			const auto& msg
		) -> void;

		auto drain(
			const std::function<void(raw_message&)>& on_receive
		) -> void;

		auto push_input(
			const actions::state& s,
			std::span<const std::uint16_t> axis1_ids,
			std::span<const std::uint16_t> axis2_ids,
			angle camera_yaw = {}
		) -> void;

	private:
		udp_socket m_socket;
		remote_peer m_server;
		std::atomic<state> m_state = state::disconnected;

		time_t<std::uint32_t> m_timeout;
		time_t<std::uint32_t> m_retry;

		clock m_connection_start_clock;
		clock m_retry_clock;

		std::mutex m_inbox_mutex;
		std::vector<raw_message> m_inbox;

		std::jthread m_thread;
		std::atomic<bool> m_running{ false };

		std::uint32_t m_input_sequence = 0;
		clock m_input_clock;

		struct input_snapshot {
			actions::state state;
			std::vector<std::uint16_t> axis1_ids;
			std::vector<std::uint16_t> axis2_ids;
			angle camera_yaw;
		};

		std::mutex m_input_mutex;
		std::optional<input_snapshot> m_next_input;
		input_snapshot m_last_input;
		bool m_has_last_input = false;
	};
}

gse::network::client::client(const address& listen, const address& server) : m_server(server) {
	if (!m_socket.bind(listen)) {
		log::println(
			log::level::error,
			log::category::network,
			"Client failed to bind socket to {}:{}",
			listen.ip,
			listen.port
		);
		return;
	}

	if (const auto local = m_socket.local_address()) {
		log::println(log::category::network, "Client bound to local port {}", local->port);
	}

	m_running.store(true, std::memory_order_release);

	m_thread = std::jthread([this](const std::stop_token& st) {
		constexpr time_t<std::uint32_t> max_sleep = milliseconds(8);

		while (m_running.load(std::memory_order_acquire) && !st.stop_requested()) {
			auto wait = max_sleep;
			if (m_state.load(std::memory_order_relaxed) == state::connecting) {
				const auto retry_elapsed = m_retry_clock.elapsed<std::uint32_t>();
				const auto total_elapsed = m_connection_start_clock.elapsed<std::uint32_t>();
				const auto to_retry = (retry_elapsed >= m_retry) ? seconds(0u) : (m_retry - retry_elapsed);
				const auto to_timeout = (total_elapsed >= m_timeout) ? seconds(0u) : (m_timeout - total_elapsed);
				const auto time = std::min(to_retry, to_timeout);
				wait = std::min(wait, time == seconds(0u) ? seconds(1u) : time);
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

	if (!m_socket.valid()) {
		log::println(log::level::error, log::category::network,
					 "Client cannot connect because the socket is not valid");
		return false;
	}

	log::println(log::category::network, "Client connecting to {}:{}...", m_server.addr().ip, m_server.addr().port);
	send(connection_request{});

	m_state = state::connecting;
	m_timeout = timeout;
	m_retry = retry;

	m_connection_start_clock.reset();
	m_retry_clock.reset();

	return true;
}

auto gse::network::client::tick() -> void {
	const auto current = m_state.load(std::memory_order_relaxed);

	if (current == state::connecting) {
		if (m_connection_start_clock.elapsed<std::uint32_t>() > m_timeout) {
			log::println(log::level::warning, log::category::network, "Client connection timed out");
			m_state = state::disconnected;
		}
		else if (m_retry_clock.elapsed<std::uint32_t>() > m_retry) {
			send(connection_request{});
			m_retry_clock.reset();
		}
	}

	std::array<std::byte, max_packet_size> buffer;
	while (const auto received = m_socket.receive_data(buffer)) {
		if (received->from.ip != m_server.addr().ip || received->from.port != m_server.addr().port) {
			continue;
		}

		const std::span received_data(buffer.data(), received->bytes_read);
		read_bitstream stream(received_data);

		const auto header = stream.read<packet_header>();
		m_server.ingest_packet_sequence(header.sequence);

		const auto id = stream.read<std::uint64_t>();

		if (m_state.load(std::memory_order_relaxed) == state::connecting && id == message_id_v<connection_accepted>) {
			log::println(log::category::network, "Client connected to {}:{}", m_server.addr().ip, m_server.addr().port);
			m_state = state::connected;
		}

		const auto remaining = stream.remaining_bytes();
		std::vector<std::byte> payload(remaining);
		if (remaining > 0) {
			stream.read_bytes(payload.data(), remaining);
		}

		std::lock_guard lk(m_inbox_mutex);
		m_inbox.emplace_back(raw_message{
			.id = id,
			.payload = std::move(payload),
		});
	}

	if (current == state::connected && m_input_clock.elapsed<std::uint32_t>() > milliseconds(16u)) {
		std::optional<input_snapshot> next;

		{
			std::lock_guard lk(m_input_mutex);
			if (m_next_input) {
				next.emplace(std::move(*m_next_input));
				m_next_input.reset();
			}
		}

		if (next) {
			m_last_input = std::move(*next);
			m_has_last_input = true;
		}

		if (m_has_last_input) {
			send(
				extract_input_frame(
					m_last_input.state,
					m_last_input.axis1_ids,
					m_last_input.axis2_ids,
					++m_input_sequence,
					m_last_input.camera_yaw
				)
			);
			m_input_clock.reset();

			if (!next) {
				m_last_input.state.begin_frame();
			}
		}
	}
}

auto gse::network::client::current_state() const -> state {
	return m_state.load(std::memory_order_relaxed);
}

auto gse::network::client::send(const auto& msg) -> void {
	std::array<std::byte, max_packet_size> buffer;

	const packet_header header{
		.sequence = ++m_server.sequence(),
		.ack = m_server.remote_ack_sequence(),
		.ack_bits = m_server.remote_ack_bitfield()
	};

	write_bitstream stream(buffer);
	stream.write(header);
	write(stream, msg);

	const packet pkt{
		.data = reinterpret_cast<std::uint8_t*>(buffer.data()),
		.size = stream.bytes_written()
	};

	(void)m_socket.send_data(pkt, m_server.addr());
}

auto gse::network::client::drain(const std::function<void(raw_message&)>& on_receive) -> void {
	std::vector<raw_message> batch;
	{
		std::lock_guard lk(m_inbox_mutex);
		if (m_inbox.empty()) {
			return;
		}
		batch.swap(m_inbox);
	}
	for (auto& m : batch) {
		on_receive(m);
	}
}

auto gse::network::client::push_input(const actions::state& s, std::span<const std::uint16_t> axis1_ids, std::span<const std::uint16_t> axis2_ids, const angle camera_yaw) -> void {
	input_snapshot snap;
	snap.state = s;
	snap.axis1_ids.assign(axis1_ids.begin(), axis1_ids.end());
	snap.axis2_ids.assign(axis2_ids.begin(), axis2_ids.end());
	snap.camera_yaw = camera_yaw;

	std::lock_guard lk(m_input_mutex);
	m_next_input.emplace(std::move(snap));
}
