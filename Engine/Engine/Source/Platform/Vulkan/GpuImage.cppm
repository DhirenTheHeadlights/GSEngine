export module gse.platform:gpu_image;

import std;

import :gpu_types;
import :vulkan_allocator;
import :vulkan_enums;

import gse.utility;
import gse.math;

export namespace gse::gpu {
	class image_view {
	public:
		image_view() = default;
		explicit image_view(vk::ImageView handle) : m_handle(handle) {}

		[[nodiscard]] auto native() const -> vk::ImageView { return m_handle; }
		explicit operator bool() const { return static_cast<bool>(m_handle); }
	private:
		vk::ImageView m_handle{};
	};

	class image final : public non_copyable {
	public:
		image() = default;
		image(vulkan::image_resource&& resource, image_format fmt, vec2u extent, std::vector<vk::raii::ImageView>&& layer_views = {});
		image(image&&) noexcept = default;
		auto operator=(image&&) noexcept -> image& = default;

		[[nodiscard]] auto extent(this const image& self) -> vec2u { return self.m_extent; }
		[[nodiscard]] auto format(this const image& self) -> image_format { return self.m_format; }
		[[nodiscard]] auto layout(this const image& self) -> image_layout;
		auto set_layout(image_layout l) -> void;

		[[nodiscard]] auto native(this auto&& self) -> auto&& { return std::forward_like<decltype(self)>(self.m_resource); }
		[[nodiscard]] auto view(this const image& self) -> image_view { return image_view(self.m_resource.view); }
		[[nodiscard]] auto layer_view(std::uint32_t layer) const -> image_view;

		explicit operator bool() const;
	private:
		vulkan::image_resource m_resource;
		image_format m_format = image_format::d32_sfloat;
		vec2u m_extent = { 1, 1 };
		std::vector<vk::raii::ImageView> m_layer_views;
	};
}

namespace {
	auto from_vk_layout(vk::ImageLayout l) -> gse::gpu::image_layout {
		switch (l) {
			case vk::ImageLayout::eGeneral:              return gse::gpu::image_layout::general;
			case vk::ImageLayout::eShaderReadOnlyOptimal: return gse::gpu::image_layout::shader_read_only;
			default:                                     return gse::gpu::image_layout::undefined;
		}
	}
}

gse::gpu::image::image(vulkan::image_resource&& resource, image_format fmt, vec2u extent, std::vector<vk::raii::ImageView>&& layer_views)
	: m_resource(std::move(resource)), m_format(fmt), m_extent(extent), m_layer_views(std::move(layer_views)) {}

auto gse::gpu::image::layer_view(const std::uint32_t layer) const -> image_view {
	return image_view(*m_layer_views[layer]);
}

auto gse::gpu::image::layout(this const image& self) -> image_layout {
	return from_vk_layout(self.m_resource.current_layout);
}

auto gse::gpu::image::set_layout(image_layout l) -> void {
	m_resource.current_layout = vulkan::to_vk(l);
}

gse::gpu::image::operator bool() const {
	return m_resource.image != nullptr;
}
