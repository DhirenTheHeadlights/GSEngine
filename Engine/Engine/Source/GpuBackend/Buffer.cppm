export module gse.gpu_backend:buffer;

import std;

import :core;

import gse.assert;
import gse.math;
import gse.core;

export namespace gse::gpu {
	enum class buffer_flag : std::uint32_t {
		uniform = 0x01,
		storage = 0x02,
		indirect = 0x04,
		transfer_dst = 0x08,
		byte_address = 0x10,
		index = 0x20,
		transfer_src = 0x40,
		acceleration_structure_storage = 0x80,
		acceleration_structure_build_input = 0x100,
		video_encode_dst = 0x200,
		acceleration_structure_scratch = 0x400,
	};

	using buffer_usage = gse::flags<buffer_flag>;

	struct buffer_desc {
		device_size size = 0;
		device_size stride = 0;
		buffer_usage usage = buffer_flag::storage;
		const void* data = nullptr;
		const void* pnext = nullptr;
		bool bindless = false;
		bool writable = false;
		bool readback = false;
	};

	struct buffer_copy_region {
		device_size src_offset = 0;
		device_size dst_offset = 0;
		device_size size = 0;
	};

	class buffer final : public non_copyable {
	public:
		buffer() {}
		~buffer() = default;

		buffer(
			gpu::handle<buffer> buffer,
			gpu::device_size size,
			gpu::device_address address,
			std::byte* mapped,
			gpu::bindless_slot slot = {}
		);

		buffer(
			buffer&& other
		) noexcept;

		auto operator=(
			buffer&& other
		) noexcept -> buffer&;

		[[nodiscard]] auto handle() const -> gpu::handle<buffer>;

		[[nodiscard]] auto size_bytes() const -> gpu::device_size;

		[[nodiscard]] auto size() const -> gpu::device_size;

		[[nodiscard]] auto device_address() const -> gpu::device_address;

		[[nodiscard]] auto slot() const -> gpu::bindless_slot;

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
		gpu::handle<buffer> m_buffer;
		gpu::device_size m_size = 0;
		gpu::device_address m_address = 0;
		std::byte* m_mapped = nullptr;
		gpu::bindless_slot m_slot;
		mutable std::atomic<bool> m_host_dirty{ false };
	};
}

gse::gpu::buffer::buffer(const gpu::handle<buffer> buffer, const gpu::device_size size, const gpu::device_address address, std::byte* mapped, const gpu::bindless_slot slot)
	: m_buffer(buffer), m_size(size), m_address(address), m_mapped(mapped), m_slot(slot) {
}

gse::gpu::buffer::buffer(buffer&& other) noexcept
	: m_buffer(other.m_buffer), m_size(other.m_size), m_address(other.m_address), m_mapped(other.m_mapped), m_slot(other.m_slot), m_host_dirty(other.m_host_dirty.load(std::memory_order_relaxed)) {
	other.m_buffer = {};
	other.m_size = 0;
	other.m_address = 0;
	other.m_mapped = nullptr;
	other.m_slot = {};
}

auto gse::gpu::buffer::operator=(buffer&& other) noexcept -> buffer& {
	if (this != &other) {
		m_buffer = other.m_buffer;
		m_size = other.m_size;
		m_address = other.m_address;
		m_mapped = other.m_mapped;
		m_slot = other.m_slot;
		m_host_dirty.store(other.m_host_dirty.load(std::memory_order_relaxed), std::memory_order_relaxed);
		other.m_buffer = {};
		other.m_size = 0;
		other.m_address = 0;
		other.m_mapped = nullptr;
		other.m_slot = {};
	}
	return *this;
}

auto gse::gpu::buffer::handle() const -> gpu::handle<buffer> {
	return m_buffer;
}

auto gse::gpu::buffer::size_bytes() const -> gpu::device_size {
	return m_size;
}

auto gse::gpu::buffer::size() const -> gpu::device_size {
	return m_size;
}

auto gse::gpu::buffer::device_address() const -> gpu::device_address {
	return m_address;
}

auto gse::gpu::buffer::slot() const -> gpu::bindless_slot {
	return m_slot;
}

auto gse::gpu::buffer::valid() const -> bool {
	return static_cast<bool>(m_buffer);
}

auto gse::gpu::buffer::host_write(const void* data, const std::size_t bytes, const std::size_t offset) const -> void {
	assert(m_mapped, "Buffer must be persistently mapped to host_write");
	assert(offset + bytes <= m_size, "host_write extends past buffer size");

	gse::memcpy(m_mapped + offset, data, bytes);
	m_host_dirty.store(true, std::memory_order_release);
}

template <typename T>
requires(!std::is_pointer_v<T>)
auto gse::gpu::buffer::host_write(const T& src, const std::size_t offset) const -> void {
	if constexpr (std::ranges::contiguous_range<T>) {
		host_write(std::ranges::data(src), std::ranges::size(src) * sizeof(std::ranges::range_value_t<T>), offset);
	}
	else {
		static_assert(std::is_trivially_copyable_v<T>, "host_write requires a trivially copyable type");
		host_write(std::addressof(src), sizeof(T), offset);
	}
}

auto gse::gpu::buffer::host_zero() const -> void {
	assert(m_mapped, "Buffer must be persistently mapped to host_zero");
	std::memset(m_mapped, 0, m_size);
	m_host_dirty.store(true, std::memory_order_release);
}

auto gse::gpu::buffer::host_read() const -> std::span<const std::byte> {
	assert(m_mapped, "Buffer must be persistently mapped to host_read");
	return std::span<const std::byte>(m_mapped, m_size);
}

template <typename T>
auto gse::gpu::buffer::mapped() const -> T* {
	return reinterpret_cast<T*>(m_mapped);
}

auto gse::gpu::buffer::mark_host_dirty() const noexcept -> void {
	m_host_dirty.store(true, std::memory_order_release);
}

auto gse::gpu::buffer::host_dirty() const noexcept -> bool {
	return m_host_dirty.load(std::memory_order_acquire);
}

auto gse::gpu::buffer::clear_host_dirty() const noexcept -> void {
	m_host_dirty.store(false, std::memory_order_release);
}
