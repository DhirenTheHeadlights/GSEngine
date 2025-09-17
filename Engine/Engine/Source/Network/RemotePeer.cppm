export module gse.network:remote_peer;

import std;

import :socket;

export namespace gse::network {
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
	private:
		address m_address;

		std::uint32_t m_sequence = 0;
		std::uint32_t m_remote_ack_sequence = 0;
		std::uint32_t m_remote_ack_bitfield = 0;
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