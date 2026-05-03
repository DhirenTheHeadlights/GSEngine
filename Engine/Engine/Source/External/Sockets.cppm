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

export module gse.sockets;

export {
	using ::sockaddr;
	using ::sockaddr_in;
	using ::in_addr;
	using ::socklen_t;

	using ::socket;
	using ::bind;
	using ::getsockname;
	using ::setsockopt;
	using ::sendto;
	using ::recvfrom;

#ifdef _WIN32
	using ::SOCKET;
	using ::WSADATA;
	using ::WSAPOLLFD;
	using ::WORD;
	using ::u_long;

	using ::WSAStartup;
	using ::WSACleanup;
	using ::WSAGetLastError;
	using ::closesocket;
	using ::ioctlsocket;
	using ::WSAPoll;
#else
	using ::pollfd;
	using ::ssize_t;

	using ::close;
	using ::fcntl;
	using ::poll;
#endif

	using ::inet_ntop;
	using ::inet_pton;
	using ::htons;
	using ::ntohs;
}

export namespace gse::sockets {
	inline constexpr int af_inet = AF_INET;
	inline constexpr int sock_dgram = SOCK_DGRAM;
	inline constexpr int ipproto_udp = IPPROTO_UDP;
	inline constexpr int sol_socket = SOL_SOCKET;
	inline constexpr int so_reuseaddr = SO_REUSEADDR;
	inline constexpr int inet_addrstrlen = INET_ADDRSTRLEN;

	inline constexpr short pollin = POLLIN;
	inline constexpr short pollerr = POLLERR;
	inline constexpr short pollhup = POLLHUP;

#ifdef _WIN32
	inline constexpr ::SOCKET invalid_socket = INVALID_SOCKET;
	inline constexpr int socket_error = SOCKET_ERROR;
	inline constexpr long fionbio = FIONBIO;
	inline constexpr short pollrdnorm = POLLRDNORM;

	inline auto last_error() -> int {
		return ::WSAGetLastError();
	}

	inline auto close_socket(::SOCKET s) -> void {
		::closesocket(s);
	}

	inline auto make_word(int low, int high) -> ::WORD {
		return MAKEWORD(low, high);
	}
#else
	inline constexpr int invalid_socket = -1;
	inline constexpr int socket_error = -1;

	inline constexpr int o_nonblock = O_NONBLOCK;
	inline constexpr int f_getfl = F_GETFL;
	inline constexpr int f_setfl = F_SETFL;

	inline auto last_error() -> int {
		return errno;
	}

	inline auto close_socket(int s) -> void {
		::close(s);
	}
#endif
}
