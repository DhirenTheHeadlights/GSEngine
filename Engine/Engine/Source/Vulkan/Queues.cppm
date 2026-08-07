export module gse.vulkan:queues;

import std;
import vulkan;

import gse.gpu_backend;
import :types;
import :commands;
import :physical_device;

import gse.core;
import gse.log;
import gse.assert;
import gse.math;

export namespace gse::vulkan {
	struct queue_family {
		std::optional<std::uint32_t> graphics_family;
		std::optional<std::uint32_t> present_family;
		std::optional<std::uint32_t> compute_family;
		std::optional<std::uint32_t> video_encode_family;

		[[nodiscard]] auto complete() const -> bool;
	};

	class queue : public non_copyable {
	public:
		queue(
			gpu::handle<gpu::queue> graphics,
			gpu::handle<gpu::queue> present,
			gpu::handle<gpu::queue> compute,
			std::uint32_t graphics_family,
			std::uint32_t compute_family,
			gpu::handle<gpu::queue> video_encode = {},
			std::optional<std::uint32_t> video_encode_family = std::nullopt
		);

		~queue() = default;

		queue(
			queue&&
		) noexcept = default;

		auto operator=(
			queue&&
		) noexcept -> queue& = default;

		[[nodiscard]] auto has_video_encode() const -> bool;

		[[nodiscard]] auto graphics_family_index() const -> std::uint32_t;

		[[nodiscard]] auto compute_family_index() const -> std::uint32_t;

		[[nodiscard]] auto video_encode_family_index() const -> std::optional<std::uint32_t>;

		auto submit(
			gpu::queue_type queue,
			const gpu::submit_info& info,
			gpu::handle<gpu::fence> signal_fence = {}
		) -> gpu::result;

		auto submit_graphics(
			const gpu::submit_info& info,
			gpu::handle<gpu::fence> signal_fence = {}
		) -> gpu::result;

		auto submit_compute(
			const gpu::submit_info& info,
			gpu::handle<gpu::fence> signal_fence = {}
		) -> gpu::result;

		auto submit_video_encode(
			const gpu::submit_info& info,
			gpu::handle<gpu::fence> signal_fence = {}
		) -> void;

		[[nodiscard]] auto present(
			const gpu::present_info& info
		) -> gpu::result;

	private:
		gpu::handle<gpu::queue> m_graphics;
		gpu::handle<gpu::queue> m_present;
		gpu::handle<gpu::queue> m_compute;
		std::uint32_t m_graphics_family_index = 0;
		std::uint32_t m_compute_family_index = 0;
		std::unique_ptr<std::recursive_mutex> m_mutex;

		gpu::handle<gpu::queue> m_video_encode;
		std::optional<std::uint32_t> m_video_encode_family_index;
	};
}

namespace gse::vulkan {
	[[nodiscard]]
	auto find_queue_families(
		const physical_device& device,
		gpu::surface surface
	) -> queue_family;

	struct submit_scratch {
		std::vector<vk::SemaphoreSubmitInfo> waits;
		std::vector<vk::SemaphoreSubmitInfo> signals;
		std::vector<vk::CommandBufferSubmitInfo> cmds;
	};

	auto build_vk_submit_info(
		const gpu::submit_info& info,
		submit_scratch& scratch
	) -> vk::SubmitInfo2;

	struct present_scratch {
		std::vector<vk::Semaphore> waits;
		std::vector<vk::SwapchainKHR> swapchains;
		std::vector<vk::PresentModeKHR> present_modes;
		std::vector<vk::Fence> release_fences;
		std::vector<vk::PresentTimingInfoEXT> timing_infos;
		std::optional<vk::SwapchainPresentModeInfoEXT> mode_info;
		std::optional<vk::SwapchainPresentFenceInfoEXT> fence_info;
		std::optional<vk::PresentId2KHR> present_id_info;
		std::optional<vk::PresentTimingsInfoEXT> timing_info;
	};

	auto build_vk_present_info(
		const gpu::present_info& info,
		present_scratch& scratch
	) -> vk::PresentInfoKHR;
}

auto gse::vulkan::build_vk_submit_info(const gpu::submit_info& info, submit_scratch& scratch) -> vk::SubmitInfo2 {
	scratch.waits.reserve(info.wait_semaphores.size());
	for (const auto& w : info.wait_semaphores) {
		scratch.waits.push_back(
			vk::SemaphoreSubmitInfo{
				.semaphore = std::bit_cast<vk::Semaphore>(w.semaphore),
				.value = w.value,
				.stageMask = to_vk(w.stages),
				.deviceIndex = 0,
			}
		);
	}
	scratch.signals.reserve(info.signal_semaphores.size());
	for (const auto& s : info.signal_semaphores) {
		scratch.signals.push_back(
			vk::SemaphoreSubmitInfo{
				.semaphore = std::bit_cast<vk::Semaphore>(s.semaphore),
				.value = s.value,
				.stageMask = to_vk(s.stages),
				.deviceIndex = 0,
			}
		);
	}
	scratch.cmds.reserve(info.command_buffers.size());
	for (const auto& c : info.command_buffers) {
		scratch.cmds.push_back(
			vk::CommandBufferSubmitInfo{
				.commandBuffer = std::bit_cast<vk::CommandBuffer>(c.command_buffer),
				.deviceMask = 1,
			}
		);
	}
	return vk::SubmitInfo2{
		.waitSemaphoreInfoCount = static_cast<std::uint32_t>(scratch.waits.size()),
		.pWaitSemaphoreInfos = scratch.waits.data(),
		.commandBufferInfoCount = static_cast<std::uint32_t>(scratch.cmds.size()),
		.pCommandBufferInfos = scratch.cmds.data(),
		.signalSemaphoreInfoCount = static_cast<std::uint32_t>(scratch.signals.size()),
		.pSignalSemaphoreInfos = scratch.signals.data(),
	};
}

