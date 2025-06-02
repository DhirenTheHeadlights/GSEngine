export module gse.platform.vulkan.persistent_allocator;

import std;
import vulkan_hpp;

import gse.platform.vulkan.config;
import gse.platform.assert;
import gse.platform.vulkan.resources;

export namespace gse::vulkan::persistent_allocator {
	template <typename T>
	struct mapped_buffer_view {
		void* base = nullptr;
		vk::DeviceSize stride = sizeof(T);

		auto operator[](const size_t index) -> T& {
			assert(base != nullptr, "mapped_buffer_view base is null");
			return *reinterpret_cast<T*>(static_cast<std::byte*>(base) + index * stride);
		}

		auto get_offset(const size_t index) const -> vk::DeviceSize {
			return index * stride;
		}

		auto get_span(const size_t index) const -> std::span<const std::byte> {
			const auto ptr = static_cast<const std::byte*>(base) + index * stride;
			return std::span{ ptr, stride };
		}
			
		auto get_span(const size_t index) -> std::span<std::byte> {
			const auto ptr = static_cast<std::byte*>(base) + index * stride;
			return std::span{ ptr, stride };
		}
	};

	auto allocate(config::device_config config, const vk::MemoryRequirements& requirements, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal) -> allocation;
	auto bind(config::device_config config, vk::Buffer buffer, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal, vk::MemoryRequirements requirements = {}) -> allocation;
	auto bind(config::device_config config, vk::Image image, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal, vk::MemoryRequirements requirements = {}) -> allocation;
	auto create_buffer(config::device_config config, const vk::BufferCreateInfo& buffer_info, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal, const void* data = nullptr) -> buffer_resource;
	auto create_image(config::device_config config, const vk::ImageCreateInfo& info, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageViewCreateInfo view_info = {}, const void* data = nullptr) -> image_resource;
	auto free(allocation& alloc) -> void;
	auto free(config::device_config config, buffer_resource& resource) -> void;
	auto free(config::device_config config, image_resource& resource) -> void;
	auto clean_up(vk::Device device) -> void;
}

namespace gse::vulkan::persistent_allocator {
	auto get_memory_flag_preferences(vk::BufferUsageFlags usage) -> std::vector<vk::MemoryPropertyFlags>;
}