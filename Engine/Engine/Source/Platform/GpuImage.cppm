export module gse.platform:gpu_image;

import std;

import :gpu_types;
import :vulkan_allocator;

import gse.utility;
import gse.math;

export namespace gse::gpu {
	class image final : public non_copyable {
	public:
		image() = default;
		image(vulkan::image_resource&& resource, image_format fmt, vec2u extent);
		image(image&&) noexcept = default;
		auto operator=(image&&) noexcept -> image& = default;

		[[nodiscard]] auto extent(this const image& self) -> vec2u { return self.m_extent; }
		[[nodiscard]] auto format(this const image& self) -> image_format { return self.m_format; }
		[[nodiscard]] auto layout(this const image& self) -> image_layout;
		auto set_layout(image_layout l) -> void;

		[[nodiscard]] auto native(this auto&& self) -> auto&& { return std::forward_like<decltype(self)>(self.m_resource); }

		explicit operator bool() const;
	private:
		vulkan::image_resource m_resource;
		image_format m_format = image_format::d32_sfloat;
		vec2u m_extent = { 1, 1 };
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

	auto to_vk_layout(gse::gpu::image_layout l) -> vk::ImageLayout {
		switch (l) {
			case gse::gpu::image_layout::general:          return vk::ImageLayout::eGeneral;
			case gse::gpu::image_layout::shader_read_only: return vk::ImageLayout::eShaderReadOnlyOptimal;
			default:                                       return vk::ImageLayout::eUndefined;
		}
	}
}

gse::gpu::image::image(vulkan::image_resource&& resource, image_format fmt, vec2u extent)
	: m_resource(std::move(resource)), m_format(fmt), m_extent(extent) {}

auto gse::gpu::image::layout(this const image& self) -> image_layout {
	return from_vk_layout(self.m_resource.current_layout);
}

auto gse::gpu::image::set_layout(image_layout l) -> void {
	m_resource.current_layout = to_vk_layout(l);
}

gse::gpu::image::operator bool() const {
	return m_resource.image != nullptr;
}
