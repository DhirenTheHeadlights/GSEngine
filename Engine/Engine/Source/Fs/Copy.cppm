export module gse.fs:copy;

import std;

export namespace gse::fs {
	auto copy_file(
		const std::filesystem::path& from,
		const std::filesystem::path& to
	) -> std::expected<void, std::string>;

	auto copy_tree(
		const std::filesystem::path& from,
		const std::filesystem::path& to
	) -> std::expected<std::size_t, std::string>;

	auto read_text(
		const std::filesystem::path& path
	) -> std::string;

	auto write_text(
		const std::filesystem::path& path,
		std::string_view text
	) -> std::expected<void, std::string>;
}

auto gse::fs::copy_file(const std::filesystem::path& from, const std::filesystem::path& to) -> std::expected<void, std::string> {
	std::error_code ec;
	std::filesystem::create_directories(to.parent_path(), ec);
	std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, ec);
	if (ec) {
		return std::unexpected(std::format("could not copy {} to {}: {}", from.generic_display_string(), to.generic_display_string(), ec.message()));
	}
	return {};
}

auto gse::fs::copy_tree(const std::filesystem::path& from, const std::filesystem::path& to) -> std::expected<std::size_t, std::string> {
	std::error_code ec;
	std::size_t count = 0;
	for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(from, ec)) {
		if (!entry.is_regular_file()) {
			continue;
		}
		if (const auto copied = fs::copy_file(entry.path(), to / entry.path().lexically_relative(from)); !copied) {
			return std::unexpected(copied.error());
		}
		++count;
	}
	if (ec) {
		return std::unexpected(std::format("could not walk {}: {}", from.generic_display_string(), ec.message()));
	}
	return count;
}

auto gse::fs::read_text(const std::filesystem::path& path) -> std::string {
	std::ifstream in(path, std::ios::binary);
	return { std::istreambuf_iterator(in), std::istreambuf_iterator<char>() };
}

auto gse::fs::write_text(const std::filesystem::path& path, const std::string_view text) -> std::expected<void, std::string> {
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	std::ofstream out(path, std::ios::binary);
	out.write(text.data(), static_cast<std::streamsize>(text.size()));
	if (!out) {
		return std::unexpected(std::format("could not write {}", path.generic_display_string()));
	}
	return {};
}
