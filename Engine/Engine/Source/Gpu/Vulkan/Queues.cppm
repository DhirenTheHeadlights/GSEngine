export module gse.gpu:vulkan_queues;

import std;
import vulkan;

import :handles;
import :types;
import :vulkan_commands;

import gse.core;
import gse.log;

export namespace gse::vulkan {
	class fence;
	class semaphore;
	class swap_chain;
}

export namespace gse::gpu {
	struct semaphore_submit_info {
		gpu::handle<vulkan::semaphore> semaphore;
		std::uint64_t value = 0;
		pipeline_stage_flags stages;
	};

	struct command_buffer_submit_info {
		gpu::handle<vulkan::command_buffer> command_buffer;
	};

	struct submit_info {
		std::span<const semaphore_submit_info> wait_semaphores;
		std::span<const command_buffer_submit_info> command_buffers;
		std::span<const semaphore_submit_info> signal_semaphores;
	};

	struct present_info {
		std::span<const gpu::handle<vulkan::semaphore>> wait_semaphores;
		std::span<const gpu::handle<vulkan::swap_chain>> swapchains;
		std::span<const std::uint32_t> image_indices;
	};
}

export namespace gse::vulkan {
	struct queue_family {
		std::optional<std::uint32_t> graphics_family;
		std::optional<std::uint32_t> present_family;
		std::optional<std::uint32_t> compute_family;
		std::optional<std::uint32_t> video_encode_family;

		[[nodiscard]] auto complete(
		) const -> bool;
	};

	[[nodiscard]] auto find_queue_families(
		const vk::raii::PhysicalDevice& device,
		const vk::raii::SurfaceKHR& surface
	) -> queue_family;

	class queue : public non_copyable {
	public:
		queue(
			vk::raii::Queue&& graphics,
			vk::raii::Queue&& present,
			vk::raii::Queue&& compute,
			std::uint32_t graphics_family,
			std::uint32_t compute_family
		);

		~queue(
		) = default;

		queue(
			queue&&
		) noexcept = default;

		auto operator=(
			queue&&
		) noexcept -> queue& = default;

		auto set_video_encode(
			vk::raii::Queue&& q,
			std::uint32_t family
		) -> void;

		[[nodiscard]] auto has_video_encode(
		) const -> bool;

		[[nodiscard]] auto graphics_family_index(
		) const -> std::uint32_t;

		[[nodiscard]] auto compute_family_index(
		) const -> std::uint32_t;

		[[nodiscard]] auto video_encode_family_index(
		) const -> std::optional<std::uint32_t>;

		[[nodiscard]] auto raii_compute_queue(
		) const -> const vk::raii::Queue&;

		auto submit_graphics(
			const gpu::submit_info& info,
			gpu::handle<fence> signal_fence = {}
		) -> void;

		auto submit_compute(
			const gpu::submit_info& info,
			gpu::handle<fence> signal_fence = {}
		) -> void;

		auto submit_video_encode(
			const gpu::submit_info& info,
			gpu::handle<fence> signal_fence = {}
		) -> void;

		[[nodiscard]] auto present(
			const gpu::present_info& info
		) -> gpu::result;

	private:
		vk::raii::Queue m_graphics;
		vk::raii::Queue m_present;
		vk::raii::Queue m_compute;
		std::uint32_t m_graphics_family_index = 0;
		std::uint32_t m_compute_family_index = 0;
		std::unique_ptr<std::recursive_mutex> m_mutex;

		vk::raii::Queue m_video_encode{ nullptr };
		std::optional<std::uint32_t> m_video_encode_family_index;
	};
}

namespace gse::vulkan {
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
	};

	auto build_vk_present_info(
		const gpu::present_info& info,
		present_scratch& scratch
	) -> vk::PresentInfoKHR;
}

