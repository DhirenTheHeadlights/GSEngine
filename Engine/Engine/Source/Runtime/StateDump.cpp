module gse.runtime:state_dump_impl;

import std;

import :state_dump;

import gse.core;
import gse.math;
import gse.containers;

auto gse::read_state_dump(const std::filesystem::path& path) -> std::expected<std::vector<state_dump_record>, archive_mismatch> {
	std::ifstream stream(path, std::ios::binary);
	if (!stream) {
		return std::unexpected(archive_mismatch{});
	}

	auto reader = binary_reader::open(stream, state_dump_magic, state_dump_version);
	if (!reader) {
		return std::unexpected(reader.error());
	}

	const auto payload_begin = stream.tellg();
	stream.seekg(0, std::ios::end);
	const auto payload = static_cast<std::uint64_t>(stream.tellg() - payload_begin);
	stream.seekg(payload_begin);

	if (payload % sizeof(state_dump_record) != 0) {
		return std::unexpected(archive_mismatch{
			.magic = state_dump_magic,
			.version = state_dump_version,
			.readable = false,
		});
	}

	std::vector<state_dump_record> records;
	records.reserve(payload / sizeof(state_dump_record));
	for (std::uint64_t i = 0; i < payload / sizeof(state_dump_record); ++i) {
		state_dump_record record{};
		*reader & record;
		if (!reader->valid()) {
			break;
		}
		records.push_back(record);
	}

	return records;
}
