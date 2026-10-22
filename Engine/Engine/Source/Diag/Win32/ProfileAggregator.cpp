module gse.diag:profile_aggregator_platform;

import gse.win32;
import std;

import :profile_aggregator;

auto gse::profile::process_alive(const std::uint32_t pid) -> bool {
	if (void* owner = win32::OpenProcess(win32::process_query_limited_information, 0, pid)) {
		win32::CloseHandle(owner);
		return true;
	}
	return false;
}

auto gse::profile::process_id_text() -> std::string {
	return std::format("{}", win32::GetCurrentProcessId());
}
