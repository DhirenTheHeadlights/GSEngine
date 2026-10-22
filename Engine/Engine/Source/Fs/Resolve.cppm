export module gse.fs:resolve;

import std;

export namespace gse::fs {
	auto resolve_links(
		const std::filesystem::path& path
	) -> std::expected<std::filesystem::path, std::string>;
}
