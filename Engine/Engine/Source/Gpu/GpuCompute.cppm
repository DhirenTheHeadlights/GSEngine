export module gse.gpu.resources:gpu_compute;

import std;

import gse.gpu.types;
import gse.gpu.vulkan;
import gse.gpu.shader;
import gse.gpu.device;
import gse.gpu.context;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;

export namespace gse::gpu {
	struct buffer_copy {
		const vulkan::buffer_resource& src;
		const vulkan::buffer_resource& dst;
		std::size_t src_offset = 0;
		std::size_t dst_offset = 0;
		std::size_t size = 0;
	};

	class compute_queue final : public non_copyable {
	public:
		compute_queue() = default;
		compute_queue(
			vk::raii::CommandPool&& pool,
			vk::raii::CommandBuffer&& cmd,
			vk::raii::Fence&& fence,
			vk::raii::Semaphore&& timeline,
			vk::raii::QueryPool&& query_pool,
			const vk::raii::Queue* queue,
			const vk::raii::Device* device,
			vulkan::descriptor_heap* heap,
			float timestamp_period
		);
		compute_queue(compute_queue&&) noexcept = default;
		auto operator=(compute_queue&&) noexcept -> compute_queue& = default;

		auto wait() const -> void;
		auto is_complete() const -> bool;
		auto begin() const -> void;
		auto submit() -> void;

		[[nodiscard]] auto semaphore_state(
		) const -> compute_semaphore_state;

		auto bind_pipeline(
			const vulkan::pipeline_handle& p
		) const -> void;

		auto bind_descriptors(
			const vulkan::pipeline_handle& p,
			const vulkan::descriptor_region& region
		) const -> void;

		auto dispatch(
			std::uint32_t x,
			std::uint32_t y = 1,
			std::uint32_t z = 1
		) const -> void;

		auto dispatch_indirect(
			const vulkan::buffer_resource& buf,
			std::size_t offset = 0
		) const -> void;

		auto barrier(
			barrier_scope scope = barrier_scope::compute_to_compute
		) const -> void;

		auto barriers(
			std::span<const barrier_scope> scopes
		) const -> void;

		auto copy_buffer(
			const buffer_copy& copy
		) const -> void;

		auto push(
			const vulkan::pipeline_handle& p,
			const cached_push_constants& cache
		) const -> void;

		static constexpr std::uint32_t timing_slot_count = 32;

		auto begin_timing(
		) const -> void;

		auto end_timing(
		) const -> void;

		auto mark_timing(
			std::uint32_t slot
		) const -> void;

		[[nodiscard]] auto read_timing(
		) const -> float;

		[[nodiscard]] auto read_timing(
			std::uint32_t start_slot,
			std::uint32_t end_slot
		) const -> float;

		[[nodiscard]] auto native_command_buffer(
		) const -> vk::CommandBuffer;

		explicit operator bool() const;

	private:
		vk::raii::CommandPool m_pool = nullptr;
		vk::raii::CommandBuffer m_cmd = nullptr;
		vk::raii::Fence m_fence = nullptr;
		vk::raii::Semaphore m_timeline = nullptr;
		std::uint64_t m_timeline_value = 0;
		vk::raii::QueryPool m_query_pool = nullptr;
		const vk::raii::Queue* m_queue = nullptr;
		const vk::raii::Device* m_device = nullptr;
		vulkan::descriptor_heap* m_descriptor_heap = nullptr;
		float m_timestamp_period = 0.0f;
		std::uint32_t m_frame_count = 0;
	};

	auto create_compute_queue(
		context& ctx
	) -> compute_queue;
}

namespace gse::vulkan {
	auto to_vk(const gpu::barrier_scope scope) -> vk::MemoryBarrier2 {
		using enum gpu::barrier_scope;
		switch (scope) {
			case compute_to_compute:
				return {
					.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
					.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
					.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
					.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite
				};
			case compute_to_indirect:
				return {
					.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
					.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
					.dstStageMask = vk::PipelineStageFlagBits2::eDrawIndirect | vk::PipelineStageFlagBits2::eComputeShader,
					.dstAccessMask = vk::AccessFlagBits2::eIndirectCommandRead | vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite
				};
			case host_to_compute:
				return {
					.srcStageMask = vk::PipelineStageFlagBits2::eHost,
					.srcAccessMask = vk::AccessFlagBits2::eHostWrite,
					.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
					.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite
				};
			case transfer_to_compute:
				return {
					.srcStageMask = vk::PipelineStageFlagBits2::eCopy,
					.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
					.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
					.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite
				};
			case compute_to_transfer:
				return {
					.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
					.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
					.dstStageMask = vk::PipelineStageFlagBits2::eCopy,
					.dstAccessMask = vk::AccessFlagBits2::eTransferRead
				};
			case transfer_to_host:
				return {
					.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
					.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
					.dstStageMask = vk::PipelineStageFlagBits2::eHost,
					.dstAccessMask = vk::AccessFlagBits2::eHostRead
				};
			case transfer_to_transfer:
				return {
					.srcStageMask = vk::PipelineStageFlagBits2::eCopy,
					.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
					.dstStageMask = vk::PipelineStageFlagBits2::eCopy,
					.dstAccessMask = vk::AccessFlagBits2::eTransferWrite
				};
		}
		return {};
	}
}

