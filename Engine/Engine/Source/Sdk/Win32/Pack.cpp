module gse.sdk:pack_impl;

import std;

import gse.win32;

import :pack;

auto gse::sdk::compress(const std::vector<char>& data) -> std::expected<std::vector<char>, std::string> {
	if (data.empty()) {
		return std::vector<char>{};
	}
	const std::size_t bound = win32::compressed_size_bound(data.data(), data.size());
	if (bound == 0) {
		return std::unexpected("the LZMS compressor is unavailable");
	}
	std::vector<char> packed(bound);
	std::size_t written = 0;
	if (!win32::compress_lzms(data.data(), data.size(), packed.data(), packed.size(), &written)) {
		return std::unexpected("LZMS compression failed");
	}
	packed.resize(written);
	return packed;
}

auto gse::sdk::decompress(const std::span<const char> packed, const std::span<char> out) -> bool {
	return win32::decompress_lzms(packed.data(), packed.size(), out.data(), out.size());
}

auto gse::sdk::sha256(const std::span<const char> data, const std::span<unsigned char> digest) -> bool {
	return win32::hash_sha256(data.data(), data.size(), digest.data());
}
