module;

#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
#endif

module gse.network;

import std;

import gse.core;

auto gse::network::key_hash::operator()(const address& a) const noexcept -> std::size_t {
    return std::hash<std::string>{}(a.ip) ^ (static_cast<std::size_t>(a.port) << 1);
}

auto gse::network::key_eq::operator()(const address& a, const address& b) const noexcept -> bool {
    return a.ip == b.ip && a.port == b.port;
}
