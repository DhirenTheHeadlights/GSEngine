module;

#include <compare>
#include <stdexcept>

module gse.platform.context;

import vulkan_hpp;

auto gse::vulkan::get_next_image(const vk::SwapchainKHR swap_chain) -> std::uint32_t {
	const auto result = get_device().acquireNextImageKHR(swap_chain, std::numeric_limits<std::uint64_t>::max(), g_image_available_semaphore, nullptr);
	perma_assert(result.result != vk::Result::eSuccess, "Failed to acquire next image!");
	return result.value;
}

auto gse::vulkan::find_memory_type(const std::uint32_t type_filter, const vk::MemoryPropertyFlags properties) -> std::uint32_t {
	const auto memory_properties = g_physical_device.getMemoryProperties();
	for (std::uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
		if (type_filter & 1 << i && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	throw std::runtime_error("Failed to find suitable memory type!");
}

auto gse::vulkan::begin_single_line_commands() -> vk::CommandBuffer {
	const vk::CommandBufferAllocateInfo alloc_info(
		g_command_pool, vk::CommandBufferLevel::ePrimary, 1
	);

	const vk::CommandBuffer command_buffer = g_device.allocateCommandBuffers(alloc_info)[0];
	constexpr vk::CommandBufferBeginInfo begin_info(
		vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	);

	command_buffer.begin(begin_info);
	return command_buffer;
}

auto gse::vulkan::end_single_line_commands(const vk::CommandBuffer command_buffer) -> void {
	command_buffer.end();
	const vk::SubmitInfo submit_info(
		0, nullptr, nullptr, 1, &command_buffer, 0, nullptr
	);
	g_graphics_queue.submit(submit_info, nullptr);
	g_graphics_queue.waitIdle();
	g_device.freeCommandBuffers(g_command_pool, command_buffer);
}