gse::gpu::compute_queue::compute_queue(
	vk::raii::CommandPool&& pool,
	vk::raii::CommandBuffer&& cmd,
	vk::raii::Fence&& fence,
	vk::raii::Semaphore&& timeline,
	vk::raii::QueryPool&& query_pool,
	const vk::raii::Queue* queue,
	const vk::raii::Device* device,
	vulkan::descriptor_heap* heap,
	const float timestamp_period
) : m_pool(std::move(pool)),
    m_cmd(std::move(cmd)),
    m_fence(std::move(fence)),
    m_timeline(std::move(timeline)),
    m_query_pool(std::move(query_pool)),
    m_queue(queue),
    m_device(device),
    m_descriptor_heap(heap),
    m_timestamp_period(timestamp_period) {}

auto gse::gpu::compute_queue::wait() const -> void {
	if (!*m_fence) {
		return;
	}
	static_cast<void>(m_device->waitForFences(*m_fence, vk::True, std::numeric_limits<std::uint64_t>::max()));
}

auto gse::gpu::compute_queue::is_complete() const -> bool {
	if (!*m_fence) {
		return true;
	}
	return m_fence.getStatus() == vk::Result::eSuccess;
}

auto gse::gpu::compute_queue::begin() const -> void {
	m_cmd.reset({});
	m_cmd.begin({});
	m_cmd.resetQueryPool(*m_query_pool, 0, timing_slot_count);
	if (m_descriptor_heap) {
		m_descriptor_heap->bind_buffer(*m_cmd);
	}
}

auto gse::gpu::compute_queue::submit() -> void {
	m_cmd.end();

	++m_timeline_value;

	const vk::CommandBufferSubmitInfo cmd_info{
		.commandBuffer = *m_cmd,
		.deviceMask = 1
	};

	const vk::SemaphoreSubmitInfo signal_info{
		.semaphore = *m_timeline,
		.value = m_timeline_value,
		.stageMask = vk::PipelineStageFlagBits2::eAllCommands,
		.deviceIndex = 0
	};

	const vk::SubmitInfo2 submit_info{
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &cmd_info,
		.signalSemaphoreInfoCount = 1,
		.pSignalSemaphoreInfos = &signal_info
	};

	m_device->resetFences(*m_fence);
	m_queue->submit2(submit_info, *m_fence);
	++m_frame_count;
}

auto gse::gpu::compute_queue::semaphore_state() const -> compute_semaphore_state {
	return {
		.semaphore = &m_timeline,
		.value = m_timeline_value,
		.dst_stage = vk::PipelineStageFlagBits2::eComputeShader
	};
}

auto gse::gpu::compute_queue::bind_pipeline(const vulkan::pipeline_handle& p) const -> void {
	m_cmd.bindPipeline(vk::PipelineBindPoint::eCompute, p.pipeline);
}

auto gse::gpu::compute_queue::bind_descriptors(const vulkan::pipeline_handle& p, const vulkan::descriptor_region& region) const -> void {
	region.heap->bind(*m_cmd, vk::PipelineBindPoint::eCompute, p.layout, 0, region);
}

auto gse::gpu::compute_queue::dispatch(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) const -> void {
	m_cmd.dispatch(x, y, z);
}

auto gse::gpu::compute_queue::dispatch_indirect(const vulkan::buffer_resource& buf, const std::size_t offset) const -> void {
	m_cmd.dispatchIndirect(buf.buffer, static_cast<vk::DeviceSize>(offset));
}

auto gse::gpu::compute_queue::barrier(const barrier_scope scope) const -> void {
	const auto b = vulkan::to_vk(scope);
	const vk::DependencyInfo dep{ .memoryBarrierCount = 1, .pMemoryBarriers = &b };
	m_cmd.pipelineBarrier2(dep);
}

