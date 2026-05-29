export module gse.vulkan:image;

import std;

import :types;
import :handles;

import gse.core;
import gse.math;

export namespace gse::vulkan {
	class image final : public non_copyable {
	public:
		image() = default;

		~image() override = default;

		image(
			gpu::image_handle image,
			gpu::image_view_handle view,
			gpu::image_format_value format,
			vec3u extent,
			gpu::image_view_create_info view_info
		);

		image(
			image&&
		) noexcept = default;

		auto operator=(
			image&&
		) noexcept -> image& = default;

		[[nodiscard]] auto handle() const -> gpu::image_handle;

		[[nodiscard]] auto view() const -> gpu::image_view_handle;

		[[nodiscard]] auto format() const -> gpu::image_format_value;

		[[nodiscard]] auto extent() const -> vec3u;

		[[nodiscard]] auto view_create_info() const -> const gpu::image_view_create_info&;

		[[nodiscard]] auto valid() const -> bool;

	private:
		gpu::image_handle m_image;
		gpu::image_view_handle m_view;
		gpu::image_format_value m_format = 0;
		vec3u m_extent;
		gpu::image_view_create_info m_view_info;
	};
}

gse::vulkan::image::image(const gpu::image_handle image, const gpu::image_view_handle view, const gpu::image_format_value format, const vec3u extent, gpu::image_view_create_info view_info)
	: m_image(image), m_view(view), m_format(format), m_extent(extent), m_view_info(view_info) {
}

auto gse::vulkan::image::handle() const -> gpu::image_handle {
	return m_image;
}

auto gse::vulkan::image::view() const -> gpu::image_view_handle {
	return m_view;
}

auto gse::vulkan::image::format() const -> gpu::image_format_value {
	return m_format;
}

auto gse::vulkan::image::extent() const -> vec3u {
	return m_extent;
}

auto gse::vulkan::image::view_create_info() const -> const gpu::image_view_create_info& {
	return m_view_info;
}

auto gse::vulkan::image::valid() const -> bool {
	return static_cast<bool>(m_image);
}
