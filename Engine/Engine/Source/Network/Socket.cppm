module;

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

export module gse.network:socket;

import std;

import gse.assert;

export namespace gse::network {
	struct packet {
		std::uint8_t* data;
		std::size_t size;
	};

	struct address {
		std::string ip;
		std::uint16_t port;

		auto operator<=>(const address&) const = default;
	};

	enum struct socket_state {
		ready,
		sending,
		receiving,
		error,
		empty
	};

	struct udp_socket {
		udp_socket();
		~udp_socket();

		auto bind(
			const address& address
		) const -> void;

		auto send_data(
			const packet& packet, 
			const address& address
		) const -> socket_state;

		struct receive_result {
			std::size_t bytes_read = 0;
			address from;
		};

		auto receive_data(
			std::span<std::byte> buffer
		) const -> std::optional<receive_result>;

		std::uint64_t socket_id;
	};
}

gse::network::udp_socket::udp_socket() : socket_id(INVALID_SOCKET) {
	socket_id = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	assert(socket_id != INVALID_SOCKET, std::source_location::current(), "Failed to create socket.");
}

gse::network::udp_socket::~udp_socket() {
	if (socket_id != INVALID_SOCKET) {
		closesocket(socket_id);
	}
}

auto gse::network::udp_socket::bind(const address& address) const -> void {
	sockaddr_in addr {
		.sin_family = AF_INET,
		.sin_port = htons(address.port),
		.sin_addr = {}
	};

	inet_pton(AF_INET, address.ip.c_str(), &addr.sin_addr);

	if (::bind(socket_id, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
		assert(false, std::source_location::current(), "Failed to bind socket.");
		closesocket(socket_id);
	}

	u_long mode = 1;
	if (ioctlsocket(socket_id, FIONBIO, &mode) == SOCKET_ERROR) {
		assert(false, std::source_location::current(), "Failed to set non-blocking mode.");
	}
}

auto gse::network::udp_socket::send_data(const packet& packet, const address& address) const -> socket_state {
	sockaddr_in addr {
		.sin_family = AF_INET,
		.sin_port = htons(address.port),
		.sin_addr = {}
	};

	inet_pton(AF_INET, address.ip.c_str(), &addr.sin_addr);

	if (const auto result = sendto(socket_id, reinterpret_cast<const char*>(packet.data), packet.size, 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)); result == SOCKET_ERROR) {
		return socket_state::error;
	}

	return socket_state::sending;
}

auto gse::network::udp_socket::receive_data(std::span<std::byte> buffer) const -> std::optional<receive_result> {
	sockaddr_in addr;
	int addr_len = sizeof(addr);

	const int result = ::recvfrom(
		socket_id,
		reinterpret_cast<char*>(buffer.data()),
		static_cast<int>(buffer.size()),
		0,
		reinterpret_cast<sockaddr*>(&addr),
		&addr_len
	);

	if (result == SOCKET_ERROR) {
		if (const int err = WSAGetLastError(); err == WSAEWOULDBLOCK) {
			return std::nullopt;
		}

		return std::nullopt;
	}

	return receive_result{
		.bytes_read = static_cast<std::size_t>(result),
		.from = {
			.ip = inet_ntoa(addr.sin_addr),
			.port = ntohs(addr.sin_port)
		}
	};
}

