export module gse.network:packet_header;

import std;

export namespace gse {
	struct packet_header {
		std::uint32_t sequence = 0;
		std::uint32_t ack = 0;
		std::uint32_t ack_bits = 0;
	};

	constexpr std::uint32_t max_packet_size = 1200;
}