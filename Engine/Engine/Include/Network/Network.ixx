module;

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

export module gse.network;

import gse.platform.perma_assert;

export namespace gse::network {
	auto initialize() -> void;
	auto shutdown() -> void;
	auto bind_socket() -> void;
}

auto gse::network::initialize() -> void {
	WSADATA wsa_data;
	if (const auto result = WSAStartup(MAKEWORD(2, 2), &wsa_data); result != 0) {
		std::cerr << "WSAStartup failed: " << result << '\n';
	}
}

auto gse::network::shutdown() -> void {
	if (const auto result = WSACleanup(); result != 0) {
		std::cerr << "WSACleanup failed: " << result << '\n';
	}
}

