export module gse.tests.archive;

import std;

import gse.containers;
import gse.core;
import gse.process;
import gse.test;

export namespace gse::tests::archive {
	[[= test::unit{ .tags = "archive" }]]
	auto reflected_struct_round_trips(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "archive" }]]
	auto archive_skip_is_honoured(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "archive" }]]
	auto truncated_payload_is_rejected(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "archive" }]]
	auto version_mismatch_is_rejected(
		test::context& ctx
	) -> void;
}

namespace gse::tests::archive {
	struct payload {
		std::int32_t count = 0;
		float weight = 0.f;
		bool flagged = false;
		std::string label;
		std::vector<std::int32_t> samples;

		[[= archive_skip{}]]
		std::int32_t scratch = 0;
	};

	constexpr std::uint32_t payload_magic = 0x54535447;
	constexpr std::uint32_t payload_version = 3;

	auto authored() -> payload;

	auto write_payload(
		const std::filesystem::path& path,
		const payload& value,
		std::uint32_t version
	) -> bool;
}

auto gse::tests::archive::authored() -> payload {
	return {
		.count = 42,
		.weight = 1.5f,
		.flagged = true,
		.label = "archive fixture",
		.samples = { 1, 2, 3, 5, 8 },
		.scratch = 7,
	};
}

auto gse::tests::archive::write_payload(const std::filesystem::path& path, const payload& value, const std::uint32_t version) -> bool {
	std::ofstream out(path, std::ios::binary);
	binary_writer writer(out, payload_magic, version);
	writer & value;
	return writer.valid();
}

auto gse::tests::archive::reflected_struct_round_trips(test::context& ctx) -> void {
	const std::filesystem::path path = process::temporary_path("archive_round_trip", "bin");
	const auto _ = make_scope_exit([&path] {
		std::error_code ec;
		std::filesystem::remove(path, ec);
	});

	const payload original = authored();
	if (!ctx.expect(write_payload(path, original, payload_version))) {
		return;
	}

	std::ifstream in(path, std::ios::binary);
	const std::expected<binary_reader, archive_mismatch> opened = binary_reader::open(in, payload_magic, payload_version);
	if (!ctx.expect_value(opened)) {
		return;
	}

	binary_reader reader = *opened;
	payload restored;
	reader & restored;

	if (!ctx.expect(reader.valid())) {
		return;
	}
	ctx.expect_eq(restored.count, original.count);
	ctx.expect_eq(restored.weight, original.weight);
	ctx.expect_eq(restored.flagged, original.flagged);
	ctx.expect_eq(restored.label, original.label);
	ctx.expect_eq(restored.samples, original.samples);
}

auto gse::tests::archive::archive_skip_is_honoured(test::context& ctx) -> void {
	const std::filesystem::path path = process::temporary_path("archive_skip", "bin");
	const auto _ = make_scope_exit([&path] {
		std::error_code ec;
		std::filesystem::remove(path, ec);
	});

	if (!ctx.expect(write_payload(path, authored(), payload_version))) {
		return;
	}

	std::ifstream in(path, std::ios::binary);
	const std::expected<binary_reader, archive_mismatch> opened = binary_reader::open(in, payload_magic, payload_version);
	if (!ctx.expect_value(opened)) {
		return;
	}

	binary_reader reader = *opened;
	payload restored{ .scratch = 3 };
	reader & restored;

	ctx.expect(reader.valid());
	ctx.expect_eq(restored.scratch, 3);
}

auto gse::tests::archive::truncated_payload_is_rejected(test::context& ctx) -> void {
	const std::filesystem::path path = process::temporary_path("archive_truncated", "bin");
	const auto _ = make_scope_exit([&path] {
		std::error_code ec;
		std::filesystem::remove(path, ec);
	});

	if (!ctx.expect(write_payload(path, authored(), payload_version))) {
		return;
	}

	std::error_code ec;
	const std::uintmax_t written = std::filesystem::file_size(path, ec);
	if (!ctx.expect(!ec && written > 16)) {
		return;
	}
	std::filesystem::resize_file(path, written - 8, ec);
	if (!ctx.expect(!ec)) {
		return;
	}

	std::ifstream in(path, std::ios::binary);
	const std::expected<binary_reader, archive_mismatch> opened = binary_reader::open(in, payload_magic, payload_version);
	if (!ctx.expect_value(opened)) {
		return;
	}

	binary_reader reader = *opened;
	payload restored;
	reader & restored;
	ctx.expect(!reader.valid());
}

auto gse::tests::archive::version_mismatch_is_rejected(test::context& ctx) -> void {
	const std::filesystem::path path = process::temporary_path("archive_version", "bin");
	const auto _ = make_scope_exit([&path] {
		std::error_code ec;
		std::filesystem::remove(path, ec);
	});

	if (!ctx.expect(write_payload(path, authored(), payload_version - 1))) {
		return;
	}

	std::ifstream in(path, std::ios::binary);
	const std::expected<binary_reader, archive_mismatch> opened = binary_reader::open(in, payload_magic, payload_version);
	if (!ctx.expect_error(opened)) {
		return;
	}
	ctx.expect(opened.error().readable);
	ctx.expect_eq(opened.error().version, payload_version - 1);
}
