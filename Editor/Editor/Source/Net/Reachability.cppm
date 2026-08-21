export module gse.ide.net;

import std;
import gse;
import gse.sockets;

export namespace gse::ide::net {
	constexpr std::uint64_t no_handle = ~std::uint64_t{ 0 };

	enum class reach : std::uint8_t {
		pending,
		up,
		down,
	};

	struct probe {
		std::uint64_t handle = no_handle;
		std::optional<clock> started;
		std::size_t target = 0;
	};

	auto begin(
		probe& p
	) -> void;

	auto poll(
		probe& p
	) -> reach;

	auto cancel(
		probe& p
	) -> void;

	auto active(
		const probe& p
	) -> bool;
}

namespace gse::ide::net {
	struct endpoint {
		const char* ip = nullptr;
		std::uint16_t port = 0;
	};

	struct winsock_lifetime {
		winsock_lifetime();

		~winsock_lifetime();
	};

	auto targets() -> std::span<const endpoint>;

	auto attempt_timeout() -> time;

	auto ensure_winsock() -> void;

	auto open(
		const endpoint& target
	) -> std::uint64_t;

	auto completed(
		std::uint64_t handle
	) -> reach;

	auto advance(
		probe& p
	) -> reach;
}

gse::ide::net::winsock_lifetime::winsock_lifetime() {
	::WSADATA data;
	if (const int result = ::WSAStartup(sockets::make_word(2, 2), &data); result != 0) {
		log::println(log::level::error, log::category::task, "agent: WSAStartup failed ({}) - connection probing is unavailable", result);
	}
}

gse::ide::net::winsock_lifetime::~winsock_lifetime() {
	::WSACleanup();
}

auto gse::ide::net::targets() -> std::span<const endpoint> {
	static constexpr endpoint list[] = {
		{ .ip = "1.1.1.1", .port = 443 },
		{ .ip = "8.8.8.8", .port = 443 },
	};
	return list;
}

auto gse::ide::net::attempt_timeout() -> time {
	return seconds(3.f);
}

auto gse::ide::net::ensure_winsock() -> void {
	static const winsock_lifetime started;
}

auto gse::ide::net::open(const endpoint& target) -> std::uint64_t {
	ensure_winsock();

	const ::SOCKET s = ::socket(sockets::af_inet, sockets::sock_stream, sockets::ipproto_tcp);
	if (s == sockets::invalid_socket) {
		return no_handle;
	}

	::u_long mode = 1;
	if (::ioctlsocket(s, sockets::fionbio, &mode) == sockets::socket_error) {
		sockets::close_socket(s);
		return no_handle;
	}

	::sockaddr_in addr{
		.sin_family = static_cast<decltype(::sockaddr_in::sin_family)>(sockets::af_inet),
		.sin_port = ::htons(target.port),
		.sin_addr = {},
	};
	::inet_pton(sockets::af_inet, target.ip, &addr.sin_addr);

	if (::connect(s, reinterpret_cast<::sockaddr*>(&addr), sizeof(addr)) == sockets::socket_error && sockets::last_error() != sockets::connect_pending) {
		sockets::close_socket(s);
		return no_handle;
	}

	return static_cast<std::uint64_t>(s);
}

auto gse::ide::net::completed(const std::uint64_t handle) -> reach {
	::WSAPOLLFD pfd{
		.fd = static_cast<::SOCKET>(handle),
		.events = sockets::pollwrnorm,
		.revents = 0,
	};

	const int ready = ::WSAPoll(&pfd, 1, 0);
	if (ready == sockets::socket_error) {
		return reach::down;
	}
	if (ready == 0) {
		return reach::pending;
	}
	if (pfd.revents & (sockets::pollerr | sockets::pollhup)) {
		return reach::down;
	}
	if (!(pfd.revents & sockets::pollwrnorm)) {
		return reach::pending;
	}

	int error = 0;
	::socklen_t size = sizeof(error);
	if (::getsockopt(static_cast<::SOCKET>(handle), sockets::sol_socket, sockets::so_error, reinterpret_cast<char*>(&error), &size) == sockets::socket_error) {
		return reach::down;
	}
	return error == 0 ? reach::up : reach::down;
}

auto gse::ide::net::advance(probe& p) -> reach {
	if (p.handle != no_handle) {
		sockets::close_socket(static_cast<::SOCKET>(p.handle));
		p.handle = no_handle;
	}

	++p.target;
	if (p.target >= targets().size()) {
		p.started.reset();
		p.target = 0;
		return reach::down;
	}

	p.handle = open(targets()[p.target]);
	p.started.emplace();
	return reach::pending;
}

auto gse::ide::net::begin(probe& p) -> void {
	cancel(p);
	p.handle = open(targets().front());
	p.started.emplace();
}

auto gse::ide::net::poll(probe& p) -> reach {
	if (!p.started) {
		return reach::down;
	}
	if (p.handle == no_handle) {
		return advance(p);
	}

	const reach state = completed(p.handle);
	if (state == reach::up) {
		cancel(p);
		return reach::up;
	}
	if (state == reach::pending && p.started->elapsed() < attempt_timeout()) {
		return reach::pending;
	}
	return advance(p);
}

auto gse::ide::net::cancel(probe& p) -> void {
	if (p.handle != no_handle) {
		sockets::close_socket(static_cast<::SOCKET>(p.handle));
		p.handle = no_handle;
	}
	p.started.reset();
	p.target = 0;
}

auto gse::ide::net::active(const probe& p) -> bool {
	return p.started.has_value();
}
