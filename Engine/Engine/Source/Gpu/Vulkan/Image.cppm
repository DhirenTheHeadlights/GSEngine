export module gse.gpu:vulkan_image;

import std;

import :types;
import :handles;
import :vulkan_allocation;

import gse.assert;
import gse.core;
import gse.math;

export namespace gse::vulkan {
	class device;
	struct image_view;

	template <typename Device>
	class basic_image final : public non_copyable {
	public:
		basic_image() = default;

		[[nodiscard]] static auto create(
			Device& dev,
			const gpu::image_desc& desc
		) -> basic_image;

		basic_image(
			gpu::handle<basic_image<device>> image,
			gpu::handle<image_view> view,
			gpu::image_format_value format,
			gpu::image_layout current_layout,
			vec3u extent,
			basic_allocation<Device> allocation
		);

		~basic_image(
		) override;

		basic_image(
			basic_image&& other
		) noexcept;

		auto operator=(
			basic_image&& other
		) noexcept -> basic_image&;

		[[nodiscard]] auto handle(
		) const -> gpu::handle<basic_image<device>>;

		[[nodiscard]] auto view(
		) const -> gpu::handle<image_view>;

		[[nodiscard]] auto format(
		) const -> gpu::image_format_value;

		[[nodiscard]] auto layout(
		) const -> gpu::image_layout;

		[[nodiscard]] auto extent(
		) const -> vec3u;

		auto set_layout(
			gpu::image_layout new_layout
		) -> void;

	private:
		gpu::handle<basic_image<device>> m_image;
		gpu::handle<image_view> m_view;
		gpu::image_format_value m_format = 0;
		gpu::image_layout m_current_layout = gpu::image_layout::undefined;
		vec3u m_extent;
		basic_allocation<Device> m_allocation;
	};

	using image = basic_image<device>;
}

template <typename Device>
gse::vulkan::basic_image<Device>::basic_image(const gpu::handle<basic_image<device>> image, const gpu::handle<image_view> view, const gpu::image_format_value format, const gpu::image_layout current_layout, const vec3u extent, basic_allocation<Device> allocation)
	: m_image(image), m_view(view), m_format(format), m_current_layout(current_layout), m_extent(extent), m_allocation(std::move(allocation)) {}

template <typename Device>
auto gse::vulkan::basic_image<Device>::create(Device& dev, const gpu::image_desc& desc) -> basic_image {
	const bool is_depth = desc.format == gpu::image_format::d32_sfloat;
	const bool is_cube = desc.view == gpu::image_view_type::cube;
	const std::uint32_t layers = is_cube ? 6u : 1u;

	const gpu::image_create_info create_info{
		.flags = is_cube ? gpu::image_create_flags{ gpu::image_create_flag::cube_compatible } : gpu::image_create_flags{},
		.type = gpu::image_type::e2d,
		.format = desc.format,
		.extent = vec3u{ desc.size.x(), desc.size.y(), 1 },
		.mip_levels = 1,
		.array_layers = layers,
		.samples = gpu::sample_count::e1,
		.usage = desc.usage,
	};
	const gpu::image_view_create_info view_info{
		.format = desc.format,
		.view_type = is_cube ? gpu::image_view_type::cube : gpu::image_view_type::e2d,
		.aspects = is_depth ? gpu::image_aspect_flags{ gpu::image_aspect_flag::depth } : gpu::image_aspect_flags{ gpu::image_aspect_flag::color },
		.base_mip_level = 0,
		.level_count = 1,
		.base_array_layer = 0,
		.layer_count = layers,
	};

	return dev.create_image(create_info, gpu::memory_property_flag::device_local, view_info);
}

template <typename Device>
gse::vulkan::basic_image<Device>::~basic_image() {
	if (m_allocation.device()) {
		if (m_view) {
			m_allocation.device()->destroy_image_view(m_view);
		}
		if (m_image) {
			m_allocation.device()->destroy_image(m_image);
		}
	}
}

template <typename Device>
gse::vulkan::basic_image<Device>::basic_image(basic_image&& other) noexcept
	: m_image(other.m_image), m_view(other.m_view), m_format(other.m_format), m_current_layout(other.m_current_layout), m_extent(other.m_extent), m_allocation(std::move(other.m_allocation)) {
	other.m_image = {};
	other.m_view = {};
}

template <typename Device>
auto gse::vulkan::basic_image<Device>::operator=(basic_image&& other) noexcept -> basic_image& {
	if (this != &other) {
		if (m_allocation.device()) {
			if (m_view) {
				m_allocation.device()->destroy_image_view(m_view);
			}
			if (m_image) {
				m_allocation.device()->destroy_image(m_image);
			}
		}

		m_image = other.m_image;
		m_view = other.m_view;
		m_format = other.m_format;
		m_current_layout = other.m_current_layout;
		m_extent = other.m_extent;
		m_allocation = std::move(other.m_allocation);
		other.m_image = {};
		other.m_view = {};
	}
	return *this;
}

template <typename Device>
auto gse::vulkan::basic_image<Device>::handle() const -> gpu::handle<basic_image<device>> {
	return m_image;
}

template <typename Device>
auto gse::vulkan::basic_image<Device>::view() const -> gpu::handle<image_view> {
	return m_view;
}

template <typename Device>
auto gse::vulkan::basic_image<Device>::format() const -> gpu::image_format_value {
	return m_format;
}

template <typename Device>
auto gse::vulkan::basic_image<Device>::layout() const -> gpu::image_layout {
	return m_current_layout;
}

template <typename Device>
auto gse::vulkan::basic_image<Device>::extent() const -> vec3u {
	return m_extent;
}

template <typename Device>
auto gse::vulkan::basic_image<Device>::set_layout(const gpu::image_layout new_layout) -> void {
	m_current_layout = new_layout;
}
