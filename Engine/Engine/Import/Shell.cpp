module gse.shell;

import std;

import gse.win32;

namespace gse::shell {
	auto native_form(
		const std::filesystem::path& target
	) -> std::filesystem::path;

	auto shell_open(
		const wchar_t* file,
		const wchar_t* parameters
	) -> bool;
}

auto gse::shell::native_form(const std::filesystem::path& target) -> std::filesystem::path {
	std::filesystem::path preferred = target.lexically_normal();
	preferred.make_preferred();
	return preferred;
}

auto gse::shell::shell_open(const wchar_t* file, const wchar_t* parameters) -> bool {
	const auto result = win32::ShellExecuteW(nullptr, L"open", file, parameters, nullptr, win32::sw_show_normal);
	return reinterpret_cast<std::intptr_t>(result) > 32;
}

auto gse::shell::reveal(const std::filesystem::path& target) -> bool {
	if (target.empty()) {
		return false;
	}

	std::error_code ec;
	const std::filesystem::path native = native_form(target);

	if (std::filesystem::is_directory(native, ec)) {
		return shell_open(native.c_str(), nullptr);
	}

	if (!std::filesystem::exists(native, ec)) {
		const std::filesystem::path parent = native.parent_path();
		if (parent.empty() || !std::filesystem::exists(parent, ec)) {
			return false;
		}
		return shell_open(parent.c_str(), nullptr);
	}

	const std::wstring parameters = L"/select,\"" + native.wstring() + L"\"";
	return shell_open(L"explorer.exe", parameters.c_str());
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
	return shell_open(native.c_str(), nullptr);
}
