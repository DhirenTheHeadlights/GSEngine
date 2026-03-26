export module gse.network:server_info;

import std;

import :message;
import :bitstream;

export namespace gse::network {
	struct server_info_request {
	};

	constexpr auto message_id(
		std::type_identity<server_info_request>
	) -> std::uint16_t;

	auto encode(
		bitstream& stream,
		const server_info_request&
	) -> void;

	auto decode(
		bitstream& stream,
		std::type_identity<server_info_request>
	) -> server_info_request;

	struct server_info_response {
		std::uint8_t players{};
		std::uint8_t max_players{};
	};

	constexpr auto message_id(
		std::type_identity<server_info_response>
	) -> std::uint16_t;

	auto encode(
		bitstream& stream,
		const server_info_response& msg
	) -> void;

	auto decode(
		bitstream& stream,
		std::type_identity<server_info_response>
	) -> server_info_response;
}

constexpr auto gse::network::message_id(std::type_identity<server_info_request>) -> std::uint16_t {
	return 0x0005;
}

auto gse::network::encode(bitstream&, const server_info_request&) -> void {
}

auto gse::network::decode(bitstream&, std::type_identity<server_info_request>) -> server_info_request {
	return {};
}

constexpr auto gse::network::message_id(std::type_identity<server_info_response>) -> std::uint16_t {
	return 0x0006;
}

auto gse::network::encode(bitstream& stream, const server_info_response& msg) -> void {
	stream.write(msg.players);
	stream.write(msg.max_players);
}

auto gse::network::decode(bitstream& stream, std::type_identity<server_info_response>) -> server_info_response {
	return {
		.players = stream.read<std::uint8_t>(),
		.max_players = stream.read<std::uint8_t>()
	};
}
