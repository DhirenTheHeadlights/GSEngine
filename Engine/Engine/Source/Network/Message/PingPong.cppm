export module gse.network:ping_pong;

import std;

import :message;
import :bitstream;

export namespace gse::network {
	struct ping {
		std::uint32_t sequence{};
	};

	constexpr auto message_id(
		std::type_identity<ping>
	) -> std::uint16_t;

	auto encode(
		const ping& msg,
		bitstream& stream
	) -> void;

	auto decode(
		bitstream& stream,
		ping& msg
	) -> void;

	struct pong {
		std::uint32_t sequence{};
	};

	constexpr auto message_id(
		std::type_identity<pong>
	) -> std::uint16_t;

	auto encode(
		const pong& msg,
		bitstream& stream
	) -> void;

	auto decode(
		bitstream& stream,
		pong& msg
	) -> void;
}

constexpr auto gse::network::message_id(std::type_identity<ping>) -> std::uint16_t {
	return 0x0003;
}

auto gse::network::encode(const ping& msg, bitstream& stream) -> void {
	stream.write(msg.sequence);
}

auto gse::network::decode(bitstream& stream, ping& msg) -> void {
	msg.sequence = stream.read<std::uint32_t>();
}

constexpr auto gse::network::message_id(std::type_identity<pong>) -> std::uint16_t {
	return 0x0004;
}

auto gse::network::encode(const pong& msg, bitstream& stream) -> void {
	stream.write(msg.sequence);
}

auto gse::network::decode(bitstream& stream, pong& msg) -> void {
	msg.sequence = stream.read<std::uint32_t>();
}