auto gse::gpu::compute_queue::barriers(const std::span<const barrier_scope> scopes) const -> void {
	std::vector<vk::MemoryBarrier2> vk_barriers;
	vk_barriers.reserve(scopes.size());
	for (const auto s : scopes) {
		vk_barriers.push_back(vulkan::to_vk(s));
	}
	const vk::DependencyInfo dep{
		.memoryBarrierCount = static_cast<std::uint32_t>(vk_barriers.size()),
		.pMemoryBarriers = vk_barriers.data()
	};
	m_cmd.pipelineBarrier2(dep);
}

auto gse::gpu::compute_queue::copy_buffer(const buffer_copy& copy) const -> void {
	m_cmd.copyBuffer(
		copy.src.buffer,
		copy.dst.buffer,
		vk::BufferCopy{
			.srcOffset = copy.src_offset,
			.dstOffset = copy.dst_offset,
			.size = copy.size
		}
	);
}

auto gse::gpu::compute_queue::push(const vulkan::pipeline_handle& p, const cached_push_constants& cache) const -> void {
	cache.replay(*m_cmd, p.layout);
}

auto gse::gpu::compute_queue::begin_timing() const -> void {
	m_cmd.writeTimestamp2(vk::PipelineStageFlagBits2::eTopOfPipe, *m_query_pool, 0);
}

auto gse::gpu::compute_queue::end_timing() const -> void {
	m_cmd.writeTimestamp2(vk::PipelineStageFlagBits2::eComputeShader, *m_query_pool, 1);
}

auto gse::gpu::compute_queue::mark_timing(const std::uint32_t slot) const -> void {
	assert(slot < timing_slot_count, std::source_location::current(), "mark_timing slot out of range");
	m_cmd.writeTimestamp2(vk::PipelineStageFlagBits2::eComputeShader, *m_query_pool, slot);
}

auto gse::gpu::compute_queue::read_timing() const -> float {
	return read_timing(0, 1);
}

auto gse::gpu::compute_queue::read_timing(const std::uint32_t start_slot, const std::uint32_t end_slot) const -> float {
	if (m_frame_count < 2) return 0.0f;
	assert(start_slot < timing_slot_count && end_slot < timing_slot_count, std::source_location::current(), "read_timing slot out of range");
	const auto [r0, start_ts] = m_query_pool.getResult<std::uint64_t>(
		start_slot, 1, sizeof(std::uint64_t), vk::QueryResultFlagBits::e64
	);
	const auto [r1, end_ts] = m_query_pool.getResult<std::uint64_t>(
		end_slot, 1, sizeof(std::uint64_t), vk::QueryResultFlagBits::e64
	);
	if (r0 == vk::Result::eSuccess && r1 == vk::Result::eSuccess) {
		return static_cast<float>(end_ts - start_ts) * m_timestamp_period * 1e-6f;
	}
	return 0.0f;
}

auto gse::gpu::compute_queue::native_command_buffer() const -> vk::CommandBuffer {
	return *m_cmd;
}

gse::gpu::compute_queue::operator bool() const {
	return *m_cmd != nullptr;
}

auto gse::gpu::create_compute_queue(context& ctx) -> compute_queue {
	auto& dev = ctx.device();
	const auto& vk_device = ctx.logical_device();

	auto pool = vk_device.createCommandPool({
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = dev.compute_queue_family()
	});

	auto cmd = std::move(vk_device.allocateCommandBuffers({
		.commandPool = *pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1
	}).front());

	auto fence = vk_device.createFence({
		.flags = vk::FenceCreateFlagBits::eSignaled
	});

	constexpr vk::SemaphoreTypeCreateInfo timeline_type{
		.semaphoreType = vk::SemaphoreType::eTimeline,
		.initialValue = 0
	};
	auto timeline = vk_device.createSemaphore({
		.pNext = &timeline_type
	});

	auto query_pool = vk_device.createQueryPool({
		.queryType = vk::QueryType::eTimestamp,
		.queryCount = compute_queue::timing_slot_count
	});

	query_pool.reset(0, compute_queue::timing_slot_count);

	const float timestamp_period = dev.physical_device().getProperties().limits.timestampPeriod;

	return compute_queue(
		std::move(pool),
		std::move(cmd),
		std::move(fence),
		std::move(timeline),
		std::move(query_pool),
		&dev.compute_queue(),
		&vk_device,
		&ctx.descriptor_heap(),
		timestamp_period
	);
}
