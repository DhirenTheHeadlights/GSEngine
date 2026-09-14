export module gse.network:endpoint;

import std;

import gse.core;
import gse.log;
import gse.math;
import gse.time;
import gse.concurrency;

import :socket;
import :remote_peer;
import :packet_header;
import :bitstream;
import :message;

export namespace gse::network {
	struct raw_packet {
		address peer;
		std::size_t size = 0;
		std::array<std::byte, max_packet_size> buffer{};
	};

	struct inbound_message {
		address from;
		std::uint64_t id = 0;
		std::vector<std::byte> payload;
	};

	class endpoint : public non_copyable {
	public:
		endpoint();

		~endpoint();

		endpoint(
			endpoint&&
		) = delete;

		auto operator=(
			endpoint&&
		) -> endpoint& = delete;

		auto bind(
			const address& local
		) -> bool;

		auto valid() const -> bool;

		auto local_address() const -> std::optional<address>;

		auto ensure_peer(
			const address& addr
		) -> remote_peer&;

		auto find_peer(
			const address& addr
		) -> remote_peer*;

		auto remove_peer(
			const address& addr
		) -> void;

		auto silent_peers(
			time_t<std::uint64_t, milliseconds> deadline
		) const -> std::vector<address>;

		auto peers() const -> const std::unordered_map<address, remote_peer>&;

		template <is_network_message T>
		auto send(
			const T& msg,
			const address& to,
			bool reliable = false
		) -> void;

		auto poll(
			const std::function<void(inbound_message&)>& on_message
		) -> void;

		auto resend_reliable() -> void;

		static auto set_simulation(
			std::uint32_t latency_ms,
			std::uint32_t loss_permille
		) -> void;

		auto dropped() const -> std::uint64_t;

	private:
		struct delayed_packet {
			time_t<std::uint64_t, milliseconds> due;
			raw_packet packet;
		};

		auto start_thread() -> void;

		auto take_incoming(
			raw_packet& out
		) -> bool;

		auto flush_outgoing() -> void;

		auto send_ack(
			const address& to,
			remote_peer& peer,
			time_t<std::uint64_t, milliseconds> now
		) -> void;

		udp_socket m_socket;
		std::unordered_map<address, remote_peer> m_peers;
		spsc_ring_buffer<raw_packet, 1024> m_incoming;
		mpsc_ring_buffer<raw_packet, 1024> m_outgoing;
		std::deque<delayed_packet> m_delayed;
		std::minstd_rand m_loss_rng{ 0x5eedu };
		std::atomic<std::uint64_t> m_dropped{ 0 };
		task::thread m_thread;
	};
}

auto gse::network::endpoint::dropped() const -> std::uint64_t {
	return m_dropped.load(std::memory_order_relaxed);
}

namespace gse::network {
	std::uint32_t simulated_latency_ms = 0;
	std::uint32_t simulated_loss_permille = 0;
}

auto gse::network::endpoint::set_simulation(const std::uint32_t latency_ms, const std::uint32_t loss_permille) -> void {
	simulated_latency_ms = latency_ms;
	simulated_loss_permille = loss_permille;
	if (latency_ms > 0 || loss_permille > 0) {
		log::println(log::category::network, "network simulation: {} ms one-way latency, {} permille loss on every received packet", latency_ms, loss_permille);
	}
}

auto gse::network::endpoint::take_incoming(raw_packet& out) -> bool {
	if (simulated_latency_ms == 0 && simulated_loss_permille == 0) {
		return m_incoming.pop(out);
	}

	const auto now = system_clock::now<time_t<std::uint64_t, milliseconds>>();
	raw_packet arrived;
	while (m_incoming.pop(arrived)) {
		if (simulated_loss_permille > 0 && std::uniform_int_distribution<std::uint32_t>(0, 999)(m_loss_rng) < simulated_loss_permille) {
			continue;
		}
		m_delayed.push_back({
			.due = now + milliseconds(std::uint64_t{ simulated_latency_ms }),
			.packet = arrived,
		});
	}

	if (m_delayed.empty() || m_delayed.front().due > now) {
		return false;
	}
	out = m_delayed.front().packet;
	m_delayed.pop_front();
	return true;
}

gse::network::endpoint::endpoint() = default;

gse::network::endpoint::~endpoint() = default;

auto gse::network::endpoint::bind(const address& local) -> bool {
	if (!m_socket.bind(local)) {
		log::println(
			log::level::error,
			log::category::network,
			"endpoint failed to bind to {}:{}",
			local.ip,
			local.port
		);
		return false;
	}

	start_thread();
	return true;
}

auto gse::network::endpoint::valid() const -> bool {
	return m_socket.valid();
}

auto gse::network::endpoint::local_address() const -> std::optional<address> {
	return m_socket.local_address();
}

auto gse::network::endpoint::start_thread() -> void {
	m_thread = task::spawn(log::thread_role::network, [this](const std::stop_token& st) {
		const time_t<std::uint32_t> max_sleep = milliseconds(4);
		std::array<std::byte, max_packet_size> buffer;

		while (!st.stop_requested()) {
			(void)m_socket.wait_readable(max_sleep);

			while (const auto received = m_socket.receive_data(buffer)) {
				raw_packet pkt;
				pkt.peer = received->from;
				pkt.size = received->bytes_read;
				std::memcpy(pkt.buffer.data(), buffer.data(), received->bytes_read);
				if (!m_incoming.push(pkt)) {
					m_dropped.fetch_add(1, std::memory_order_relaxed);
					break;
				}
			}

			flush_outgoing();
		}

		flush_outgoing();
	});
}

