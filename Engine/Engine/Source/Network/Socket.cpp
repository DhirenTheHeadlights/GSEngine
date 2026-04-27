module;

#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
#else
	#include <arpa/inet.h>
	#include <fcntl.h>
	#include <netinet/in.h>
	#include <poll.h>
	#include <sys/socket.h>
	#include <sys/types.h>
	#include <unistd.h>
	#include <cerrno>
#endif

#undef assert

module gse.network;

import std;

import :socket;

import gse.assert;
import gse.core;
import gse.log;
import gse.math;

namespace gse::network {
#ifdef _WIN32
	using native_socket = ::SOCKET;
	constexpr native_socket native_invalid = INVALID_SOCKET;
	constexpr int native_error = SOCKET_ERROR;

	auto last_error_native() -> int {
		return ::WSAGetLastError();
	}

	auto close_native(native_socket s) -> void {
		::closesocket(s);
	}

	auto set_nonblocking_native(native_socket s) -> bool {
		u_long mode = 1;
		return ::ioctlsocket(s, FIONBIO, &mode) != SOCKET_ERROR;
	}

	struct winsock_lifetime {
		winsock_lifetime() {
			WSADATA data;
			if (const int r = ::WSAStartup(MAKEWORD(2, 2), &data); r != 0) {
				log::println(log::level::error, log::category::network, "WSAStartup failed: {}", r);
				std::terminate();
			}
		}

		~winsock_lifetime() {
			::WSACleanup();
		}
	};

	auto ensure_initialized() -> void {
		static const winsock_lifetime _;
	}
#else
	using native_socket = int;
	constexpr native_socket native_invalid = -1;
	constexpr int native_error = -1;

	auto last_error_native() -> int {
		return errno;
	}

	auto close_native(native_socket s) -> void {
		::close(s);
	}

	auto set_nonblocking_native(native_socket s) -> bool {
		const int flags = ::fcntl(s, F_GETFL, 0);
		if (flags == -1) {
			return false;
		}
		return ::fcntl(s, F_SETFL, flags | O_NONBLOCK) != -1;
	}

	auto ensure_initialized() -> void {}
#endif

	auto to_native(std::uint64_t h) -> native_socket {
		return static_cast<native_socket>(h);
	}

	auto from_native(native_socket s) -> std::uint64_t {
		return static_cast<std::uint64_t>(s);
	}

	constexpr auto handle_invalid = ~std::uint64_t{0};

	auto sockaddr_to_address(const sockaddr_in& src) -> address {
		char buf[INET_ADDRSTRLEN] = {};
		::inet_ntop(AF_INET, &src.sin_addr, buf, sizeof(buf));
		return {
			.ip = std::string{buf},
			.port = ::ntohs(src.sin_port),
		};
	}

	auto make_sockaddr(const address& a) -> sockaddr_in {
		sockaddr_in result{
			.sin_family = AF_INET,
			.sin_port = ::htons(a.port),
			.sin_addr = {},
		};
		::inet_pton(AF_INET, a.ip.c_str(), &result.sin_addr);
		return result;
	}
}

gse::network::udp_socket::udp_socket() {
	ensure_initialized();
	const native_socket s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	assert(s != native_invalid, std::source_location::current(), "Failed to create socket.");
	m_handle = from_native(s);
}

gse::network::udp_socket::~udp_socket() {
	if (m_handle != handle_invalid) {
		close_native(to_native(m_handle));
	}
}

gse::network::udp_socket::udp_socket(udp_socket&& other) noexcept : m_handle(other.m_handle), m_local_address(std::move(other.m_local_address)) {
	other.m_handle = handle_invalid;
}

auto gse::network::udp_socket::operator=(udp_socket&& other) noexcept -> udp_socket& {
	if (this != &other) {
		if (m_handle != handle_invalid) {
			close_native(to_native(m_handle));
		}
		m_handle = other.m_handle;
		m_local_address = std::move(other.m_local_address);
		other.m_handle = handle_invalid;
	}
	return *this;
}

