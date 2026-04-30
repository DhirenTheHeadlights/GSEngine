export module gse.gpu:aliases;

import :vulkan_buffer;
import :vulkan_image;
import :vulkan_sampler;
import :vulkan_fence;
import :vulkan_semaphore;
import :vulkan_query_pool;
import :vulkan_pipeline_layout;
import :vulkan_pipeline;
import :vulkan_commands;
import :vulkan_descriptor_set_layout;
import :vulkan_shader_module;
import :vulkan_device;

export namespace gse::gpu {
	using buffer = vulkan::buffer;
	using image = vulkan::image;
	using sampler = vulkan::sampler;
	using fence = vulkan::fence;
	using semaphore = vulkan::semaphore;
	using query_pool = vulkan::query_pool;
	using pipeline_layout = vulkan::pipeline_layout;
	using pipeline = vulkan::pipeline;
	using command_buffer = vulkan::command_buffer;
	using image_view = vulkan::image_view;
	using descriptor_set_layout = vulkan::descriptor_set_layout;
	using shader_module = vulkan::shader_module;
}
