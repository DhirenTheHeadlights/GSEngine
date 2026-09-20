export module gse.network:client;

import gse.assert;
import gse.assets;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.gpu;
import gse.log;
import gse.math;
import gse.os;
import gse.time;
import std;

import :bitstream;
import :connection;
import :endpoint;
import :message;
import :socket;

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

		auto disconnect() -> void;

		auto tick() -> void;

		auto current_state() const -> state;

		auto server_address() const -> const address&;

		auto dropped() const -> std::uint64_t;

		template <is_network_message T>
		auto send(
			const T& msg,
			bool reliable = false
		) -> void;

		auto poll(
			const std::function<void(inbound_message&)>& on_message
		) -> void;

	private:
		endpoint m_endpoint;
		address m_server;
		state m_state = state::disconnected;

		time m_timeout{ seconds(5.f) };
		time m_retry{ seconds(1.f) };

		clock m_connection_start_clock;
		clock m_retry_clock;
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

gse::network::client::~client() {
	disconnect();
}

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

auto gse::network::client::disconnect() -> void {
	if (m_state == state::disconnected) {
		return;
	}

	log::println(log::category::network, "Client disconnecting from {}:{}", m_server.ip, m_server.port);
	send(disconnect_notice{});
	m_state = state::disconnected;
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

	m_endpoint.resend_reliable();
}

auto gse::network::client::current_state() const -> state {
	return m_state;
}

auto gse::network::client::server_address() const -> const address& {
	return m_server;
}

auto gse::network::client::dropped() const -> std::uint64_t {
	return m_endpoint.dropped();
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

template <gse::network::is_network_message T>
auto gse::network::client::send(const T& msg, const bool reliable) -> void {
	m_endpoint.send(msg, m_server, reliable);
}