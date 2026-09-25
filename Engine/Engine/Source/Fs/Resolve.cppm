export module gse.fs:resolve;

import std;
import gse.win32;

export namespace gse::fs {
	auto resolve_links(
		const std::filesystem::path& path
	) -> std::expected<std::filesystem::path, std::string>;
}

namespace gse::fs {
	constexpr std::wstring_view extended_prefix = LR"(\\?\)";
}

auto gse::fs::resolve_links(const std::filesystem::path& path) -> std::expected<std::filesystem::path, std::string> {
	const win32::HANDLE handle = win32::CreateFileW(
		path.native().c_str(),
		0,
		win32::file_share_read | win32::file_share_write | win32::file_share_delete,
		nullptr,
		win32::open_existing,
		win32::file_flag_backup_semantics,
		nullptr
	);
	if (!win32::valid_handle(handle)) {
		return std::unexpected(std::format("could not open {}", path.generic_display_string()));
	}
	std::wstring resolved(static_cast<std::size_t>(win32::max_path), L'\0');
	const win32::DWORD length = win32::GetFinalPathNameByHandleW(handle, resolved.data(), static_cast<win32::DWORD>(resolved.size()), 0);
	win32::CloseHandle(handle);
	if (length == 0 || length >= resolved.size()) {
		return std::unexpected(std::format("could not resolve the final path of {}", path.generic_display_string()));
	}
	resolved.resize(length);
	if (resolved.starts_with(extended_prefix)) {
		resolved.erase(0, extended_prefix.size());
	}
	return std::filesystem::path(resolved);
}
