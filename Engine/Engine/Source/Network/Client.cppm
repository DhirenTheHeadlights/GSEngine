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
import :endpoint;
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
	class client : public non_copyable {
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

		client(
			client&&
		) = delete;

		auto operator=(
			client&&
		) -> client& = delete;

		auto connect(
			time timeout = seconds(5.f),
			time retry = seconds(1.f)
		) -> bool;

		auto tick() -> void;

		auto current_state() const -> state;

		template <is_network_message T>
		auto send(
			const T& msg,
			bool reliable = false
		) -> void;

		auto poll(
			const std::function<void(inbound_message&)>& on_message
		) -> void;

		auto push_input(
			const actions::state& s,
			std::span<const std::uint16_t> axis1_ids,
			std::span<const std::uint16_t> axis2_ids,
			angle camera_yaw = {}
		) -> void;

	private:
		struct input_snapshot {
			actions::state state;
			std::vector<std::uint16_t> axis1_ids;
			std::vector<std::uint16_t> axis2_ids;
			angle camera_yaw;
		};

		endpoint m_endpoint;
		address m_server;
		state m_state = state::disconnected;

		time m_timeout{ seconds(5.f) };
		time m_retry{ seconds(1.f) };

		clock m_connection_start_clock;
		clock m_retry_clock;

		std::uint32_t m_input_sequence = 0;
		clock m_input_clock;

		input_snapshot m_pending;
		bool m_has_pending = false;
	};
}

gse::network::client::client(const address& listen, const address& server) : m_server(server) {
	if (!m_endpoint.bind(listen)) {
		return;
	}

	if (const auto local = m_endpoint.local_address()) {
		log::println(log::category::network, "Client bound to local port {}", local->port);
	}

	m_endpoint.ensure_peer(server);
}

gse::network::client::~client() = default;

auto gse::network::client::connect(const time timeout, const time retry) -> bool {
	if (m_state != state::disconnected) {
		return false;
	}

	if (!m_endpoint.valid()) {
		log::println(log::level::error, log::category::network, "Client cannot connect because the socket is not valid");
		return false;
	}

	log::println(log::category::network, "Client connecting to {}:{}...", m_server.ip, m_server.port);

	m_timeout = timeout;
	m_retry = retry;
	m_state = state::connecting;

	m_connection_start_clock.reset();
	m_retry_clock.reset();

	send(connection_request{});

	return true;
}

auto gse::network::client::tick() -> void {
	if (m_state == state::connecting) {
		if (m_connection_start_clock.elapsed() > m_timeout) {
			log::println(log::level::warning, log::category::network, "Client connection timed out");
			m_state = state::disconnected;
		}
		else if (m_retry_clock.elapsed() > m_retry) {
			send(connection_request{});
			m_retry_clock.reset();
		}
	}

	const time input_send_interval = milliseconds(16.f);

	if (m_state == state::connected && m_has_pending && m_input_clock.elapsed() > input_send_interval) {
		send(
			extract_input_frame(
				m_pending.state,
				m_pending.axis1_ids,
				m_pending.axis2_ids,
				++m_input_sequence,
				m_pending.camera_yaw
			)
		);
		m_input_clock.reset();
	}

	m_endpoint.resend_reliable();
}

auto gse::network::client::current_state() const -> state {
	return m_state;
}

auto gse::network::client::poll(const std::function<void(inbound_message&)>& on_message) -> void {
	m_endpoint.poll([this, &on_message](inbound_message& msg) {
		if (msg.from != m_server) {
			return;
		}

		if (m_state != state::connected && msg.id == message_id_v<connection_accepted>) {
			log::println(log::category::network, "Client connected to {}:{}", m_server.ip, m_server.port);
			m_state = state::connected;
		}

		on_message(msg);
	});
}

auto gse::network::client::push_input(const actions::state& s, std::span<const std::uint16_t> axis1_ids, std::span<const std::uint16_t> axis2_ids, const angle camera_yaw) -> void {
	m_pending.state = s;
	m_pending.axis1_ids.assign(axis1_ids.begin(), axis1_ids.end());
	m_pending.axis2_ids.assign(axis2_ids.begin(), axis2_ids.end());
	m_pending.camera_yaw = camera_yaw;
	m_has_pending = true;
}

template <gse::network::is_network_message T>
auto gse::network::client::send(const T& msg, const bool reliable) -> void {
	m_endpoint.send(msg, m_server, reliable);
}
