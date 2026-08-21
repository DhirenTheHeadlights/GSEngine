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

	private:
		auto start_thread() -> void;

		udp_socket m_socket;
		std::unordered_map<address, remote_peer> m_peers;
		spsc_ring_buffer<raw_packet, 1024> m_incoming;
		mpsc_ring_buffer<raw_packet, 1024> m_outgoing;
		std::jthread m_thread;
	};
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
	m_thread = std::jthread([this](const std::stop_token& st) {
		log::name_thread(log::thread_role::network);

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
					break;
				}
			}

			raw_packet out;
			while (m_outgoing.pop(out)) {
				const packet wire{
					.data = reinterpret_cast<std::uint8_t*>(out.buffer.data()),
					.size = out.size
				};
				(void)m_socket.send_data(wire, out.peer);
			}
		}
	});
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

auto gse::network::endpoint::peers() const -> const std::unordered_map<address, remote_peer>& {
	return m_peers;
}

auto gse::network::endpoint::poll(const std::function<void(inbound_message&)>& on_message) -> void {
	constexpr std::size_t max_packets_per_poll = 256;

	raw_packet pkt;
	for (std::size_t processed = 0; processed < max_packets_per_poll && m_incoming.pop(pkt); ++processed) {
		const std::span data(pkt.buffer.data(), pkt.size);
		read_bitstream stream(data);

		const auto header = stream.read<packet_header>();
		const auto id = stream.read<std::uint64_t>();

		if (auto* peer = find_peer(pkt.peer)) {
			peer->process_acks(header.ack, header.ack_bits);
			peer->ingest_packet_sequence(header.sequence);
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

			msg->sent_time = system_clock::now<time_t<std::uint64_t, milliseconds>>();
			++msg->send_count;
		}
	}
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
}
