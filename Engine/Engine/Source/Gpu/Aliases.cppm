export module gse.gpu:aliases;

import std;

export import gse.gpu_backend;
import gse.vulkan;

export namespace gse::gpu {
	using as_instance = vulkan::as_instance;
	using sync = vulkan::sync;
	using command_buffer = vulkan::command_buffer;
	using transient_command_buffer = vulkan::transient_command_buffer;
	using commands = vulkan::commands;
	using pipeline_state_cache = vulkan::pipeline_state_cache;
	using shader_object_create_info = vulkan::shader_object_create_info;
	using shader_program_create_info = vulkan::shader_program_create_info;
	using video_encoder = vulkan::video_encoder;
	using video_codec = vulkan::video_codec;
	using encode_capabilities = vulkan::encode_capabilities;
	using encoded_unit = vulkan::encoded_unit;
	using device_settings = vulkan::device::settings;
	using transient_queue = vulkan::transient_queue;
	using transient_executor = vulkan::transient_executor;
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
