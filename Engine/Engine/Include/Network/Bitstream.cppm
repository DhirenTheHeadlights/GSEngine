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

		auto bytes_written() const -> std::size_t;
	private:
		std::span<std::byte> m_buffer;
		std::size_t m_head_bits = 0;
	};
}

gse::network::bitstream::bitstream(const std::span<std::byte> buffer) : m_buffer(buffer) {}

template <gse::is_trivially_copyable T>
auto gse::network::bitstream::write(const T& data) -> void {
	write(std::as_bytes(std::span{ &data, 1 }));
}

auto gse::network::bitstream::write(const std::span<const std::byte> data) -> void {
	assert(
		m_head_bits + data.size_bytes() * 8 <= m_buffer.size_bytes() * 8,
		"Bitstream overflow"
	);

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
	assert(
		m_head_bits + data.size_bytes() * 8 <= m_buffer.size_bytes() * 8,
		"Bitstream underflow"
	);

	std::ranges::fill(data, std::byte{ 0 });

	for (auto& byte_out : data) {
		for (int i = 0; i < 8; ++i) {
			const auto byte_index = m_head_bits / 8;
			const auto bit_index = m_head_bits % 8;

			if ((m_buffer[byte_index] & (std::byte{ 1 } << bit_index)) != std::byte{ 0 }) {
				byte_out |= (std::byte{ 1 } << i);
			}
			m_head_bits++;
		}
	}
}

auto gse::network::bitstream::bytes_written() const -> std::size_t {
	return (m_head_bits + 7) / 8;
}