export module gse.gpu:vulkan_compute_context;

import std;
import vulkan;

import :handles;
import :types;
import :vulkan_device;
import :vulkan_queues;

import gse.core;

export namespace gse::gpu {
	class compute_semaphore_state {
	public:
		compute_semaphore_state() = default;

		compute_semaphore_state(
			const vk::raii::Semaphore* semaphore,
			std::uint64_t value,
			gpu::pipeline_stage_flags dst_stages
		);

		[[nodiscard]] auto has_signaled(
		) const -> bool;

		[[nodiscard]] auto value(
		) const -> std::uint64_t;

		[[nodiscard]] auto raii_semaphore(
		) const -> const vk::raii::Semaphore*;

		[[nodiscard]] auto dst_stages(
		) const -> gpu::pipeline_stage_flags;

		explicit operator bool(
		) const;

	private:
		const vk::raii::Semaphore* m_semaphore = nullptr;
		std::uint64_t m_value = 0;
		gpu::pipeline_stage_flags m_dst_stages = gpu::pipeline_stage_flag::all_commands;
	};
}

export namespace gse::vulkan {
	class compute_context final : public non_copyable {
	public:
		compute_context() = default;

		~compute_context(
		) = default;

		compute_context(
			compute_context&&
		) noexcept = default;

		auto operator=(
			compute_context&&
		) noexcept -> compute_context& = default;

		[[nodiscard]] static auto create(
			const device& dev,
			const queue& q,
			std::uint32_t timestamp_slot_count
		) -> compute_context;

		[[nodiscard]] auto command_buffer_handle(
		) const -> gpu::handle<command_buffer>;

		[[nodiscard]] auto query_pool_handle(
		) const -> gpu::handle<query_pool>;

		[[nodiscard]] auto timeline_handle(
		) const -> gpu::handle<semaphore>;

		[[nodiscard]] auto fence_handle(
		) const -> gpu::handle<fence>;

		[[nodiscard]] auto raii_timeline(
		) const -> const vk::raii::Semaphore&;

		auto wait_fence(
		) const -> void;

		[[nodiscard]] auto is_fence_signaled(
		) const -> bool;

		auto reset_fence(
		) const -> void;

		auto submit(
			std::uint64_t signal_value,
			gpu::pipeline_stage_flags signal_stages
		) const -> void;

		[[nodiscard]] auto read_timestamps(
			std::uint32_t start_slot,
			std::uint32_t end_slot
		) const -> std::optional<std::pair<std::uint64_t, std::uint64_t>>;

		explicit operator bool(
		) const;

	private:
		compute_context(
			vk::raii::CommandPool&& pool,
			vk::raii::CommandBuffer&& cmd,
			vk::raii::Fence&& fence,
			vk::raii::Semaphore&& timeline,
			vk::raii::QueryPool&& query_pool,
			const vk::raii::Device* device,
			const vk::raii::Queue* queue
		);

		vk::raii::CommandPool m_pool = nullptr;
		vk::raii::CommandBuffer m_cmd = nullptr;
		vk::raii::Fence m_fence = nullptr;
		vk::raii::Semaphore m_timeline = nullptr;
		vk::raii::QueryPool m_query_pool = nullptr;
		const vk::raii::Device* m_device = nullptr;
		const vk::raii::Queue* m_queue = nullptr;
	};
}

gse::gpu::compute_semaphore_state::compute_semaphore_state(const vk::raii::Semaphore* semaphore, const std::uint64_t value, const gpu::pipeline_stage_flags dst_stages)
	: m_semaphore(semaphore), m_value(value), m_dst_stages(dst_stages) {}

auto gse::gpu::compute_semaphore_state::has_signaled() const -> bool {
	if (m_semaphore == nullptr || m_value == 0) {
		return false;
	}
	return m_semaphore->getCounterValue() >= m_value;
}

auto gse::gpu::compute_semaphore_state::value() const -> std::uint64_t {
	return m_value;
}

auto gse::gpu::compute_semaphore_state::raii_semaphore() const -> const vk::raii::Semaphore* {
	return m_semaphore;
}

auto gse::gpu::compute_semaphore_state::dst_stages() const -> gpu::pipeline_stage_flags {
	return m_dst_stages;
}

gse::gpu::compute_semaphore_state::operator bool() const {
	return m_semaphore != nullptr && m_value > 0;
}

