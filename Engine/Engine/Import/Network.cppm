module;

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

export module gse.network;

import gse.utility;

export import :actions;
export import :remote_peer;
export import :socket;
export import :bitstream;
export import :packet_header;
export import :message;
export import :connection;
export import :ping_pong;
export import :input_frame;
export import :notify_scene_change;
export import :api;
export import :client;
export import :discovery;
export import :registry_sync;
export import :replication;

export namespace gse::network {
	auto initialize() -> void;
	auto update(registry& reg) -> void;
	auto shutdown() -> void;
}

auto gse::network::initialize() -> void {
	WSADATA wsa_data;
	if (const auto result = WSAStartup(MAKEWORD(2, 2), &wsa_data); result != 0) {
		std::println("WSAStartup failed: {}", result);
	}
}

auto gse::network::update(registry& reg) -> void {
	update_registry(reg);
}

auto gse::network::shutdown() -> void {
	if (const auto result = WSACleanup(); result != 0) {
		std::println("WSACleanup failed: {}", result);
	}
}