auto gse::vulkan::build_vk_present_info(const gpu::present_info& info, present_scratch& scratch) -> vk::PresentInfoKHR {
	scratch.waits.reserve(info.wait_semaphores.size());
	for (const auto h : info.wait_semaphores) {
		scratch.waits.push_back(std::bit_cast<vk::Semaphore>(h));
	}
	scratch.swapchains.reserve(info.swapchains.size());
	for (const auto h : info.swapchains) {
		scratch.swapchains.push_back(std::bit_cast<vk::SwapchainKHR>(h));
	}

	void* pnext_head = nullptr;
	if (!info.present_modes.empty()) {
		scratch.present_modes.reserve(info.present_modes.size());
		for (const auto m : info.present_modes) {
			scratch.present_modes.push_back(to_vk(m));
		}
		scratch.mode_info = vk::SwapchainPresentModeInfoEXT{
			.pNext = pnext_head,
			.swapchainCount = static_cast<std::uint32_t>(scratch.present_modes.size()),
			.pPresentModes = scratch.present_modes.data(),
		};
		pnext_head = &*scratch.mode_info;
	}
	if (!info.release_fences.empty()) {
		scratch.release_fences.reserve(info.release_fences.size());
		for (const auto h : info.release_fences) {
			scratch.release_fences.push_back(std::bit_cast<vk::Fence>(h));
		}
		scratch.fence_info = vk::SwapchainPresentFenceInfoEXT{
			.pNext = pnext_head,
			.swapchainCount = static_cast<std::uint32_t>(scratch.release_fences.size()),
			.pFences = scratch.release_fences.data(),
		};
		pnext_head = &*scratch.fence_info;
	}
	if (!info.present_ids.empty()) {
		scratch.present_id_info = vk::PresentId2KHR{
			.pNext = pnext_head,
			.swapchainCount = static_cast<std::uint32_t>(info.present_ids.size()),
			.pPresentIds = info.present_ids.data(),
		};
		pnext_head = &*scratch.present_id_info;
	}
	if (!info.target_present_times.empty()) {
		const auto stage_queries = to_vk(info.present_stage_queries);
		scratch.timing_infos.reserve(info.target_present_times.size());
		for (std::size_t i = 0; i < info.target_present_times.size(); ++i) {
			scratch.timing_infos.push_back(
				vk::PresentTimingInfoEXT{
					.flags = vk::PresentTimingInfoFlagBitsEXT::ePresentAtRelativeTime,
					.targetTime = static_cast<std::uint64_t>(info.target_present_times[i]),
					.timeDomainId = info.time_domain_id,
					.presentStageQueries = stage_queries,
					.targetTimeDomainPresentStage = {},
				}
			);
		}
		scratch.timing_info = vk::PresentTimingsInfoEXT{
			.pNext = pnext_head,
			.swapchainCount = static_cast<std::uint32_t>(scratch.timing_infos.size()),
			.pTimingInfos = scratch.timing_infos.data(),
		};
		pnext_head = &*scratch.timing_info;
	}

	return vk::PresentInfoKHR{
		.pNext = pnext_head,
		.waitSemaphoreCount = static_cast<std::uint32_t>(scratch.waits.size()),
		.pWaitSemaphores = scratch.waits.data(),
		.swapchainCount = static_cast<std::uint32_t>(scratch.swapchains.size()),
		.pSwapchains = scratch.swapchains.data(),
		.pImageIndices = info.image_indices.data(),
	};
}

auto gse::vulkan::queue_family::complete() const -> bool {
	return graphics_family.has_value() && present_family.has_value() && compute_family.has_value();
}

