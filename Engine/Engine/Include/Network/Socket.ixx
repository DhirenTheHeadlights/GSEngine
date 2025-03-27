module;

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

export module gse.network.socket;

import gse.platform.perma_assert;

export namespace gse::network {
	
	struct packet {
		std::uint8_t* data;
		std::size_t size;
	};

	struct address {
		std::string ip;
		std::uint16_t port;
	};

	enum struct socket_state {
		ready,
		sending,
		receiving,
		error
	};

	struct udp_socket {
		udp_socket() = default;
		~udp_socket();

		auto send_data(const packet& packet, const address& address) const -> socket_state;
		auto receive_data(packet& packet, address& sender_address) const -> socket_state;
		auto bind_socket(uint16_t port) -> bool;

		std::uint64_t socket_id = INVALID_SOCKET;
	};
}

gse::network::udp_socket::~udp_socket() {
	if (socket_id != INVALID_SOCKET) {
		closesocket(socket_id);
	}
}

auto gse::network::udp_socket::send_data(const packet& packet, const address& address) const -> socket_state {
	sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(address.port);

	inet_pton(AF_INET, address.ip.c_str(), &addr.sin_addr);

	if (const auto result = sendto(socket_id, reinterpret_cast<const char*>(packet.data), packet.size, 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)); result == SOCKET_ERROR) {
		return socket_state::error;
	}

	return socket_state::sending;
}

auto gse::network::udp_socket::bind_socket(uint16_t port) -> bool {
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(socket_id, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
		std::cerr << "Failed to bind socket: " << WSAGetLastError() << '\n';
		return false;
	}
	return true;
}

auto gse::network::udp_socket::receive_data(packet& packet, address& sender_address) const -> socket_state {
	sockaddr_in addr;
	int addr_len = sizeof(addr);

	auto result = recvfrom(socket_id, reinterpret_cast<char*>(packet.data), packet.size, 0,
		reinterpret_cast<sockaddr*>(&addr), &addr_len);
	if (result == SOCKET_ERROR) {
		return socket_state::error;
	}

	sender_address.ip = inet_ntoa(addr.sin_addr);
	sender_address.port = ntohs(addr.sin_port);
	return socket_state::receiving;
}