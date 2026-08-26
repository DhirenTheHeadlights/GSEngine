export module gse.network:bitstream;

import std;

import gse.assert;
import gse.log;
import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

export namespace gse::network {
	struct read_mode {};
	struct write_mode {};

	template <typename Mode>
	constexpr bool is_read_mode_v = std::is_same_v<Mode, read_mode>;

	template <typename Mode>
	class bitstream {
		using byte_t = std::conditional_t<is_read_mode_v<Mode>, const std::byte, std::byte>;

	public:
		explicit bitstream(
			std::span<byte_t> buffer
		);

		template <is_trivially_copyable T>
		requires is_read_mode_v<Mode>
		auto read() -> T;

		auto read(
			std::span<std::byte> data
		) -> void
		requires is_read_mode_v<Mode>;

		auto read_bytes(
			std::byte* data,
			std::size_t bytes
		) -> void
		requires is_read_mode_v<Mode>;

		auto remaining_bytes() const -> std::size_t
		requires is_read_mode_v<Mode>;

		enum class read_result {
			ok,
			incomplete
		};

		auto try_read(
			std::span<std::byte> data
		) -> read_result
		requires is_read_mode_v<Mode>;

		template <is_trivially_copyable T>
		requires(!is_read_mode_v<Mode>)
		auto write(
			const T& data
		) -> void;

		auto write(
			std::span<const std::byte> data
		) -> void
		requires(!is_read_mode_v<Mode>);

		auto write_bytes(
			const std::byte* data,
			std::size_t bytes
		) -> void
		requires(!is_read_mode_v<Mode>);

		auto bytes_written() const -> std::size_t
		requires(!is_read_mode_v<Mode>);

		auto reset(
			std::span<std::byte> buffer
		) -> void
		requires(!is_read_mode_v<Mode>);

		auto capacity_bits() const -> std::size_t;

		auto remaining_bits() const -> std::size_t;

		auto good() const -> bool;

		auto error() const -> bool;

		auto seek(
			std::size_t bit_pos
		) -> void;

	private:
		auto can_advance(
			std::size_t bits
		) const -> bool;

		std::span<byte_t> m_buffer;
		std::size_t m_head_bits = 0;
		bool m_error = false;
	};

	using read_bitstream = bitstream<read_mode>;
	using write_bitstream = bitstream<write_mode>;
}

template <typename Mode>
gse::network::bitstream<Mode>::bitstream(const std::span<byte_t> buffer) : m_buffer(buffer) {
}

template <typename Mode>
template <gse::is_trivially_copyable T>
requires gse::network::is_read_mode_v<Mode>
auto gse::network::bitstream<Mode>::read() -> T {
	std::array<std::byte, sizeof(T)> buf{};
	read(std::span<std::byte>(buf));
	return std::bit_cast<T>(buf);
}

template <typename Mode>
auto gse::network::bitstream<Mode>::read(std::span<std::byte> data) -> void
requires is_read_mode_v<Mode>
{
	const std::size_t bits = data.size_bytes() * 8;
	const bool ok = can_advance(bits);

	if (!ok) {
		log::println(
			log::level::warning,
			log::category::network,
			"Incomplete packet read: need {} bits, have {} bits available",
			bits,
			m_buffer.size() * 8 - m_head_bits
		);
		std::fill(data.begin(), data.end(), std::byte(0));
		m_error = true;
		return;
	}

	if ((m_head_bits % 8) == 0) {
		const std::size_t byte_index = m_head_bits / 8;
		memcpy(data.data(), m_buffer.data() + byte_index, data.size_bytes());
		m_head_bits += bits;
		return;
	}

	std::ranges::fill(
		data,
		std::byte{ 0 }
	);
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

template <typename Mode>
auto gse::network::bitstream<Mode>::read_bytes(std::byte* data, const std::size_t bytes) -> void
requires is_read_mode_v<Mode>
{
	read(std::span(data, bytes));
}

template <typename Mode>
auto gse::network::bitstream<Mode>::remaining_bytes() const -> std::size_t
requires is_read_mode_v<Mode>
{
	return remaining_bits() / 8;
}

template <typename Mode>
auto gse::network::bitstream<Mode>::try_read(std::span<std::byte> data) -> read_result
requires is_read_mode_v<Mode>
{
	const std::size_t bits = data.size_bytes() * 8;
	if (!can_advance(bits)) {
		return read_result::incomplete;
	}
	read(data);
	return read_result::ok;
}

template <typename Mode>
template <gse::is_trivially_copyable T>
requires(!gse::network::is_read_mode_v<Mode>)
auto gse::network::bitstream<Mode>::write(const T& data) -> void {
	write(std::as_bytes(std::span{ &data, 1 }));
}

template <typename Mode>
auto gse::network::bitstream<Mode>::write(const std::span<const std::byte> data) -> void
requires(!is_read_mode_v<Mode>)
{
	const std::size_t bits = data.size_bytes() * 8;
	const bool ok = can_advance(bits);

	assert(ok, "write_bitstream overflow need={} have={} head_bits={}", bits, remaining_bits(), m_head_bits);

	if (!ok) {
		m_error = true;
		return;
	}

	if ((m_head_bits % 8) == 0) {
		const std::size_t byte_index = m_head_bits / 8;
		memcpy(m_buffer.data() + byte_index, data);
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

template <typename Mode>
auto gse::network::bitstream<Mode>::write_bytes(const std::byte* data, const std::size_t bytes) -> void
requires(!is_read_mode_v<Mode>)
{
	write(std::span(data, bytes));
}

template <typename Mode>
auto gse::network::bitstream<Mode>::bytes_written() const -> std::size_t
requires(!is_read_mode_v<Mode>)
{
	return (m_head_bits + 7) / 8;
}

template <typename Mode>
auto gse::network::bitstream<Mode>::reset(const std::span<std::byte> buffer) -> void
requires(!is_read_mode_v<Mode>)
{
	m_buffer = buffer;
	m_head_bits = 0;
	m_error = false;
}

template <typename Mode>
auto gse::network::bitstream<Mode>::capacity_bits() const -> std::size_t {
	return m_buffer.size_bytes() * 8;
}

template <typename Mode>
auto gse::network::bitstream<Mode>::remaining_bits() const -> std::size_t {
	const std::size_t cap = capacity_bits();
	return (m_head_bits >= cap) ? 0 : (cap - m_head_bits);
}

template <typename Mode>
auto gse::network::bitstream<Mode>::good() const -> bool {
	return !m_error;
}

template <typename Mode>
auto gse::network::bitstream<Mode>::error() const -> bool {
	return m_error;
}

template <typename Mode>
auto gse::network::bitstream<Mode>::seek(const std::size_t bit_pos) -> void {
	const bool ok = (bit_pos <= capacity_bits());
	assert(ok, "bitstream seek out of range");
	if (!ok) {
		m_error = true;
		return;
	}
	m_head_bits = bit_pos;
}

template <typename Mode>
auto gse::network::bitstream<Mode>::can_advance(const std::size_t bits) const -> bool {
	const std::size_t cap = capacity_bits();
	return bits <= (cap - std::min(m_head_bits, cap));
}