auto gse::vulkan::find_queue_families(const physical_device& device, const gpu::surface surface) -> queue_family {
	const auto vk_device = std::bit_cast<vk::PhysicalDevice>(device.handle());
	queue_family indices;
	const auto queue_families = vk_device.getQueueFamilyProperties();
	for (std::uint32_t i = 0; i < queue_families.size(); i++) {
		log::println(log::category::vulkan,
					 "Queue family {}: flags = {}",
					 i,
					 vk::to_string(queue_families[i].queueFlags));
		if (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
			indices.graphics_family = i;
		}
		if (const auto vk_surface = std::bit_cast<vk::SurfaceKHR>(surface)) {
			auto [present_result, present_support] = vk_device.getSurfaceSupportKHR(i, vk_surface);
			assert(present_result == vk::Result::eSuccess, "failed to query surface support: {}", vk::to_string(present_result));
			if (present_support) {
				indices.present_family = i;
			}
		}
		if ((queue_families[i].queueFlags & vk::QueueFlagBits::eCompute) && !(queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics)) {
			indices.compute_family = i;
		}
		if ((queue_families[i].queueFlags & vk::QueueFlagBits::eVideoEncodeKHR) && !indices.video_encode_family.has_value()) {
			indices.video_encode_family = i;
		}
	}
	if (!indices.compute_family.has_value()) {
		indices.compute_family = indices.graphics_family;
	}
	return indices;
}

gse::vulkan::queue::queue(const gpu::handle<gpu::queue> graphics, const gpu::handle<gpu::queue> present, const gpu::handle<gpu::queue> compute, const std::uint32_t graphics_family, const std::uint32_t compute_family, const gpu::handle<gpu::queue> video_encode, const std::optional<std::uint32_t> video_encode_family)
	: m_graphics(graphics), m_present(present), m_compute(compute), m_graphics_family_index(graphics_family), m_compute_family_index(compute_family), m_mutex(std::make_unique<std::recursive_mutex>()), m_video_encode(video_encode), m_video_encode_family_index(video_encode_family) {
}

auto gse::vulkan::queue::has_video_encode() const -> bool {
	return m_video_encode_family_index.has_value();
}

auto gse::vulkan::queue::graphics_family_index() const -> std::uint32_t {
	return m_graphics_family_index;
}

auto gse::vulkan::queue::compute_family_index() const -> std::uint32_t {
	return m_compute_family_index;
}

auto gse::vulkan::queue::video_encode_family_index() const -> std::optional<std::uint32_t> {
	return m_video_encode_family_index;
}

auto gse::vulkan::queue::submit(const gpu::queue_type queue, const gpu::submit_info& info, const gpu::handle<gpu::fence> signal_fence) -> gpu::result {
	switch (queue) {
		case gpu::queue_type::graphics:
			return submit_graphics(info, signal_fence);
		case gpu::queue_type::compute:
			return submit_compute(info, signal_fence);
	}
	return gpu::result::error_unknown;
}

auto gse::vulkan::queue::submit_graphics(const gpu::submit_info& info, const gpu::handle<gpu::fence> signal_fence) -> gpu::result {
	std::lock_guard lock(*m_mutex);
	submit_scratch scratch;
	const auto vk_info = build_vk_submit_info(info, scratch);
	const auto result = std::bit_cast<vk::Queue>(m_graphics).submit2(vk_info, std::bit_cast<vk::Fence>(signal_fence));
	return from_vk(result);
}

auto gse::vulkan::queue::submit_compute(const gpu::submit_info& info, const gpu::handle<gpu::fence> signal_fence) -> gpu::result {
	std::lock_guard lock(*m_mutex);
	submit_scratch scratch;
	const auto vk_info = build_vk_submit_info(info, scratch);
	const auto result = std::bit_cast<vk::Queue>(m_compute).submit2(vk_info, std::bit_cast<vk::Fence>(signal_fence));
	return from_vk(result);
}

auto gse::vulkan::queue::submit_video_encode(const gpu::submit_info& info, const gpu::handle<gpu::fence> signal_fence) -> void {
	std::lock_guard lock(*m_mutex);
	submit_scratch scratch;
	const auto vk_info = build_vk_submit_info(info, scratch);
	const auto result = std::bit_cast<vk::Queue>(m_video_encode).submit2(vk_info, std::bit_cast<vk::Fence>(signal_fence));
	assert(result == vk::Result::eSuccess, "failed to submit video encode commands: {}", vk::to_string(result));
}

auto gse::vulkan::queue::present(const gpu::present_info& info) -> gpu::result {
	std::lock_guard lock(*m_mutex);
	present_scratch scratch;
	const auto vk_info = build_vk_present_info(info, scratch);
	auto vk_result = std::bit_cast<vk::Queue>(m_present).presentKHR(vk_info);
	if (vk_result == vk::Result::eErrorPresentTimingQueueFullEXT) {
		present_scratch retry_scratch;
		gpu::present_info retry_info = info;
		retry_info.target_present_times = {};
		const auto retry_vk_info = build_vk_present_info(retry_info, retry_scratch);
		vk_result = std::bit_cast<vk::Queue>(m_present).presentKHR(retry_vk_info);
	}
	if (vk_result != vk::Result::eSuccess && vk_result != vk::Result::eSuboptimalKHR && vk_result != vk::Result::eErrorOutOfDateKHR) {
		log::println(log::level::error, log::category::vulkan, "vkQueuePresentKHR returned {}", vk::to_string(vk_result));
	}
	return from_vk(vk_result);
}
