export module gse.platform:gpu_descriptor;

import std;

import :gpu_types;
import :gpu_buffer;
import :gpu_pipeline;
import :gpu_image;
import :vulkan_allocator;
import :descriptor_heap;

import gse.utility;

export namespace gse::gpu {
	class descriptor_region final : public non_copyable {
	public:
		descriptor_region() = default;
		descriptor_region(vulkan::descriptor_region&& region);
		descriptor_region(descriptor_region&&) noexcept = default;
		auto operator=(descriptor_region&&) noexcept -> descriptor_region& = default;

		[[nodiscard]] auto native(this auto&& self) -> auto&& { return std::forward_like<decltype(self)>(self.m_region); }

		explicit operator bool() const;
	private:
		vulkan::descriptor_region m_region;
	};
}

gse::gpu::descriptor_region::descriptor_region(vulkan::descriptor_region&& region)
	: m_region(region) {}

gse::gpu::descriptor_region::operator bool() const {
	return m_region;
}
