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
		std::uint16_t port = 0;

		auto operator<=>(
			const address&
		) const = default;
	};

	enum struct address_error : std::uint8_t {
		empty,
		missing_host,
		malformed_port,
		port_out_of_range,
		port_required
	};

	auto parse_address(
		std::string_view text,
		std::uint16_t fallback_port = 0
	) -> std::expected<address, address_error>;

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

		auto local_address() const -> std::optional<address>;

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

		auto id() const -> std::uint64_t;

		auto valid() const -> bool;

	private:
		std::uint64_t m_handle = ~std::uint64_t{ 0 };
		address m_local_address;
	};
}

auto gse::network::parse_address(const std::string_view text, const std::uint16_t fallback_port) -> std::expected<address, address_error> {
	if (text.empty()) {
		return std::unexpected(address_error::empty);
	}

	const auto separator = text.rfind(':');
	const auto host = separator == std::string_view::npos ? text : text.substr(0, separator);

	if (host.empty()) {
		return std::unexpected(address_error::missing_host);
	}

	std::uint16_t port = fallback_port;

	if (separator != std::string_view::npos) {
		const auto digits = text.substr(separator + 1);
		std::uint32_t parsed = 0;
		const auto* first = digits.data();
		const auto* last = first + digits.size();
		const auto result = std::from_chars(first, last, parsed);
		if (result.ec != std::errc{} || result.ptr != last) {
			return std::unexpected(address_error::malformed_port);
		}
		if (parsed == 0 || parsed > 65535) {
			return std::unexpected(address_error::port_out_of_range);
		}
		port = static_cast<std::uint16_t>(parsed);
	}

	if (port == 0) {
		return std::unexpected(address_error::port_required);
	}

	return address{
		.ip = std::string(host),
		.port = port,
	};
}
