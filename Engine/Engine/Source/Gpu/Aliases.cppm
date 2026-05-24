export module gse.gpu:aliases;

import :vulkan_acceleration_structure;
import :vulkan_buffer;
import :vulkan_image;
import :vulkan_sampler;
import :vulkan_fence;
import :vulkan_semaphore;
import :vulkan_query_pool;
import :vulkan_pipeline_layout;
import :vulkan_commands;
import :vulkan_descriptor_set_layout;
import :vulkan_shader_object;
import :vulkan_shader_program;
import :vulkan_device;
import :vulkan_sync;
import :types;

export namespace gse::gpu {
	using blas = vulkan::blas;
	using tlas = vulkan::tlas;
	using buffer = vulkan::buffer;
	using image = vulkan::image;
	using sampler = vulkan::sampler;
	using fence = vulkan::fence;
	using semaphore = vulkan::semaphore;
	using query_pool = vulkan::query_pool;
	using pipeline_layout = vulkan::pipeline_layout;
	using command_buffer = vulkan::command_buffer;
	using commands = vulkan::commands;
	constexpr std::uint32_t max_frames_in_flight = vulkan::max_frames_in_flight;
	using image_view = vulkan::image_view;
	using descriptor_set_layout = vulkan::descriptor_set_layout;
	using shader_object = vulkan::shader_object;
	using shader_program = vulkan::shader_program;
	constexpr auto present_mode_from_setting_index(const int index) -> present_mode {
		switch (index) {
			case 0:
				return present_mode::fifo;
			case 1:
				return present_mode::fifo_relaxed;
			case 2:
				return present_mode::mailbox;
			case 3:
				return present_mode::immediate;
			default:
				return present_mode::fifo;
		}
	}
}
