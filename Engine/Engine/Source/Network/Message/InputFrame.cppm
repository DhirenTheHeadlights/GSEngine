export module gse.network:input_frame;

import std;

import :message;

export namespace gse::network {
    struct input_frame_header {
        std::uint32_t input_sequence = 0;
        std::uint32_t client_time_ms = 0;
        std::uint16_t action_word_count = 0;
        std::uint16_t axes1_count = 0;
        std::uint16_t axes2_count = 0;
    };

    constexpr auto message_id(
        std::type_identity<input_frame_header>
    ) -> std::uint16_t;

    auto encode(
        bitstream& s,
        const input_frame_header& header
	) -> void;

    auto decode(
        bitstream& s,
        std::type_identity<input_frame_header>
	) -> input_frame_header;
}

constexpr auto gse::network::message_id(std::type_identity<input_frame_header>) -> std::uint16_t {
    return 0x0006;
}

auto gse::network::encode(bitstream& s, const input_frame_header& header) -> void {
	s.write(header);
}

auto gse::network::decode(bitstream& s, std::type_identity<input_frame_header>) -> input_frame_header {
	return s.read<input_frame_header>();
}