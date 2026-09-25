export module gse.sdk:pack;

import std;

import gse.containers;
import gse.win32;

export namespace gse::sdk {
	struct pack_entry {
		std::string path;
		std::uint64_t offset = 0;
		std::uint64_t compressed = 0;
		std::uint64_t size = 0;
	};

	struct pack_table {
		std::string version;
		std::string preset;
		std::vector<pack_entry> entries;
	};

	struct pack_view {
		std::filesystem::path file;
		std::uint64_t payload_offset = 0;
		pack_table table;
	};

	auto append_pack(
		const std::filesystem::path& root,
		const std::filesystem::path& target,
		std::string_view version,
		std::string_view preset
	) -> std::expected<pack_table, std::string>;

	auto read_pack(
		const std::filesystem::path& file
	) -> std::expected<pack_view, std::string>;

	auto extract_entry(
		std::ifstream& payload,
		const pack_entry& entry,
		const std::filesystem::path& destination
	) -> std::expected<void, std::string>;
}

namespace gse::sdk {
	constexpr std::uint32_t pack_magic = 0x4B504553;
	constexpr std::uint32_t pack_version = 1;
	constexpr std::uint64_t trailer_magic = 0x4B41504B44534553;

	struct pack_trailer {
		std::uint64_t payload_offset = 0;
		std::uint64_t table_offset = 0;
		std::uint64_t magic = trailer_magic;
	};

	auto read_bytes(
		const std::filesystem::path& path
	) -> std::expected<std::vector<char>, std::string>;

	auto compress(
		const std::vector<char>& data
	) -> std::expected<std::vector<char>, std::string>;

	auto write_file(
		const std::filesystem::path& path,
		std::span<const char> bytes
	) -> std::expected<void, std::string>;
}

auto gse::sdk::read_bytes(const std::filesystem::path& path) -> std::expected<std::vector<char>, std::string> {
	std::ifstream in(path, std::ios::binary | std::ios::ate);
	if (!in) {
		return std::unexpected(std::format("could not open {}", path.generic_display_string()));
	}
	const std::streamoff size = in.tellg();
	std::vector<char> bytes(static_cast<std::size_t>(size));
	in.seekg(0);
	in.read(bytes.data(), size);
	if (!in && size > 0) {
		return std::unexpected(std::format("could not read {}", path.generic_display_string()));
	}
	return bytes;
}

auto gse::sdk::compress(const std::vector<char>& data) -> std::expected<std::vector<char>, std::string> {
	if (data.empty()) {
		return std::vector<char>{};
	}
	const std::size_t bound = win32::compressed_size_bound(data.data(), data.size());
	if (bound == 0) {
		return std::unexpected("the LZMS compressor is unavailable");
	}
	std::vector<char> packed(bound);
	std::size_t written = 0;
	if (!win32::compress_lzms(data.data(), data.size(), packed.data(), packed.size(), &written)) {
		return std::unexpected("LZMS compression failed");
	}
	packed.resize(written);
	return packed;
}

auto gse::sdk::write_file(const std::filesystem::path& path, const std::span<const char> bytes) -> std::expected<void, std::string> {
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	std::ofstream out(path, std::ios::binary);
	out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	if (!out) {
		return std::unexpected(std::format("could not write {}", path.generic_display_string()));
	}
	return {};
}

auto gse::sdk::append_pack(const std::filesystem::path& root, const std::filesystem::path& target, const std::string_view version, const std::string_view preset) -> std::expected<pack_table, std::string> {
	std::fstream out(target, std::ios::in | std::ios::out | std::ios::binary | std::ios::ate);
	if (!out) {
		return std::unexpected(std::format("could not open {} for appending", target.generic_display_string()));
	}
	pack_trailer trailer{ .payload_offset = static_cast<std::uint64_t>(out.tellp()) };
	pack_table table{ .version = std::string(version), .preset = std::string(preset) };

	std::error_code ec;
	for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
		if (!entry.is_regular_file()) {
			continue;
		}
		const auto bytes = read_bytes(entry.path());
		if (!bytes) {
			return std::unexpected(bytes.error());
		}
		const auto packed = compress(*bytes);
		if (!packed) {
			return std::unexpected(std::format("{}: {}", entry.path().generic_display_string(), packed.error()));
		}
		table.entries.push_back({
			.path = entry.path().lexically_relative(root).generic_native_encoded_string(),
			.offset = static_cast<std::uint64_t>(out.tellp()),
			.compressed = packed->size(),
			.size = bytes->size(),
		});
		out.write(packed->data(), static_cast<std::streamsize>(packed->size()));
	}
	if (ec) {
		return std::unexpected(std::format("could not walk {}: {}", root.generic_display_string(), ec.message()));
	}
	if (table.entries.empty()) {
		return std::unexpected(std::format("{} holds no files to pack", root.generic_display_string()));
	}

	trailer.table_offset = static_cast<std::uint64_t>(out.tellp());
	binary_writer writer(out, pack_magic, pack_version);
	writer & table;
	out.write(reinterpret_cast<const char*>(&trailer), sizeof(trailer));
	if (!out) {
		return std::unexpected(std::format("could not write the pack table to {}", target.generic_display_string()));
	}
	return table;
}

auto gse::sdk::read_pack(const std::filesystem::path& file) -> std::expected<pack_view, std::string> {
	std::ifstream in(file, std::ios::binary | std::ios::ate);
	if (!in) {
		return std::unexpected(std::format("could not open {}", file.generic_display_string()));
	}
	const std::streamoff end = in.tellg();
	if (end < static_cast<std::streamoff>(sizeof(pack_trailer))) {
		return std::unexpected(std::format("{} carries no SDK payload", file.generic_display_string()));
	}
	pack_trailer trailer{};
	in.seekg(end - static_cast<std::streamoff>(sizeof(pack_trailer)));
	in.read(reinterpret_cast<char*>(&trailer), sizeof(trailer));
	if (!in || trailer.magic != trailer_magic || trailer.table_offset >= static_cast<std::uint64_t>(end)) {
		return std::unexpected(std::format("{} carries no SDK payload", file.generic_display_string()));
	}

	in.seekg(static_cast<std::streamoff>(trailer.table_offset));
	auto reader = binary_reader::open(in, pack_magic, pack_version);
	if (!reader) {
		return std::unexpected(std::format("{} carries a payload table this installer cannot read", file.generic_display_string()));
	}
	pack_view view{ .file = file, .payload_offset = trailer.payload_offset };
	*reader & view.table;
	if (!reader->valid()) {
		return std::unexpected(std::format("{} carries a truncated payload table", file.generic_display_string()));
	}
	return view;
}

auto gse::sdk::extract_entry(std::ifstream& payload, const pack_entry& entry, const std::filesystem::path& destination) -> std::expected<void, std::string> {
	std::vector<char> packed(static_cast<std::size_t>(entry.compressed));
	payload.seekg(static_cast<std::streamoff>(entry.offset));
	payload.read(packed.data(), static_cast<std::streamsize>(packed.size()));
	if (!payload) {
		return std::unexpected(std::format("{}: payload truncated", entry.path));
	}
	std::vector<char> bytes(static_cast<std::size_t>(entry.size));
	if (!bytes.empty() && !win32::decompress_lzms(packed.data(), packed.size(), bytes.data(), bytes.size())) {
		return std::unexpected(std::format("{}: LZMS decompression failed", entry.path));
	}
	return write_file(destination / entry.path, bytes);
}