auto gse::network::udp_socket::bind(const address& address) -> bool {
	if (m_handle == handle_invalid) {
		return false;
	}

	const native_socket s = to_native(m_handle);

	int opt = 1;
	::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

	sockaddr_in addr = make_sockaddr(address);

	if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == native_error) {
		log::println(log::level::error, log::category::network, "Failed to bind socket: error {}", last_error_native());
		close_native(s);
		m_handle = handle_invalid;
		return false;
	}

	sockaddr_in bound{};
	socklen_t bound_len = sizeof(bound);
	if (::getsockname(s, reinterpret_cast<sockaddr*>(&bound), &bound_len) == 0) {
		m_local_address = sockaddr_to_address(bound);
	}

	if (!set_nonblocking_native(s)) {
		log::println(log::level::error, log::category::network, "Failed to set non-blocking mode: error {}", last_error_native());
		close_native(s);
		m_handle = handle_invalid;
		return false;
	}

	return true;
}

auto gse::network::udp_socket::local_address() const -> std::optional<address> {
	if (m_handle == handle_invalid || m_local_address.port == 0) {
		return std::nullopt;
	}
	return m_local_address;
}

auto gse::network::udp_socket::valid() const -> bool {
	return m_handle != handle_invalid;
}

auto gse::network::udp_socket::send_data(const packet& packet, const address& address) const -> socket_state {
	const native_socket s = to_native(m_handle);
	sockaddr_in addr = make_sockaddr(address);

#ifdef _WIN32
	const auto buf = reinterpret_cast<const char*>(packet.data);
	const auto len = static_cast<int>(packet.size);
#else
	const auto buf = reinterpret_cast<const void*>(packet.data);
	const auto len = packet.size;
#endif

	if (::sendto(s, buf, len, 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == native_error) {
		log::println(log::level::error, log::category::network, "Socket sendto failed with error {}", last_error_native());
		return socket_state::error;
	}

	return socket_state::sending;
}

auto gse::network::udp_socket::receive_data(std::span<std::byte> buffer) const -> std::optional<receive_result> {
	const native_socket s = to_native(m_handle);

	sockaddr_in src{};
	socklen_t src_len = sizeof(src);

#ifdef _WIN32
	const auto buf = reinterpret_cast<char*>(buffer.data());
	const auto cap = static_cast<int>(buffer.size());
	using recv_result_t = int;
#else
	const auto buf = reinterpret_cast<void*>(buffer.data());
	const auto cap = buffer.size();
	using recv_result_t = ::ssize_t;
#endif

	const recv_result_t r = ::recvfrom(s, buf, cap, 0, reinterpret_cast<sockaddr*>(&src), &src_len);
	if (r == native_error) {
		return std::nullopt;
	}

	return receive_result{
		.bytes_read = static_cast<std::size_t>(r),
		.from = sockaddr_to_address(src),
	};
}

auto gse::network::udp_socket::wait_readable(const time_t<std::uint32_t> timeout) const -> wait_result {
	const native_socket s = to_native(m_handle);

#ifdef _WIN32
	WSAPOLLFD pfd{
		.fd = s,
		.events = POLLRDNORM | POLLERR | POLLHUP,
		.revents = 0,
	};
	const int rv = ::WSAPoll(&pfd, 1, static_cast<int>(timeout.as<milliseconds>()));
#else
	pollfd pfd{
		.fd = s,
		.events = POLLIN | POLLERR | POLLHUP,
		.revents = 0,
	};
	const int rv = ::poll(&pfd, 1, static_cast<int>(timeout.as<milliseconds>()));
#endif

	if (rv == 0) {
		return wait_result::timeout;
	}
	if (rv < 0) {
		return wait_result::error;
	}
	if (pfd.revents & (POLLERR | POLLHUP)) {
		return wait_result::error;
	}
	if (pfd.revents & POLLIN) {
		return wait_result::ready;
	}
	return wait_result::timeout;
}

auto gse::network::udp_socket::id() const -> std::uint64_t {
	return m_handle;
}
