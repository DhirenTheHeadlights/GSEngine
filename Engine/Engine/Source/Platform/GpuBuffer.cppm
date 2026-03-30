export module gse.platform:gpu_buffer;

import std;

import :gpu_types;
import :vulkan_allocator;

import gse.utility;

export namespace gse::gpu {
	class buffer final : public non_copyable {
	public:
		buffer() = default;
		buffer(vulkan::buffer_resource&& resource);
		buffer(buffer&&) noexcept = default;
		auto operator=(buffer&&) noexcept -> buffer& = default;

		[[nodiscard]] auto mapped(this const buffer& self) -> std::byte* { return self.m_resource.allocation.mapped(); }
		[[nodiscard]] auto size(this const buffer& self) -> std::size_t { return static_cast<std::size_t>(self.m_resource.size); }
		[[nodiscard]] auto native(this auto&& self) -> auto&& { return std::forward_like<decltype(self)>(self.m_resource); }

		explicit operator bool() const;
	private:
		vulkan::buffer_resource m_resource;
	};

	struct descriptor_buffer_binding {
		std::uint32_t binding = 0;
		const buffer* buf = nullptr;
		std::size_t offset = 0;
		std::size_t range = 0;
	};
}

gse::gpu::buffer::buffer(vulkan::buffer_resource&& resource)
	: m_resource(std::move(resource)) {}

gse::gpu::buffer::operator bool() const {
	return m_resource.buffer != nullptr;
}