auto gse::vulkan::build_vk_submit_info(const gpu::submit_info& info, submit_scratch& scratch) -> vk::SubmitInfo2 {
	scratch.waits.reserve(info.wait_semaphores.size());
	for (const auto& w : info.wait_semaphores) {
		scratch.waits.push_back(vk::SemaphoreSubmitInfo{
			.semaphore = std::bit_cast<vk::Semaphore>(w.semaphore),
			.value = w.value,
			.stageMask = to_vk(w.stages),
			.deviceIndex = 0,
		});
	}
	scratch.signals.reserve(info.signal_semaphores.size());
	for (const auto& s : info.signal_semaphores) {
		scratch.signals.push_back(vk::SemaphoreSubmitInfo{
			.semaphore = std::bit_cast<vk::Semaphore>(s.semaphore),
			.value = s.value,
			.stageMask = to_vk(s.stages),
			.deviceIndex = 0,
		});
	}
	scratch.cmds.reserve(info.command_buffers.size());
	for (const auto& c : info.command_buffers) {
		scratch.cmds.push_back(vk::CommandBufferSubmitInfo{
			.commandBuffer = std::bit_cast<vk::CommandBuffer>(c.command_buffer),
			.deviceMask = 1,
		});
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
	return vk::PresentInfoKHR{
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

auto gse::vulkan::find_queue_families(const vk::raii::PhysicalDevice& device, const vk::raii::SurfaceKHR& surface) -> queue_family {
	queue_family indices;
	const auto queue_families = device.getQueueFamilyProperties();
	for (std::uint32_t i = 0; i < queue_families.size(); i++) {
		log::println(log::category::vulkan, "Queue family {}: flags = {}", i, vk::to_string(queue_families[i].queueFlags));
		if (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
			indices.graphics_family = i;
		}
		if (device.getSurfaceSupportKHR(i, surface)) {
			indices.present_family = i;
		}
		if ((queue_families[i].queueFlags & vk::QueueFlagBits::eCompute) &&
			!(queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics)) {
			indices.compute_family = i;
		}
		if ((queue_families[i].queueFlags & vk::QueueFlagBits::eVideoEncodeKHR) &&
			!indices.video_encode_family.has_value()) {
			indices.video_encode_family = i;
		}
	}
	if (!indices.compute_family.has_value()) {
		indices.compute_family = indices.graphics_family;
	}
	return indices;
}

gse::vulkan::queue::queue(vk::raii::Queue&& graphics, vk::raii::Queue&& present, vk::raii::Queue&& compute, const std::uint32_t graphics_family, const std::uint32_t compute_family)
	: m_graphics(std::move(graphics)),
	  m_present(std::move(present)),
	  m_compute(std::move(compute)),
	  m_graphics_family_index(graphics_family),
	  m_compute_family_index(compute_family),
	  m_mutex(std::make_unique<std::recursive_mutex>()) {}

auto gse::vulkan::queue::set_video_encode(vk::raii::Queue&& q, const std::uint32_t family) -> void {
	m_video_encode = std::move(q);
	m_video_encode_family_index = family;
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

auto gse::vulkan::queue::raii_compute_queue() const -> const vk::raii::Queue& {
	return m_compute;
}

auto gse::vulkan::queue::submit_graphics(const gpu::submit_info& info, const gpu::handle<fence> signal_fence) -> void {
	std::lock_guard lock(*m_mutex);
	submit_scratch scratch;
	const auto vk_info = build_vk_submit_info(info, scratch);
	m_graphics.submit2(vk_info, std::bit_cast<vk::Fence>(signal_fence));
}

auto gse::vulkan::queue::submit_compute(const gpu::submit_info& info, const gpu::handle<fence> signal_fence) -> void {
	std::lock_guard lock(*m_mutex);
	submit_scratch scratch;
	const auto vk_info = build_vk_submit_info(info, scratch);
	m_compute.submit2(vk_info, std::bit_cast<vk::Fence>(signal_fence));
}

auto gse::vulkan::queue::submit_video_encode(const gpu::submit_info& info, const gpu::handle<fence> signal_fence) -> void {
	std::lock_guard lock(*m_mutex);
	submit_scratch scratch;
	const auto vk_info = build_vk_submit_info(info, scratch);
	m_video_encode.submit2(vk_info, std::bit_cast<vk::Fence>(signal_fence));
}

auto gse::vulkan::queue::present(const gpu::present_info& info) -> gpu::result {
	std::lock_guard lock(*m_mutex);
	present_scratch scratch;
	const auto vk_info = build_vk_present_info(info, scratch);
	return from_vk(m_present.presentKHR(vk_info));
}
