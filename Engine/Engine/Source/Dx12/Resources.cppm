export module gse.dx12:resources;

import std;

import gse.gpu_types;
import gse.core;
import gse.math;

export namespace gse::dx12 {
	struct image_view {};

	class buffer final : public non_copyable {
	public:
		buffer() = default;

		~buffer() override = default;

		buffer(
			buffer&&
		) noexcept = default;

		auto operator=(
			buffer&&
		) noexcept -> buffer& = default;

		[[nodiscard]] auto handle() const -> gpu::handle<buffer>;

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
		gpu::handle<buffer> m_handle;
		gpu::device_size m_size = 0;
	};

	class image final : public non_copyable {
	public:
		image() = default;

		~image() override = default;

		image(
			image&&
		) noexcept = default;

		auto operator=(
			image&&
		) noexcept -> image& = default;

		[[nodiscard]] auto handle() const -> gpu::handle<image>;

		[[nodiscard]] auto view() const -> gpu::handle<image_view>;

		[[nodiscard]] auto format() const -> gpu::image_format_value;

		[[nodiscard]] auto extent() const -> vec3u;

		[[nodiscard]] auto view_create_info() const -> const gpu::image_view_create_info&;

		[[nodiscard]] auto valid() const -> bool;

	private:
		gpu::handle<image> m_handle;
		gpu::handle<image_view> m_view;
		gpu::image_format_value m_format = 0;
		vec3u m_extent;
		gpu::image_view_create_info m_view_info;
	};

	class sampler final : public non_copyable {
	public:
		sampler() = default;

		~sampler() override = default;

		sampler(
			sampler&&
		) noexcept = default;

		auto operator=(
			sampler&&
		) noexcept -> sampler& = default;

		[[nodiscard]] auto native(
			this const sampler& self
		) -> gpu::handle<sampler>;

		[[nodiscard]] auto valid() const -> bool;

	private:
		gpu::handle<sampler> m_handle;
	};
}

auto gse::dx12::buffer::handle() const -> gpu::handle<buffer> {
	return m_handle;
}

auto gse::dx12::buffer::size_bytes() const -> gpu::device_size {
	return m_size;
}

auto gse::dx12::buffer::size() const -> gpu::device_size {
	return m_size;
}

auto gse::dx12::buffer::device_address() const -> gpu::device_address {
	return 0;
}

auto gse::dx12::buffer::host_write(const void*, const std::size_t, const std::size_t) const -> void {
}

template <typename T>
requires(!std::is_pointer_v<T>)
auto gse::dx12::buffer::host_write(const T&, const std::size_t) const -> void {
}

auto gse::dx12::buffer::host_zero() const -> void {
}

auto gse::dx12::buffer::host_read() const -> std::span<const std::byte> {
	return {};
}

template <typename T>
auto gse::dx12::buffer::mapped() const -> T* {
	return nullptr;
}

auto gse::dx12::buffer::mark_host_dirty() const noexcept -> void {
}

auto gse::dx12::buffer::host_dirty() const noexcept -> bool {
	return false;
}

auto gse::dx12::buffer::clear_host_dirty() const noexcept -> void {
}

auto gse::dx12::buffer::valid() const -> bool {
	return false;
}

auto gse::dx12::image::handle() const -> gpu::handle<image> {
	return m_handle;
}

auto gse::dx12::image::view() const -> gpu::handle<image_view> {
	return m_view;
}

auto gse::dx12::image::format() const -> gpu::image_format_value {
	return m_format;
}

auto gse::dx12::image::extent() const -> vec3u {
	return m_extent;
}

auto gse::dx12::image::view_create_info() const -> const gpu::image_view_create_info& {
	return m_view_info;
}

auto gse::dx12::image::valid() const -> bool {
	return false;
}

auto gse::dx12::sampler::native(this const sampler& self) -> gpu::handle<sampler> {
	return self.m_handle;
}

auto gse::dx12::sampler::valid() const -> bool {
	return false;
}
