export module gse.gpu:aliases;

import std;

export import gse.vulkan;

export namespace gse::gpu {
	using blas = vulkan::blas;
	using tlas = vulkan::tlas;
	using as_instance = vulkan::as_instance;
	using buffer = vulkan::buffer;
	using image = vulkan::image;
	using sampler = vulkan::sampler;
	using fence = vulkan::fence;
	using semaphore = vulkan::semaphore;
	using sync = vulkan::sync;
	using device_memory_handle = vulkan::device_memory_handle;
	using memory_requirements = vulkan::memory_requirements;
	using query_pool = vulkan::query_pool;
	using pipeline_layout = vulkan::pipeline_layout;
	using command_buffer = vulkan::command_buffer;
	using transient_command_buffer = vulkan::transient_command_buffer;
	using commands = vulkan::commands;
	using pipeline_state_cache = vulkan::pipeline_state_cache;
	constexpr std::uint32_t max_frames_in_flight = vulkan::max_frames_in_flight;
	using image_view = vulkan::image_view;
	using descriptor_set_layout = vulkan::descriptor_set_layout;
	using shader_object = vulkan::shader_object;
	using shader_program = vulkan::shader_program;
	using video_encoder = vulkan::video_encoder;
	using video_codec = vulkan::video_codec;
	using encode_capabilities = vulkan::encode_capabilities;
	using encoded_unit = vulkan::encoded_unit;
	using bindless_heaps = vulkan::bindless_heaps;
	using bindless_slot = vulkan::bindless_slot;
	using bindless_resource_heap = vulkan::bindless_resource_heap;
	using bindless_sampler_heap = vulkan::bindless_sampler_heap;
	using bindless_image = vulkan::bindless_image;
	using bindless_image_view = vulkan::bindless_image_view;
	using bindless_buffer = vulkan::bindless_buffer;
	using bindless_buffer_view = vulkan::bindless_buffer_view;
	using bindless_sampler = vulkan::bindless_sampler;
	using bindless_tlas_view = vulkan::bindless_tlas_view;
	using device_settings = vulkan::device::settings;
	using queue_id = vulkan::queue_id;
	using queue_progress = vulkan::queue_progress;
	constexpr std::size_t queue_id_count = vulkan::queue_id_count;
	using frame_resource_bin = vulkan::frame_resource_bin;
	using frame_record_fn = vulkan::frame_record_fn;
	using frame_recorder = vulkan::frame_recorder;
	using transient_queue = vulkan::transient_queue;
	using transient_executor = vulkan::transient_executor;
	using device_lost_error = vulkan::device_lost_error;
	using out_of_date_error = vulkan::out_of_date_error;
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
