module;

#include <compare>
#include <stdexcept>

module gse.platform.context;

import vulkan_hpp;

auto gse::vulkan::initialize(GLFWwindow* window) -> void {
	create_instance(window);
	select_gpu();
	create_logical_device(g_instance_config.surface);
	create_sync_objects();
	create_swap_chain(window);
}

auto gse::vulkan::get_queue_families() -> queue_family {
	return find_queue_families(g_device_config.physical_device, g_instance_config.surface);
}

auto gse::vulkan::get_next_image() -> std::uint32_t {
	const auto result = g_device_config.device.acquireNextImageKHR(g_swap_chain_config.swap_chain, std::numeric_limits<std::uint64_t>::max(), g_sync_objects.image_available_semaphore, nullptr);
	perma_assert(result.result != vk::Result::eSuccess, "Failed to acquire next image!");
	return result.value;
}

auto gse::vulkan::get_swap_chain_config() -> swap_chain_config& {
	return g_swap_chain_config;
}

auto gse::vulkan::get_instance_config() -> instance_config& {
	return g_instance_config;
}

auto gse::vulkan::get_device_config() -> device_config& {
	return g_device_config;
}

auto gse::vulkan::get_queue_config() -> queue_config& {
	return g_queue_config;
}

auto gse::vulkan::get_command_config() -> command_config& {
	return g_command_config;
}

auto gse::vulkan::get_descriptor_config() -> descriptor_config& {
	return g_descriptor_config;
}

auto gse::vulkan::get_sync_objects() -> sync_objects& {
	return g_sync_objects;
}

auto gse::vulkan::find_memory_type(const std::uint32_t type_filter, const vk::MemoryPropertyFlags properties) -> std::uint32_t {
	const auto memory_properties = g_device_config.physical_device.getMemoryProperties();
	for (std::uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
		if (type_filter & 1 << i && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	throw std::runtime_error("Failed to find suitable memory type!");
}

auto gse::vulkan::begin_single_line_commands() -> vk::CommandBuffer {
	const vk::CommandBufferAllocateInfo alloc_info(
		g_command_config.command_pool, vk::CommandBufferLevel::ePrimary, 1
	);

	const vk::CommandBuffer command_buffer = g_device_config.device.allocateCommandBuffers(alloc_info)[0];
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
	const auto graphics_queue = g_queue_config.graphics_queue;
	graphics_queue.submit(submit_info, nullptr);
	graphics_queue.waitIdle();
	g_device_config.device.freeCommandBuffers(g_command_config.command_pool, command_buffer);
}