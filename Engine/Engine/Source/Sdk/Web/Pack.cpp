module gse.sdk:pack_impl;

import std;

import :pack;

auto gse::sdk::compress(const std::vector<char>&) -> std::expected<std::vector<char>, std::string> {
	return std::unexpected("the LZMS compressor is unavailable on this platform");
}

auto gse::sdk::decompress(std::span<const char>, std::span<char>) -> bool {
	return false;
}

auto gse::sdk::sha256(std::span<const char>, std::span<unsigned char>) -> bool {
	return false;
}
