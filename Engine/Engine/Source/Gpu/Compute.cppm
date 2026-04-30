export module gse.gpu:compute;

import std;

import :types;
import :pipeline;
import :vulkan_device;
import :vulkan_commands;
import :vulkan_compute_context;
import :descriptor_heap;
import :shader;
import :device;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;

export namespace gse::gpu {
	struct buffer_copy {
		const vulkan::basic_buffer<vulkan::device>& src;
		const vulkan::basic_buffer<vulkan::device>& dst;
		std::size_t src_offset = 0;
		std::size_t dst_offset = 0;
		std::size_t size = 0;
	};

	class compute_queue final : public non_copyable {
	public:
		compute_queue() = default;

		compute_queue(
			vulkan::compute_context&& ctx,
			descriptor_heap* heap,
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

		auto dispatch_indirect(
			const vulkan::basic_buffer<vulkan::device>& buf,
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
			const pipeline& p,
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
		) const -> handle<command_buffer>;

		explicit operator bool(
		) const;

		[[nodiscard]] static auto create(
			gpu::device& dev
		) -> compute_queue;

	private:
		vulkan::compute_context m_ctx;
		std::uint64_t m_timeline_value = 0;
		descriptor_heap* m_descriptor_heap = nullptr;
		float m_timestamp_period = 0.0f;
		std::uint32_t m_frame_count = 0;
	};
}

namespace {
	auto barrier_scope_to_gpu(const gse::gpu::barrier_scope scope) -> gse::gpu::memory_barrier {
		using ps = gse::gpu::pipeline_stage_flag;
		using ac = gse::gpu::access_flag;
		switch (scope) {
			case gse::gpu::barrier_scope::compute_to_compute:
				return {
					.src_stages = ps::compute_shader,
					.src_access = ac::shader_storage_write,
					.dst_stages = ps::compute_shader,
					.dst_access = ac::shader_storage_read | ac::shader_storage_write,
				};
			case gse::gpu::barrier_scope::compute_to_indirect:
				return {
					.src_stages = ps::compute_shader,
					.src_access = ac::shader_storage_write,
					.dst_stages = ps::draw_indirect | ps::compute_shader,
					.dst_access = ac::indirect_command_read | ac::shader_storage_read | ac::shader_storage_write,
				};
			case gse::gpu::barrier_scope::host_to_compute:
				return {
					.src_stages = ps::host,
					.src_access = ac::host_write,
					.dst_stages = ps::compute_shader,
					.dst_access = ac::shader_storage_read | ac::shader_storage_write,
				};
			case gse::gpu::barrier_scope::transfer_to_compute:
				return {
					.src_stages = ps::copy,
					.src_access = ac::transfer_write,
					.dst_stages = ps::compute_shader,
					.dst_access = ac::shader_storage_read | ac::shader_storage_write,
				};
			case gse::gpu::barrier_scope::compute_to_transfer:
				return {
					.src_stages = ps::compute_shader,
					.src_access = ac::shader_storage_write,
					.dst_stages = ps::copy,
					.dst_access = ac::transfer_read,
				};
			case gse::gpu::barrier_scope::transfer_to_host:
				return {
					.src_stages = ps::transfer,
					.src_access = ac::transfer_write,
					.dst_stages = ps::host,
					.dst_access = ac::host_read,
				};
			case gse::gpu::barrier_scope::transfer_to_transfer:
				return {
					.src_stages = ps::copy,
					.src_access = ac::transfer_write,
					.dst_stages = ps::copy,
					.dst_access = ac::transfer_write,
				};
		}
		return {};
	}
}

gse::gpu::compute_queue::compute_queue(
	vulkan::compute_context&& ctx,
	descriptor_heap* heap,
	const float timestamp_period
) : m_ctx(std::move(ctx)),
    m_descriptor_heap(heap),
    m_timestamp_period(timestamp_period) {}

auto gse::gpu::compute_queue::wait() const -> void {
	m_ctx.wait_fence();
}

auto gse::gpu::compute_queue::is_complete() const -> bool {
	return m_ctx.is_fence_signaled();
}

auto gse::gpu::compute_queue::begin() const -> void {
	const auto cmd = m_ctx.command_buffer_handle();
	vulkan::commands(cmd).reset();
	vulkan::commands(cmd).begin();
	vulkan::commands(cmd).reset_query_pool(m_ctx.query_pool_handle(), 0, timing_slot_count);
	if (m_descriptor_heap) {
		m_descriptor_heap->bind_buffer(cmd);
	}
}

auto gse::gpu::compute_queue::submit() -> void {
	const auto cmd = m_ctx.command_buffer_handle();
	vulkan::commands(cmd).end();

	++m_timeline_value;

	m_ctx.reset_fence();
	m_ctx.submit(m_timeline_value, gpu::pipeline_stage_flag::all_commands);
	++m_frame_count;
}

auto gse::gpu::compute_queue::semaphore_state() const -> compute_semaphore_state {
	return compute_semaphore_state(&m_ctx.raii_timeline(), m_timeline_value, gpu::pipeline_stage_flag::compute_shader);
}

auto gse::gpu::compute_queue::bind_pipeline(const pipeline& p) const -> void {
	const auto cmd = m_ctx.command_buffer_handle();
	vulkan::commands(cmd).bind_pipeline(bind_point::compute, p.handle());
}

auto gse::gpu::compute_queue::bind_descriptors(const pipeline& p, const descriptor_region& region) const -> void {
	const auto cmd = m_ctx.command_buffer_handle();
	region.heap->bind(cmd, bind_point::compute, p.layout(), 0, region);
}

auto gse::gpu::compute_queue::dispatch(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) const -> void {
	const auto cmd = m_ctx.command_buffer_handle();
	vulkan::commands(cmd).dispatch(x, y, z);
}

auto gse::gpu::compute_queue::dispatch_indirect(const vulkan::basic_buffer<vulkan::device>& buf, const std::size_t offset) const -> void {
	const auto cmd = m_ctx.command_buffer_handle();
	vulkan::commands(cmd).dispatch_indirect(buf.buffer, static_cast<gpu::device_size>(offset));
}

auto gse::gpu::compute_queue::barrier(const barrier_scope scope) const -> void {
	const auto b = barrier_scope_to_gpu(scope);
	const gpu::dependency_info dep{ .memory_barriers = std::span(&b, 1) };
	const auto cmd = m_ctx.command_buffer_handle();
	vulkan::commands(cmd).pipeline_barrier(dep);
}

auto gse::gpu::compute_queue::barriers(const std::span<const barrier_scope> scopes) const -> void {
	std::vector<gpu::memory_barrier> barriers;
	barriers.reserve(scopes.size());
	for (const auto s : scopes) {
		barriers.push_back(barrier_scope_to_gpu(s));
	}
	const gpu::dependency_info dep{ .memory_barriers = barriers };
	const auto cmd = m_ctx.command_buffer_handle();
	vulkan::commands(cmd).pipeline_barrier(dep);
}

auto gse::gpu::compute_queue::copy_buffer(const buffer_copy& copy) const -> void {
	const auto cmd = m_ctx.command_buffer_handle();
	vulkan::commands(cmd).copy_buffer(
		copy.src.buffer,
		copy.dst.buffer,
		buffer_copy_region{
			.src_offset = copy.src_offset,
			.dst_offset = copy.dst_offset,
			.size = copy.size,
		}
	);
}

auto gse::gpu::compute_queue::push(const pipeline& p, const cached_push_constants& cache) const -> void {
	cache.replay(m_ctx.command_buffer_handle(), p.layout());
}

auto gse::gpu::compute_queue::begin_timing() const -> void {
	const auto cmd = m_ctx.command_buffer_handle();
	vulkan::commands(cmd).write_timestamp(pipeline_stage_flag::top_of_pipe, m_ctx.query_pool_handle(), 0);
}

auto gse::gpu::compute_queue::end_timing() const -> void {
	const auto cmd = m_ctx.command_buffer_handle();
	vulkan::commands(cmd).write_timestamp(pipeline_stage_flag::compute_shader, m_ctx.query_pool_handle(), 1);
}

auto gse::gpu::compute_queue::mark_timing(const std::uint32_t slot) const -> void {
	assert(slot < timing_slot_count, std::source_location::current(), "mark_timing slot out of range");
	const auto cmd = m_ctx.command_buffer_handle();
	vulkan::commands(cmd).write_timestamp(pipeline_stage_flag::compute_shader, m_ctx.query_pool_handle(), slot);
}

auto gse::gpu::compute_queue::read_timing() const -> float {
	return read_timing(0, 1);
}

auto gse::gpu::compute_queue::read_timing(const std::uint32_t start_slot, const std::uint32_t end_slot) const -> float {
	if (m_frame_count < 2) {
		return 0.0f;
	}
	assert(start_slot < timing_slot_count && end_slot < timing_slot_count, std::source_location::current(), "read_timing slot out of range");
	if (const auto results = m_ctx.read_timestamps(start_slot, end_slot); results) {
		const auto [start_ts, end_ts] = *results;
		return static_cast<float>(end_ts - start_ts) * m_timestamp_period * 1e-6f;
	}
	return 0.0f;
}

auto gse::gpu::compute_queue::native_command_buffer() const -> handle<command_buffer> {
	return m_ctx.command_buffer_handle();
}

gse::gpu::compute_queue::operator bool() const {
	return static_cast<bool>(m_ctx);
}

auto gse::gpu::compute_queue::create(gpu::device& dev) -> compute_queue {
	auto ctx = vulkan::compute_context::create(
		dev.vulkan_device(),
		dev.vulkan_queue(),
		compute_queue::timing_slot_count
	);

	return compute_queue(
		std::move(ctx),
		&dev.descriptor_heap(),
		dev.timestamp_period()
	);
}
