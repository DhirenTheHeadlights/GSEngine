module;

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

#undef assert

export module gse.network:socket;

import std;

import gse.assert;
import gse.physics.math;

namespace gse::network {
	struct winsock_initializer {
		winsock_initializer() {
			WSADATA wsa_data;
			if (const int result = WSAStartup(MAKEWORD(2, 2), &wsa_data); result != 0) {
				std::println(std::cerr, "WSAStartup failed with error: {}", result);
				std::terminate();
			}
		}
		~winsock_initializer() {
			WSACleanup();
		}
	};

	static winsock_initializer g_winsock_init;
}

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

	enum struct wait_result : std::uint8_t {
		ready,
		timeout,
		error
	};

	class udp_socket {
	public:
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

		auto wait_readable(
			time_t<std::uint32_t> timeout
		) const -> wait_result;

		auto id(
		) const -> std::uint64_t;
	private:
		std::uint64_t m_socket_id;
	};
}

gse::network::udp_socket::udp_socket() : m_socket_id(INVALID_SOCKET) {
	m_socket_id = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	assert(m_socket_id != INVALID_SOCKET, std::source_location::current(), "Failed to create socket.");
}

gse::network::udp_socket::~udp_socket() {
	if (m_socket_id != INVALID_SOCKET) {
		closesocket(m_socket_id);
	}
}

auto gse::network::udp_socket::bind(const address& address) const -> void {
	sockaddr_in addr {
		.sin_family = AF_INET,
		.sin_port = htons(address.port),
		.sin_addr = {}
	};

	inet_pton(AF_INET, address.ip.c_str(), &addr.sin_addr);

	if (::bind(m_socket_id, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
		assert(false, std::source_location::current(), "Failed to bind socket.");
		closesocket(m_socket_id);
	}

	u_long mode = 1;
	if (ioctlsocket(m_socket_id, FIONBIO, &mode) == SOCKET_ERROR) {
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

	if (const auto result = sendto(m_socket_id, reinterpret_cast<const char*>(packet.data), packet.size, 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)); result == SOCKET_ERROR) {
		return socket_state::error;
	}

	return socket_state::sending;
}

auto gse::network::udp_socket::receive_data(std::span<std::byte> buffer) const -> std::optional<receive_result> {
	sockaddr_in addr;
	int addr_len = sizeof(addr);

	const int result = ::recvfrom(
		m_socket_id,
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

auto gse::network::udp_socket::wait_readable(const time_t<std::uint32_t> timeout) const -> wait_result {
	WSAPOLLFD pfd{
		.fd = static_cast<SOCKET>(m_socket_id),
		.events = POLLRDNORM | POLLERR | POLLHUP
	};

	const int rv = WSAPoll(&pfd, 1, static_cast<int>(timeout.as<milliseconds>()));
	if (rv == 0) {
		return wait_result::timeout;
	}
	if (rv < 0) {
		return wait_result::error;
	}

	if (pfd.revents & (POLLERR | POLLHUP)) {
		return wait_result::error;
	}
	if (pfd.revents & (POLLRDNORM | POLLPRI | POLLIN)) {
		return wait_result::ready;
	}

	return wait_result::timeout;
}

auto gse::network::udp_socket::id() const -> std::uint64_t {
	return m_socket_id;
}