gse::vulkan::compute_context::compute_context(vk::raii::CommandPool&& pool, vk::raii::CommandBuffer&& cmd, vk::raii::Fence&& fence, vk::raii::Semaphore&& timeline, vk::raii::QueryPool&& query_pool, const vk::raii::Device* device, const vk::raii::Queue* queue)
	: m_pool(std::move(pool)),
	  m_cmd(std::move(cmd)),
	  m_fence(std::move(fence)),
	  m_timeline(std::move(timeline)),
	  m_query_pool(std::move(query_pool)),
	  m_device(device),
	  m_queue(queue) {}

auto gse::vulkan::compute_context::create(const device& dev, const queue& q, const std::uint32_t timestamp_slot_count) -> compute_context {
	const auto& vk_device = dev.raii_device();

	auto pool = vk_device.createCommandPool({
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = q.compute_family_index(),
	});

	auto cmd = std::move(vk_device.allocateCommandBuffers({
		.commandPool = *pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,
	}).front());

	auto fence = vk_device.createFence({
		.flags = vk::FenceCreateFlagBits::eSignaled,
	});

	constexpr vk::SemaphoreTypeCreateInfo timeline_type{
		.semaphoreType = vk::SemaphoreType::eTimeline,
		.initialValue = 0,
	};
	auto timeline = vk_device.createSemaphore({
		.pNext = &timeline_type,
	});

	auto query_pool = vk_device.createQueryPool({
		.queryType = vk::QueryType::eTimestamp,
		.queryCount = timestamp_slot_count,
	});

	query_pool.reset(0, timestamp_slot_count);

	return compute_context(
		std::move(pool),
		std::move(cmd),
		std::move(fence),
		std::move(timeline),
		std::move(query_pool),
		&vk_device,
		&q.raii_compute_queue()
	);
}

auto gse::vulkan::compute_context::command_buffer_handle() const -> gpu::handle<command_buffer> {
	return std::bit_cast<gpu::handle<command_buffer>>(*m_cmd);
}

auto gse::vulkan::compute_context::query_pool_handle() const -> gpu::handle<query_pool> {
	return std::bit_cast<gpu::handle<query_pool>>(*m_query_pool);
}

auto gse::vulkan::compute_context::timeline_handle() const -> gpu::handle<semaphore> {
	return std::bit_cast<gpu::handle<semaphore>>(*m_timeline);
}

auto gse::vulkan::compute_context::fence_handle() const -> gpu::handle<fence> {
	return std::bit_cast<gpu::handle<fence>>(*m_fence);
}

auto gse::vulkan::compute_context::raii_timeline() const -> const vk::raii::Semaphore& {
	return m_timeline;
}

auto gse::vulkan::compute_context::wait_fence() const -> void {
	if (!*m_fence) {
		return;
	}
	static_cast<void>(m_device->waitForFences(*m_fence, vk::True, std::numeric_limits<std::uint64_t>::max()));
}

auto gse::vulkan::compute_context::is_fence_signaled() const -> bool {
	if (!*m_fence) {
		return true;
	}
	return m_fence.getStatus() == vk::Result::eSuccess;
}

auto gse::vulkan::compute_context::reset_fence() const -> void {
	m_device->resetFences(*m_fence);
}

auto gse::vulkan::compute_context::submit(const std::uint64_t signal_value, const gpu::pipeline_stage_flags signal_stages) const -> void {
	const vk::CommandBufferSubmitInfo cmd_info{
		.commandBuffer = *m_cmd,
		.deviceMask = 1,
	};

	const vk::SemaphoreSubmitInfo signal_info{
		.semaphore = *m_timeline,
		.value = signal_value,
		.stageMask = to_vk(signal_stages),
		.deviceIndex = 0,
	};

	const vk::SubmitInfo2 submit_info{
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &cmd_info,
		.signalSemaphoreInfoCount = 1,
		.pSignalSemaphoreInfos = &signal_info,
	};

	m_queue->submit2(submit_info, *m_fence);
}

auto gse::vulkan::compute_context::read_timestamps(const std::uint32_t start_slot, const std::uint32_t end_slot) const -> std::optional<std::pair<std::uint64_t, std::uint64_t>> {
	const auto [r0, start_ts] = m_query_pool.getResult<std::uint64_t>(
		start_slot, 1, sizeof(std::uint64_t), vk::QueryResultFlagBits::e64
	);
	const auto [r1, end_ts] = m_query_pool.getResult<std::uint64_t>(
		end_slot, 1, sizeof(std::uint64_t), vk::QueryResultFlagBits::e64
	);
	if (r0 == vk::Result::eSuccess && r1 == vk::Result::eSuccess) {
		return std::pair{ start_ts, end_ts };
	}
	return std::nullopt;
}

gse::vulkan::compute_context::operator bool() const {
	return *m_cmd != nullptr;
}
