module gse.assets:asset_format_impl;

import gse.win32;
import std;

import :asset_format;

auto gse::replace_baked_file(const std::filesystem::path& temporary, const std::filesystem::path& destination) -> asset_result {
	if (win32::MoveFileExW(
		temporary.c_str(),
		destination.c_str(),
		win32::movefile_replace_existing | win32::movefile_write_through
		) == 0) {
		const auto code = static_cast<int>(win32::GetLastError());
		std::error_code cleanup;
		std::filesystem::remove(temporary, cleanup);
		return std::unexpected(asset_error{
			.code = asset_error_code::io_failure,
			.path = destination,
			.detail = std::system_category().message(code)
		});
	}
	return {};
}
