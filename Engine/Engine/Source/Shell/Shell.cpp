module gse.shell;

import std;

namespace gse::shell {
	auto native_form(
		const std::filesystem::path& target
	) -> std::filesystem::path;
}

auto gse::shell::native_form(const std::filesystem::path& target) -> std::filesystem::path {
	std::filesystem::path preferred = target.lexically_normal();
	preferred.make_preferred();
	return preferred;
}

auto gse::shell::reveal(const std::filesystem::path& target) -> bool {
	if (target.empty()) {
		return false;
	}

	std::error_code ec;
	const std::filesystem::path native = native_form(target);

	if (std::filesystem::is_directory(native, ec)) {
		return shell_open(native, {});
	}

	if (!std::filesystem::exists(native, ec)) {
		const std::filesystem::path parent = native.parent_path();
		if (parent.empty() || !std::filesystem::exists(parent, ec)) {
			return false;
		}
		return shell_open(parent, {});
	}

	const std::wstring parameters = L"/select,\"" + native.wstring() + L"\"";
	return shell_open("explorer.exe", parameters);
}

auto gse::shell::open_externally(const std::filesystem::path& target) -> bool {
	if (target.empty()) {
		return false;
	}

	std::error_code ec;
	const std::filesystem::path native = native_form(target);
	if (!std::filesystem::exists(native, ec)) {
		return false;
	}
	return shell_open(native, {});
}
