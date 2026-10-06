export module gse.tests.net;

import std;

import gse.network;
import gse.test;

export namespace gse::tests::net {
	[[= test::unit{ .tags = "net" }]]
	auto bitstream_round_trips_values(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "net" }]]
	auto bitstream_round_trips_across_a_bit_offset(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "net" }]]
	auto overrunning_a_read_is_reported(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "net" }]]
	auto sequence_comparison_survives_a_wrap(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "net" }]]
	auto ack_tracking_survives_a_sequence_wrap(
		test::context& ctx
	) -> void;
}

namespace gse::tests::net {
	struct sample {
		std::uint16_t id = 0;
		std::int32_t offset = 0;
		float weight = 0.f;

		auto operator==(
			const sample&
		) const -> bool = default;
	};
}

auto gse::tests::net::bitstream_round_trips_values(test::context& ctx) -> void {
	std::array<std::byte, 64> storage{};
	network::write_bitstream writer(storage);

	const sample written{ .id = 7, .offset = -31, .weight = 0.25f };
	const std::array<std::byte, 3> payload{ std::byte{ 0xAB }, std::byte{ 0x00 }, std::byte{ 0xCD } };
	writer.write(std::uint32_t{ 0xDEADBEEF });
	writer.write(written);
	writer.write(std::span<const std::byte>(payload));

	if (!ctx.expect(writer.good())) {
		return;
	}
	ctx.expect_eq(writer.bytes_written(), sizeof(std::uint32_t) + sizeof(sample) + payload.size());

	const std::span<const std::byte> written_bytes(storage);
	network::read_bitstream reader(written_bytes);
	ctx.expect_eq(reader.read<std::uint32_t>(), std::uint32_t{ 0xDEADBEEF });
	ctx.expect_eq(reader.read<sample>(), written);

	std::array<std::byte, 3> restored{};
	reader.read(std::span<std::byte>(restored));
	ctx.expect_eq(restored, payload);
	ctx.expect(reader.good());
}

auto gse::tests::net::bitstream_round_trips_across_a_bit_offset(test::context& ctx) -> void {
	std::array<std::byte, 64> storage{};
	network::write_bitstream writer(storage);
	writer.seek(3);
	writer.write(std::uint32_t{ 0x0F1E2D3C });
	if (!ctx.expect(writer.good())) {
		return;
	}

	const std::span<const std::byte> written_bytes(storage);
	network::read_bitstream reader(written_bytes);
	reader.seek(3);
	ctx.expect_eq(reader.read<std::uint32_t>(), std::uint32_t{ 0x0F1E2D3C });
	ctx.expect(reader.good());
}

auto gse::tests::net::overrunning_a_read_is_reported(test::context& ctx) -> void {
	std::array<std::byte, 4> storage{};
	network::write_bitstream writer(storage);
	writer.write(std::uint32_t{ 1 });

	const std::span<const std::byte> written_bytes(storage);
	network::read_bitstream reader(written_bytes);
	std::array<std::byte, 8> oversized{};
	ctx.expect_eq(reader.try_read(std::span<std::byte>(oversized)), network::read_bitstream::read_result::incomplete);
	ctx.expect(reader.good());

	reader.read(std::span<std::byte>(oversized));
	ctx.expect(reader.error());
}

auto gse::tests::net::sequence_comparison_survives_a_wrap(test::context& ctx) -> void {
	constexpr std::uint32_t last = std::numeric_limits<std::uint32_t>::max();

	ctx.expect(sequence_more_recent(9, 8));
	ctx.expect(!sequence_more_recent(8, 9));
	ctx.expect(!sequence_more_recent(8, 8));

	ctx.expect(sequence_more_recent(0, last));
	ctx.expect(!sequence_more_recent(last, 0));
	ctx.expect(sequence_more_recent(2, last - 1));
	ctx.expect_eq(sequence_distance(2, last - 1), std::uint32_t{ 4 });
}

auto gse::tests::net::ack_tracking_survives_a_sequence_wrap(test::context& ctx) -> void {
	constexpr std::uint32_t last = std::numeric_limits<std::uint32_t>::max();

	network::remote_peer peer(network::address{ .ip = "127.0.0.1", .port = 7777 });
	peer.remote_ack_sequence() = last - 1;
	peer.remote_ack_bitfield() = 0;

	peer.ingest_packet_sequence(last);
	peer.ingest_packet_sequence(0);
	peer.ingest_packet_sequence(1);

	ctx.expect_eq(peer.remote_ack_sequence(), std::uint32_t{ 1 });
	ctx.expect(peer.remote_ack_bitfield() != 0);
}
