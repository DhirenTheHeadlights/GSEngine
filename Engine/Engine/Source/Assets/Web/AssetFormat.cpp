module gse.assets:asset_format_impl;

import std;

import :asset_format;

auto gse::replace_baked_file(const std::filesystem::path& temporary, const std::filesystem::path& destination) -> asset_result {
	std::error_code ec;
	std::filesystem::rename(temporary, destination, ec);
	if (ec) {
		std::error_code cleanup;
		std::filesystem::remove(temporary, cleanup);
		return std::unexpected(asset_error{
			.code = asset_error_code::io_failure,
			.path = destination,
			.detail = ec.message()
		});
	}
	return {};
}