auto gse::network::endpoint::flush_outgoing() -> void {
	raw_packet out;
	while (m_outgoing.pop(out)) {
		const packet wire{
			.data = reinterpret_cast<std::uint8_t*>(out.buffer.data()),
			.size = out.size
		};
		(void)m_socket.send_data(wire, out.peer);
	}
}

auto gse::network::endpoint::ensure_peer(const address& addr) -> remote_peer& {
	if (const auto it = m_peers.find(addr); it != m_peers.end()) {
		return it->second;
	}
	return m_peers.emplace(addr, remote_peer(addr)).first->second;
}

auto gse::network::endpoint::find_peer(const address& addr) -> remote_peer* {
	const auto it = m_peers.find(addr);
	return it == m_peers.end() ? nullptr : &it->second;
}

auto gse::network::endpoint::remove_peer(const address& addr) -> void {
	m_peers.erase(addr);
}

auto gse::network::endpoint::silent_peers(const time_t<std::uint64_t, milliseconds> deadline) const -> std::vector<address> {
	const auto now = system_clock::now<time_t<std::uint64_t, milliseconds>>();

	std::vector<address> out;
	for (const auto& [addr, peer] : m_peers) {
		if (now >= peer.last_traffic() + deadline) {
			out.push_back(addr);
		}
	}
	return out;
}

auto gse::network::endpoint::peers() const -> const std::unordered_map<address, remote_peer>& {
	return m_peers;
}

auto gse::network::endpoint::poll(const std::function<void(inbound_message&)>& on_message) -> void {
	constexpr std::size_t max_packets_per_poll = 256;

	raw_packet pkt;
	for (std::size_t processed = 0; processed < max_packets_per_poll && take_incoming(pkt); ++processed) {
		const std::span data(pkt.buffer.data(), pkt.size);
		read_bitstream stream(data);

		const auto header = stream.read<packet_header>();
		const auto id = stream.read<std::uint64_t>();

		if (auto* peer = find_peer(pkt.peer)) {
			const auto now = system_clock::now<time_t<std::uint64_t, milliseconds>>();
			peer->note_traffic(now);
			peer->process_acks(header.ack, header.ack_bits);
			peer->ingest_packet_sequence(header.sequence);
			if (id != 0) {
				peer->note_received();
				if (peer->ack_owed_since(now, milliseconds(std::uint64_t{ 30 }))) {
					send_ack(pkt.peer, *peer, now);
				}
			}
		}
		if (id == 0) {
			continue;
		}

		const auto remaining = stream.remaining_bytes();
		inbound_message msg{
			.from = pkt.peer,
			.id = id,
			.payload = std::vector<std::byte>(remaining),
		};
		if (remaining > 0) {
			stream.read_bytes(msg.payload.data(), remaining);
		}

		on_message(msg);
	}
}

auto gse::network::endpoint::resend_reliable() -> void {
	const time_t<std::uint64_t, milliseconds> retry_interval = milliseconds(std::uint64_t{ 200 });

	for (auto& [addr, peer] : m_peers) {
		for (auto* msg : peer.messages_to_resend(retry_interval)) {
			raw_packet pkt;
			pkt.peer = addr;

			const packet_header header{
				.sequence = ++peer.sequence(),
				.ack = peer.remote_ack_sequence(),
				.ack_bits = peer.remote_ack_bitfield()
			};

			write_bitstream stream(pkt.buffer);
			stream.write(header);

			constexpr std::size_t header_size = sizeof(packet_header);
			if (msg->data.size() > header_size) {
				stream.write_bytes(msg->data.data() + header_size, msg->data.size() - header_size);
			}

			pkt.size = stream.bytes_written();
			m_outgoing.push(pkt);

			msg->sequence = header.sequence;
			msg->sent_time = system_clock::now<time_t<std::uint64_t, milliseconds>>();
			peer.note_sent(msg->sent_time);
			++msg->send_count;
		}
	}

	const time_t<std::uint64_t, milliseconds> ack_idle = milliseconds(std::uint64_t{ 30 });
	const auto now = system_clock::now<time_t<std::uint64_t, milliseconds>>();
	for (auto& [addr, peer] : m_peers) {
		if (peer.ack_owed_since(now, ack_idle)) {
			send_ack(addr, peer, now);
		}
	}
}

auto gse::network::endpoint::send_ack(const address& to, remote_peer& peer, const time_t<std::uint64_t, milliseconds> now) -> void {
	raw_packet pkt;
	pkt.peer = to;
	const packet_header header{
		.sequence = ++peer.sequence(),
		.ack = peer.remote_ack_sequence(),
		.ack_bits = peer.remote_ack_bitfield()
	};
	write_bitstream stream(pkt.buffer);
	stream.write(header);
	stream.write(std::uint64_t{ 0 });
	pkt.size = stream.bytes_written();
	m_outgoing.push(pkt);
	peer.note_sent(now);
}

template <gse::network::is_network_message T>
auto gse::network::endpoint::send(const T& msg, const address& to, const bool reliable) -> void {
	raw_packet pkt;
	pkt.peer = to;

	auto* peer = find_peer(to);

	const packet_header header = peer
		? packet_header{
			.sequence = ++peer->sequence(),
			.ack = peer->remote_ack_sequence(),
			.ack_bits = peer->remote_ack_bitfield()
		}
		: packet_header{};

	write_bitstream stream(pkt.buffer);
	stream.write(header);
	write(stream, msg);

	pkt.size = stream.bytes_written();

	if (reliable && peer) {
		peer->queue_reliable(header.sequence, std::span(pkt.buffer.data(), pkt.size));
	}

	m_outgoing.push(pkt);
	if (peer) {
		peer->note_sent(system_clock::now<time_t<std::uint64_t, milliseconds>>());
	}
}
