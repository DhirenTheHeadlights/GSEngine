export module gse.network:socket;

import std;

import gse.assert;
import gse.core;
import gse.log;
import gse.math;

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

	class udp_socket : public non_copyable {
	public:
		udp_socket();

		~udp_socket();

		udp_socket(
			udp_socket&& other
		) noexcept;

		auto operator=(
			udp_socket&& other
		) noexcept -> udp_socket&;

		auto bind(
			const address& address
		) -> bool;

		auto local_address(
		) const -> std::optional<address>;

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

		auto valid(
		) const -> bool;
	private:
		std::uint64_t m_handle = ~std::uint64_t{0};
		address m_local_address;
	};
}
