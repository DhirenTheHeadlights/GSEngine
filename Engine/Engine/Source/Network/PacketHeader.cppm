export module gse.network:packet_header;

import std;

export namespace gse {
	struct packet_header {
		std::uint32_t sequence = 0;
		std::uint32_t ack = 0;
		std::uint32_t ack_bits = 0;
	};

	constexpr std::uint32_t max_packet_size = 1200;

	auto sequence_more_recent(
		std::uint32_t lhs,
		std::uint32_t rhs
	) -> bool;

	auto sequence_distance(
		std::uint32_t newer,
		std::uint32_t older
	) -> std::uint32_t;
}

auto gse::sequence_more_recent(const std::uint32_t lhs, const std::uint32_t rhs) -> bool {
	constexpr std::uint32_t half = std::uint32_t{ 1 } << 31;
	return lhs != rhs && static_cast<std::uint32_t>(lhs - rhs) < half;
}

auto gse::sequence_distance(const std::uint32_t newer, const std::uint32_t older) -> std::uint32_t {
	return static_cast<std::uint32_t>(newer - older);
}
