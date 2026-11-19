module gse.diag:alloc_track_platform;

import std;

import :alloc_track;

auto gse::alloc::address_space_usage() -> address_space {
	return {};
}

auto gse::alloc::label_of(const std::uint64_t pc) -> std::string {
	return std::format("0x{:x}", pc);
}
