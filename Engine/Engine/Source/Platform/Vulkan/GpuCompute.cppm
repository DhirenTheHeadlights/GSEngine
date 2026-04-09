export module gse.platform:gpu_compute;

import std;

import :gpu_types;
import :gpu_buffer;
import :gpu_pipeline;
import :gpu_descriptor;
import :vulkan_allocator;
import :descriptor_heap;
import :gpu_push_constants;

import gse.assert;
import gse.utility;

export namespace gse::gpu {
	enum class barrier_scope : std::uint8_t {
		compute_to_compute,
		host_to_compute,
		compute_to_transfer,
		transfer_to_host,
		transfer_to_transfer
	};

	struct buffer_copy {
		const buffer* src = nullptr;
		const buffer* dst = nullptr;
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
			vk::raii::QueryPool&& query_pool,
			const vk::raii::Queue* queue,
			const vk::raii::Device* device,
			vulkan::descriptor_heap* heap,
			float timestamp_period
		);
		compute_queue(compute_queue&&) noexcept = default;
		auto operator=(compute_queue&&) noexcept -> compute_queue& = default;

		auto wait() const -> void;
		auto begin() const -> void;
		auto submit() -> void;

		auto bind_pipeline(
			const pipeline& p
		) const -> void;

		auto bind_descriptors(
			const pipeline& p,
			const descriptor_region& region
		) const -> void;

		auto dispatch(
			std::uint32_t x,
			std::uint32_t y = 1,
			std::uint32_t z = 1
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
			const pipeline& p,
			const cached_push_constants& cache
		) const -> void;

		auto begin_timing(
		) const -> void;

		auto end_timing(
		) const -> void;

		[[nodiscard]] auto read_timing(
		) const -> float;

		[[nodiscard]] auto native_command_buffer(
		) const -> vk::CommandBuffer;

		explicit operator bool() const;

	private:
		vk::raii::CommandPool m_pool = nullptr;
		vk::raii::CommandBuffer m_cmd = nullptr;
		vk::raii::Fence m_fence = nullptr;
		vk::raii::QueryPool m_query_pool = nullptr;
		const vk::raii::Queue* m_queue = nullptr;
		const vk::raii::Device* m_device = nullptr;
		vulkan::descriptor_heap* m_descriptor_heap = nullptr;
		float m_timestamp_period = 0.0f;
		std::uint32_t m_frame_count = 0;
	};
}

namespace {
	auto to_vk(const gse::gpu::barrier_scope scope) -> vk::MemoryBarrier2 {
		using enum gse::gpu::barrier_scope;
		switch (scope) {
			case compute_to_compute:
				return {
					.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
					.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
					.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
					.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite
				};
			case host_to_compute:
				return {
					.srcStageMask = vk::PipelineStageFlagBits2::eHost,
					.srcAccessMask = vk::AccessFlagBits2::eHostWrite,
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
	vk::raii::QueryPool&& query_pool,
	const vk::raii::Queue* queue,
	const vk::raii::Device* device,
	vulkan::descriptor_heap* heap,
	const float timestamp_period
) : m_pool(std::move(pool)),
    m_cmd(std::move(cmd)),
    m_fence(std::move(fence)),
    m_query_pool(std::move(query_pool)),
    m_queue(queue),
    m_device(device),
    m_descriptor_heap(heap),
    m_timestamp_period(timestamp_period) {}

auto gse::gpu::compute_queue::wait() const -> void {
	if (!*m_fence) return;
	static_cast<void>(m_device->waitForFences(*m_fence, vk::True, std::numeric_limits<std::uint64_t>::max()));
}

auto gse::gpu::compute_queue::begin() const -> void {
	m_cmd.reset({});
	m_cmd.begin({});
	(*m_cmd).resetQueryPool(*m_query_pool, 0, 4);
	if (m_descriptor_heap) {
		m_descriptor_heap->bind_buffer(*m_cmd);
	}
}

auto gse::gpu::compute_queue::submit() -> void {
	m_cmd.end();
	const vk::CommandBuffer submit_cmd = *m_cmd;
	const vk::SubmitInfo submit_info{
		.commandBufferCount = 1,
		.pCommandBuffers = &submit_cmd
	};
	(**m_device).resetFences(*m_fence);
	m_queue->submit(submit_info, *m_fence);
	++m_frame_count;
}

auto gse::gpu::compute_queue::bind_pipeline(const pipeline& p) const -> void {
	(*m_cmd).bindPipeline(vk::PipelineBindPoint::eCompute, p.native_pipeline());
}

auto gse::gpu::compute_queue::bind_descriptors(const pipeline& p, const descriptor_region& region) const -> void {
	const auto& native = region.native();
	native.heap->bind(*m_cmd, vk::PipelineBindPoint::eCompute, p.native_layout(), 0, native);
}

auto gse::gpu::compute_queue::dispatch(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) const -> void {
	(*m_cmd).dispatch(x, y, z);
}

auto gse::gpu::compute_queue::barrier(const barrier_scope scope) const -> void {
	const auto b = to_vk(scope);
	const vk::DependencyInfo dep{ .memoryBarrierCount = 1, .pMemoryBarriers = &b };
	(*m_cmd).pipelineBarrier2(dep);
}

auto gse::gpu::compute_queue::barriers(const std::span<const barrier_scope> scopes) const -> void {
	std::vector<vk::MemoryBarrier2> vk_barriers;
	vk_barriers.reserve(scopes.size());
	for (const auto s : scopes) {
		vk_barriers.push_back(to_vk(s));
	}
	const vk::DependencyInfo dep{
		.memoryBarrierCount = static_cast<std::uint32_t>(vk_barriers.size()),
		.pMemoryBarriers = vk_barriers.data()
	};
	(*m_cmd).pipelineBarrier2(dep);
}

auto gse::gpu::compute_queue::copy_buffer(const buffer_copy& copy) const -> void {
	(*m_cmd).copyBuffer(
		copy.src->native().buffer,
		copy.dst->native().buffer,
		vk::BufferCopy{
			.srcOffset = copy.src_offset,
			.dstOffset = copy.dst_offset,
			.size = copy.size
		}
	);
}

auto gse::gpu::compute_queue::push(const pipeline& p, const cached_push_constants& cache) const -> void {
	cache.replay(*m_cmd, p.native_layout());
}

auto gse::gpu::compute_queue::begin_timing() const -> void {
	(*m_cmd).writeTimestamp2(vk::PipelineStageFlagBits2::eTopOfPipe, *m_query_pool, 0);
}

auto gse::gpu::compute_queue::end_timing() const -> void {
	(*m_cmd).writeTimestamp2(vk::PipelineStageFlagBits2::eComputeShader, *m_query_pool, 1);
}

auto gse::gpu::compute_queue::read_timing() const -> float {
	if (m_frame_count < 2) return 0.0f;
	std::array<std::uint64_t, 2> timestamps{};
	const auto result = (**m_device).getQueryPoolResults(
		*m_query_pool, 0, 2,
		sizeof(timestamps), timestamps.data(), sizeof(std::uint64_t),
		vk::QueryResultFlagBits::e64
	);
	if (result == vk::Result::eSuccess) {
		return static_cast<float>(timestamps[1] - timestamps[0]) * m_timestamp_period * 1e-6f;
	}
	return 0.0f;
}

auto gse::gpu::compute_queue::native_command_buffer() const -> vk::CommandBuffer {
	return *m_cmd;
}

gse::gpu::compute_queue::operator bool() const {
	return *m_cmd != nullptr;
}
