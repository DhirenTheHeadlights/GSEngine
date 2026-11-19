module gse.diag:profile_aggregator_platform;

import std;

import :profile_aggregator;

auto gse::profile::process_alive(std::uint32_t) -> bool {
	return false;
}

auto gse::profile::process_id_text() -> std::string {
	return "web";
}
