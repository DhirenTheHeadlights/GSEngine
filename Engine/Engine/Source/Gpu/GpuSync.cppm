export module gse.gpu.device:gpu_sync;

import std;
import vulkan;

export namespace gse::gpu {
	struct compute_semaphore_state {
		const vk::raii::Semaphore* semaphore = nullptr;
		std::uint64_t value = 0;
		vk::PipelineStageFlags2 dst_stage = vk::PipelineStageFlagBits2::eAllCommands;

		[[nodiscard]] auto has_signaled(
		) const -> bool;
	};
}

auto gse::gpu::compute_semaphore_state::has_signaled() const -> bool {
	if (semaphore == nullptr || value == 0) {
		return false;
	}
	return semaphore->getCounterValue() >= value;
}
