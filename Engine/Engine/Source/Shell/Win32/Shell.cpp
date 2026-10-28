module gse.shell;

import gse.win32;
import std;

auto gse::shell::shell_open(const std::filesystem::path& file, const std::wstring& parameters) -> bool {
	const std::wstring wide = file.wstring();
	const auto result = win32::ShellExecuteW(nullptr, L"open", wide.c_str(), parameters.empty() ? nullptr : parameters.c_str(), nullptr, win32::sw_show_normal);
	return reinterpret_cast<std::intptr_t>(result) > 32;
}
