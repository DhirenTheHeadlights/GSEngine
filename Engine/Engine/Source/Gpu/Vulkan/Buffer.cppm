export module gse.gpu:vulkan_buffer;

import std;

import :handles;
import :types;
import :vulkan_allocation;

import gse.assert;
import gse.math;
import gse.core;

export namespace gse::vulkan {
	class device;

	template <typename Device>
	class basic_buffer final : public non_copyable {
	public:
		basic_buffer() = default;

		[[nodiscard]] static auto create(
			Device& dev,
			const gpu::buffer_desc& desc
		) -> basic_buffer;

		basic_buffer(
			gpu::handle<basic_buffer<device>> buffer,
			basic_allocation<Device> allocation,
			gpu::device_size size
		);

		~basic_buffer(
		) override;

		basic_buffer(
			basic_buffer&& other
		) noexcept;

		auto operator=(
			basic_buffer&& other
		) noexcept -> basic_buffer&;

		[[nodiscard]] auto handle(
		) const -> gpu::handle<basic_buffer<device>>;

		[[nodiscard]] auto size_bytes(
		) const -> gpu::device_size;

		[[nodiscard]] auto size(
		) const -> gpu::device_size;

		[[nodiscard]] auto bytes(
			this auto&& self
		) -> std::span<std::byte>;

		[[nodiscard]] auto mapped(
		) const -> std::byte*;

		auto upload(
			const void* data,
			std::size_t size
		) -> void;

		explicit operator bool(
		) const;

	private:
		gpu::handle<basic_buffer<device>> m_buffer;
		gpu::device_size m_size = 0;
		basic_allocation<Device> m_allocation;
	};

	using buffer = basic_buffer<device>;
}

template <typename Device>
gse::vulkan::basic_buffer<Device>::basic_buffer(const gpu::handle<basic_buffer<device>> buffer, basic_allocation<Device> allocation, const gpu::device_size size)
	: m_buffer(buffer), m_size(size), m_allocation(std::move(allocation)) {}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::create(Device& dev, const gpu::buffer_desc& desc) -> basic_buffer {
	return dev.create_buffer(
		gpu::buffer_create_info{
			.size = desc.size,
			.usage = desc.usage,
		},
		desc.data
	);
}

template <typename Device>
gse::vulkan::basic_buffer<Device>::~basic_buffer() {
	if (m_allocation.device() && m_buffer) {
		m_allocation.device()->destroy_buffer(m_buffer);
	}
}

template <typename Device>
gse::vulkan::basic_buffer<Device>::basic_buffer(basic_buffer&& other) noexcept
	: m_buffer(other.m_buffer), m_size(other.m_size), m_allocation(std::move(other.m_allocation)) {
	other.m_buffer = {};
	other.m_size = 0;
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::operator=(basic_buffer&& other) noexcept -> basic_buffer& {
	if (this != &other) {
		if (m_allocation.device() && m_buffer) {
			m_allocation.device()->destroy_buffer(m_buffer);
		}

		m_buffer = other.m_buffer;
		m_size = other.m_size;
		m_allocation = std::move(other.m_allocation);
		other.m_buffer = {};
		other.m_size = 0;
	}
	return *this;
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::handle() const -> gpu::handle<basic_buffer<device>> {
	return m_buffer;
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::size_bytes() const -> gpu::device_size {
	return m_size;
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::size() const -> gpu::device_size {
	return m_size;
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::bytes(this auto&& self) -> std::span<std::byte> {
	return std::span(self.m_allocation.mapped(), self.m_size);
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::mapped() const -> std::byte* {
	return m_allocation.mapped();
}

template <typename Device>
gse::vulkan::basic_buffer<Device>::operator bool() const {
	return static_cast<bool>(m_buffer);
}

template <typename Device>
auto gse::vulkan::basic_buffer<Device>::upload(const void* data, const std::size_t bytes) -> void {
	assert(m_allocation.mapped(), std::source_location::current(), "Buffer for uploading must be persistently mapped");
	assert(bytes <= m_size, std::source_location::current(), "Upload size exceeds buffer size");

	gse::memcpy(m_allocation.mapped(), data, bytes);
}
