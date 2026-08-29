export module gse.shell;

import std;

export namespace gse::shell {
	auto reveal(
		const std::filesystem::path& target
	) -> bool;

	auto open_externally(
		const std::filesystem::path& target
	) -> bool;
}
