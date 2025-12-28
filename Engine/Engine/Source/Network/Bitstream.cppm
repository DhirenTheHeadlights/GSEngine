export module gse.network:bitstream;

import std;

import gse.assert;
import gse.physics.math;
import gse.utility;

export namespace gse::network {
	class bitstream {
	public:
		explicit bitstream(
			std::span<std::byte> buffer
		);

		template <is_trivially_copyable T>
		auto write(
			const T& data
		) -> void;

		auto write(
			std::span<const std::byte> data
		) -> void;

		template <is_trivially_copyable T>
		auto read(
		) -> T;

		auto read(
			std::span<std::byte> data
		) -> void;

		auto bytes_written(
		) const -> std::size_t;

		auto capacity_bits(
		) const -> std::size_t;

		auto remaining_bits(
		) const -> std::size_t;

		auto good(
		) const -> bool;

		auto error(
		) const -> bool;

		auto seek(
			std::size_t bit_pos
		) -> void;

		auto reset(
			std::span<std::byte> buffer
		) -> void;

		auto set_current_message_id(
			std::uint16_t id
		) -> void;

		auto current_message_id(
		) const -> std::uint16_t;
	private:
		auto can_advance(
			std::size_t bits
		) const -> bool;

		std::span<std::byte> m_buffer;
		std::size_t m_head_bits = 0;
		bool m_error = false;
		std::uint16_t m_cur_msg_id = 0;
	};
}

gse::network::bitstream::bitstream(const std::span<std::byte> buffer) : m_buffer(buffer) {}

template <gse::is_trivially_copyable T>
auto gse::network::bitstream::write(const T& data) -> void {
	write(std::as_bytes(std::span{ &data, 1 }));
}

auto gse::network::bitstream::write(const std::span<const std::byte> data) -> void {
	const std::size_t bits = data.size_bytes() * 8;
	const bool ok = can_advance(bits);

	assert(
		ok,
		std::source_location::current(),
		"Bitstream overflow id=0x{:04X} need={} have={} head_bits={}", m_cur_msg_id, bits, remaining_bits(), m_head_bits
	);

	if (!ok) {
		m_error = true;
		return;
	}

	if ((m_head_bits % 8) == 0) {
		const std::size_t byte_index = m_head_bits / 8;
		std::memcpy(m_buffer.data() + byte_index, data.data(), data.size_bytes());
		m_head_bits += bits;
		return;
	}

	for (const auto& byte_in : data) {
		const auto val_in = std::to_integer<std::uint8_t>(byte_in);
		for (std::uint8_t i = 0; i < 8; ++i) {
			const auto byte_index = m_head_bits / 8;
			const auto bit_index = m_head_bits % 8;
			const std::byte mask = (std::byte{ 1 } << bit_index);
			if ((val_in & (1u << i)) != 0) {
				m_buffer[byte_index] |= mask;
			}
			else {
				m_buffer[byte_index] &= ~mask;
			}
			++m_head_bits;
		}
	}
}

template <gse::is_trivially_copyable T>
auto gse::network::bitstream::read() -> T {
	T data{};
	read(std::as_writable_bytes(std::span{ &data, 1 }));
	return data;
}

auto gse::network::bitstream::read(std::span<std::byte> data) -> void {
	const std::size_t bits = data.size_bytes() * 8;
	const bool ok = can_advance(bits);

	assert(
		ok,
		std::source_location::current(),
		"Bitstream underflow id=0x{:04X} need={} have={} head_bits={}", m_cur_msg_id, bits, remaining_bits(), m_head_bits
	);

	if (!ok) {
		m_error = true;
		std::ranges::fill(data, std::byte{ 0 });
		return;
	}

	if ((m_head_bits % 8) == 0) {
		const std::size_t byte_index = m_head_bits / 8;
		std::memcpy(data.data(), m_buffer.data() + byte_index, data.size_bytes());
		m_head_bits += bits;
		return;
	}

	std::ranges::fill(data, std::byte{ 0 });
	for (auto& byte_out : data) {
		for (int i = 0; i < 8; ++i) {
			const auto byte_index = m_head_bits / 8;
			if (const auto bit_index = m_head_bits % 8; (m_buffer[byte_index] & (std::byte{ 1 } << bit_index)) != std::byte{ 0 }) {
				byte_out |= (std::byte{ 1 } << i);
			}
			++m_head_bits;
		}
	}
}

auto gse::network::bitstream::bytes_written() const -> std::size_t {
	return (m_head_bits + 7) / 8;
}

auto gse::network::bitstream::capacity_bits() const -> std::size_t {
	return m_buffer.size_bytes() * 8;
}

auto gse::network::bitstream::remaining_bits() const -> std::size_t {
	const std::size_t cap = capacity_bits();
	return (m_head_bits >= cap) ? 0 : (cap - m_head_bits);
}

auto gse::network::bitstream::good() const -> bool {
	return !m_error;
}

auto gse::network::bitstream::error() const -> bool {
	return m_error;
}

auto gse::network::bitstream::seek(const std::size_t bit_pos) -> void {
	const bool ok = (bit_pos <= capacity_bits());
	assert(
		ok,
		std::source_location::current(),
		"Bitstream seek out of range"
	);
	if (!ok) {
		m_error = true;
		return;
	}
	m_head_bits = bit_pos;
}

auto gse::network::bitstream::reset(const std::span<std::byte> buffer) -> void {
	m_buffer = buffer;
	m_head_bits = 0;
	m_error = false;
	m_cur_msg_id = 0;
}

auto gse::network::bitstream::set_current_message_id(const std::uint16_t id) -> void {
	m_cur_msg_id = id;
}

auto gse::network::bitstream::current_message_id() const -> std::uint16_t {
	return m_cur_msg_id;
}

auto gse::network::bitstream::can_advance(const std::size_t bits) const -> bool {
	const std::size_t cap = capacity_bits();
	return bits <= (cap - std::min(m_head_bits, cap));
}