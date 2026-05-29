export module gse.vulkan:buffer;

import std;

import :handles;
import :types;

import gse.assert;
import gse.math;
import gse.core;

export namespace gse::vulkan {
	class buffer final : public non_copyable {
	public:
		buffer() = default;

		~buffer() override = default;

		buffer(
			gpu::buffer_handle buffer,
			gpu::device_size size,
			gpu::device_address address,
			std::byte* mapped
		);

		buffer(
			buffer&& other
		) noexcept;

		auto operator=(
			buffer&& other
		) noexcept -> buffer&;

		[[nodiscard]] auto handle() const -> gpu::buffer_handle;

		[[nodiscard]] auto size_bytes() const -> gpu::device_size;

		[[nodiscard]] auto size() const -> gpu::device_size;

		[[nodiscard]] auto device_address() const -> gpu::device_address;

		auto host_write(
			const void* data,
			std::size_t bytes,
			std::size_t offset = 0
		) const -> void;

		template <typename T>
		requires(!std::is_pointer_v<T>)
		auto host_write(
			const T& src,
			std::size_t offset = 0
		) const -> void;

		auto host_zero() const -> void;

		[[nodiscard]] auto host_read() const -> std::span<const std::byte>;

		template <typename T = std::byte>
		[[nodiscard]] auto mapped() const -> T*;

		auto mark_host_dirty() const noexcept -> void;

		[[nodiscard]] auto host_dirty() const noexcept -> bool;

		auto clear_host_dirty() const noexcept -> void;

		[[nodiscard]] auto valid() const -> bool;

	private:
		gpu::buffer_handle m_buffer;
		gpu::device_size m_size = 0;
		gpu::device_address m_address = 0;
		std::byte* m_mapped = nullptr;
		mutable std::atomic<bool> m_host_dirty{ false };
	};
}

gse::vulkan::buffer::buffer(const gpu::buffer_handle buffer, const gpu::device_size size, const gpu::device_address address, std::byte* mapped)
	: m_buffer(buffer), m_size(size), m_address(address), m_mapped(mapped) {
}

gse::vulkan::buffer::buffer(buffer&& other) noexcept
	: m_buffer(other.m_buffer), m_size(other.m_size), m_address(other.m_address), m_mapped(other.m_mapped), m_host_dirty(other.m_host_dirty.load(std::memory_order_relaxed)) {
	other.m_buffer = {};
	other.m_size = 0;
	other.m_address = 0;
	other.m_mapped = nullptr;
}

auto gse::vulkan::buffer::operator=(buffer&& other) noexcept -> buffer& {
	if (this != &other) {
		m_buffer = other.m_buffer;
		m_size = other.m_size;
		m_address = other.m_address;
		m_mapped = other.m_mapped;
		m_host_dirty.store(other.m_host_dirty.load(std::memory_order_relaxed), std::memory_order_relaxed);
		other.m_buffer = {};
		other.m_size = 0;
		other.m_address = 0;
		other.m_mapped = nullptr;
	}
	return *this;
}

auto gse::vulkan::buffer::handle() const -> gpu::buffer_handle {
	return m_buffer;
}

auto gse::vulkan::buffer::size_bytes() const -> gpu::device_size {
	return m_size;
}

auto gse::vulkan::buffer::size() const -> gpu::device_size {
	return m_size;
}

auto gse::vulkan::buffer::device_address() const -> gpu::device_address {
	return m_address;
}

auto gse::vulkan::buffer::valid() const -> bool {
	return static_cast<bool>(m_buffer);
}

auto gse::vulkan::buffer::host_write(const void* data, const std::size_t bytes, const std::size_t offset) const -> void {
	assert(m_mapped, "Buffer must be persistently mapped to host_write");
	assert(offset + bytes <= m_size, "host_write extends past buffer size");

	gse::memcpy(m_mapped + offset, data, bytes);
	m_host_dirty.store(true, std::memory_order_release);
}

template <typename T>
requires(!std::is_pointer_v<T>)
auto gse::vulkan::buffer::host_write(const T& src, const std::size_t offset) const -> void {
	if constexpr (std::ranges::contiguous_range<T>) {
		host_write(std::ranges::data(src), std::ranges::size(src) * sizeof(std::ranges::range_value_t<T>), offset);
	}
	else {
		static_assert(std::is_trivially_copyable_v<T>, "host_write requires a trivially copyable type");
		host_write(std::addressof(src), sizeof(T), offset);
	}
}

auto gse::vulkan::buffer::host_zero() const -> void {
	assert(m_mapped, "Buffer must be persistently mapped to host_zero");
	std::memset(m_mapped, 0, m_size);
	m_host_dirty.store(true, std::memory_order_release);
}

auto gse::vulkan::buffer::host_read() const -> std::span<const std::byte> {
	assert(m_mapped, "Buffer must be persistently mapped to host_read");
	return std::span<const std::byte>(m_mapped, m_size);
}

template <typename T>
auto gse::vulkan::buffer::mapped() const -> T* {
	return reinterpret_cast<T*>(m_mapped);
}

auto gse::vulkan::buffer::mark_host_dirty() const noexcept -> void {
	m_host_dirty.store(true, std::memory_order_release);
}

auto gse::vulkan::buffer::host_dirty() const noexcept -> bool {
	return m_host_dirty.load(std::memory_order_acquire);
}

auto gse::vulkan::buffer::clear_host_dirty() const noexcept -> void {
	m_host_dirty.store(false, std::memory_order_release);
}
