module gse.network:socket_impl;

import std;

import :socket;

import gse.core;
import gse.log;
import gse.math;

namespace gse::network {
	constexpr auto handle_invalid = ~std::uint64_t{ 0 };
}

auto gse::network::resolve_address(const address& addr) -> std::optional<address> {
	log::println(log::level::warning, log::category::network, "no socket backend on this platform; cannot resolve host '{}'", addr.ip);
	return std::nullopt;
}

gse::network::udp_socket::udp_socket() {
	log::println(log::level::warning, log::category::network, "no socket backend on this platform; every send and receive will fail");
}

gse::network::udp_socket::~udp_socket() = default;

gse::network::udp_socket::udp_socket(udp_socket&& other) noexcept
	: m_handle(other.m_handle), m_local_address(std::move(other.m_local_address)) {
	other.m_handle = handle_invalid;
}

auto gse::network::udp_socket::operator=(udp_socket&& other) noexcept -> udp_socket& {
	if (this != &other) {
		m_handle = other.m_handle;
		m_local_address = std::move(other.m_local_address);
		other.m_handle = handle_invalid;
	}
	return *this;
}

auto gse::network::udp_socket::bind(const address& address) -> bool {
	log::println(log::level::error, log::category::network, "no socket backend on this platform; cannot bind {}:{}", address.ip, address.port);
	return false;
}

auto gse::network::udp_socket::local_address() const -> std::optional<address> {
	return std::nullopt;
}

auto gse::network::udp_socket::valid() const -> bool {
	return false;
}

auto gse::network::udp_socket::send_data(const packet&, const address&) const -> socket_state {
	return socket_state::error;
}

auto gse::network::udp_socket::receive_data(std::span<std::byte>) const -> std::optional<receive_result> {
	return std::nullopt;
}

auto gse::network::udp_socket::wait_readable(time_t<std::uint32_t>) const -> wait_result {
	return wait_result::error;
}

auto gse::network::udp_socket::id() const -> std::uint64_t {
	return m_handle;
}
