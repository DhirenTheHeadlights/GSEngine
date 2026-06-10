export module gse.gpu_backend:sync;

import std;

import :core;
import :enums;

export namespace gse::gpu {
	struct queue_progress {
		queue_id queue;
		std::uint64_t reached_value;
	};

	struct frame_token {
		std::uint32_t frame_index = 0;
		std::uint32_t image_index = 0;
	};

	struct memory_barrier {
		pipeline_stage_flags src_stages;
		access_flags src_access;
		pipeline_stage_flags dst_stages;
		access_flags dst_access;
	};

	struct semaphore_submit_info {
		gpu::handle<gpu::semaphore> semaphore;
		std::uint64_t value = 0;
		pipeline_stage_flags stages;
	};

	struct command_buffer_submit_info {
		gpu::command_buffer_handle command_buffer;
	};

	struct submit_info {
		std::span<const semaphore_submit_info> wait_semaphores;
		std::span<const command_buffer_submit_info> command_buffers;
		std::span<const semaphore_submit_info> signal_semaphores;
	};

	struct present_info {
		std::span<const gpu::handle<gpu::semaphore>> wait_semaphores;
		std::span<const gpu::swap_chain_handle> swapchains;
		std::span<const std::uint32_t> image_indices;
		std::span<const present_mode> present_modes;
		std::span<const gpu::handle<gpu::fence>> release_fences;
		std::span<const std::uint64_t> present_ids;
	};
}
