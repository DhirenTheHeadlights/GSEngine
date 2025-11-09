export module gse.network:connection;

import :message;

export namespace gse::network {
	struct connection_request {};

	constexpr auto message_id(
		std::type_identity<connection_request>
	) -> std::uint16_t;

	auto encode(
		bitstream& stream,
		const connection_request&
	) -> void;

	auto decode(
		bitstream& stream,
		std::type_identity<connection_request>
	) -> connection_request;

	struct connection_accepted {};

	constexpr auto message_id(
		std::type_identity<connection_accepted>
	) -> std::uint16_t;

	auto encode(
		bitstream& stream,
		const connection_accepted&
	) -> void;

	auto decode(
		bitstream& stream,
		std::type_identity<connection_accepted>
	) -> connection_accepted;
}

constexpr auto gse::network::message_id(std::type_identity<connection_accepted>) -> std::uint16_t {
	return 0x0001;
}

auto gse::network::encode(bitstream& stream, const connection_accepted&) -> void {
	// No data to encode
}

auto gse::network::decode(bitstream& stream, std::type_identity<connection_accepted>) -> connection_accepted {
	return {};
}

constexpr auto gse::network::message_id(std::type_identity<connection_request>) -> std::uint16_t {
	return 0x0003;
}

auto gse::network::encode(bitstream& stream, const connection_request&) -> void {
	// No data to encode
}

auto gse::network::decode(bitstream& stream, std::type_identity<connection_request>) -> connection_request {
	return {};
}
