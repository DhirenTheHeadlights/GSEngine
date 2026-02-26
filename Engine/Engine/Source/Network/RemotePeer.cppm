export module gse.network:remote_peer;

import std;

import :socket;

import gse.math;
import gse.utility;

export namespace gse::network {
	struct pending_reliable_message {
		std::uint32_t sequence = 0;
		std::vector<std::byte> data;
		std::uint64_t sent_time_ms = 0;
		std::uint32_t send_count = 1;
	};

	inline auto current_time_ms() -> std::uint64_t {
		return system_clock::now<time_t<std::uint64_t, milliseconds>>().as<milliseconds>();
	}

	class remote_peer {
	public:
		explicit remote_peer(const address& addr);

		auto addr(
		) const -> const address&;

		auto sequence(
		) -> std::uint32_t&;

		auto remote_ack_sequence(
		) -> std::uint32_t&;

		auto remote_ack_bitfield(
		) -> std::uint32_t&;

		auto queue_reliable(
			std::uint32_t seq,
			std::span<const std::byte> data
		) -> void;

		auto process_acks(
			std::uint32_t ack,
			std::uint32_t ack_bits
		) -> void;

		auto messages_to_resend(
			std::uint32_t retry_interval_ms
		) -> std::vector<pending_reliable_message*>;

		auto pending_reliable_count(
		) const -> std::size_t;

	private:
		address m_address;

		std::uint32_t m_sequence = 0;
		std::uint32_t m_remote_ack_sequence = 0;
		std::uint32_t m_remote_ack_bitfield = 0;

		std::vector<pending_reliable_message> m_pending_reliable;
		std::uint32_t m_last_processed_ack = 0;
	};
}

gse::network::remote_peer::remote_peer(const address& addr) : m_address(addr) {}

auto gse::network::remote_peer::addr() const -> const address& {
	return m_address;
}

auto gse::network::remote_peer::sequence() -> std::uint32_t& {
	return m_sequence;
}

auto gse::network::remote_peer::remote_ack_sequence() -> std::uint32_t& {
	return m_remote_ack_sequence;
}

auto gse::network::remote_peer::remote_ack_bitfield() -> std::uint32_t& {
	return m_remote_ack_bitfield;
}

auto gse::network::remote_peer::queue_reliable(const std::uint32_t seq, const std::span<const std::byte> data) -> void {
	pending_reliable_message msg;
	msg.sequence = seq;
	msg.data.assign(data.begin(), data.end());
	msg.sent_time_ms = current_time_ms();
	msg.send_count = 1;
	m_pending_reliable.push_back(std::move(msg));
}

auto gse::network::remote_peer::process_acks(const std::uint32_t ack, const std::uint32_t ack_bits) -> void {
	if (ack == 0 && ack_bits == 0) {
		return;
	}

	std::erase_if(m_pending_reliable, [ack, ack_bits](const pending_reliable_message& msg) {
		if (msg.sequence == ack) {
			return true;
		}
		if (msg.sequence < ack) {
			const std::uint32_t diff = ack - msg.sequence;
			if (diff <= 32 && (ack_bits & (1u << (diff - 1))) != 0) {
				return true;
			}
		}
		return false;
	});

	m_last_processed_ack = std::max(m_last_processed_ack, ack);
}

auto gse::network::remote_peer::messages_to_resend(const std::uint32_t retry_interval_ms) -> std::vector<pending_reliable_message*> {
	std::vector<pending_reliable_message*> result;
	const auto now = current_time_ms();

	for (auto& msg : m_pending_reliable) {
		if (now - msg.sent_time_ms >= retry_interval_ms) {
			result.push_back(&msg);
		}
	}

	return result;
}

auto gse::network::remote_peer::pending_reliable_count() const -> std::size_t {
	return m_pending_reliable.size();
}