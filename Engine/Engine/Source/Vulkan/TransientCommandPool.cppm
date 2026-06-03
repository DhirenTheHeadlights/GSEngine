export module gse.vulkan:transient_command_pool;

import std;
import vulkan;

import :handles;
import :device;

import gse.core;

export namespace gse::vulkan {
	class transient_command_pool final : public non_copyable {
	public:
		transient_command_pool() {}
		~transient_command_pool() = default;

		transient_command_pool(
			transient_command_pool&&
		) noexcept = default;

		auto operator=(
			transient_command_pool&&
		) noexcept -> transient_command_pool& = default;

		[[nodiscard]]
		static auto create(
			const device& device,
			std::uint32_t family
		) -> transient_command_pool;

		[[nodiscard]] auto allocate_primary(
			const device& device
		) -> gpu::command_buffer_handle;

		auto try_reset(
			std::uint64_t queue_progress
		) -> void;

		auto mark_in_use_until(
			std::uint64_t value
		) -> void;

		auto reset_all() -> void;

	private:
		explicit transient_command_pool(
			vk::raii::CommandPool&& pool
		);

		static constexpr std::uint32_t allocation_batch_size = 8;

		vk::raii::CommandPool m_pool{ nullptr };
		std::vector<vk::raii::CommandBuffer> m_owned_cbs;
		std::size_t m_used = 0;
		std::uint64_t m_high_water_mark = 0;
	};
}

gse::vulkan::transient_command_pool::transient_command_pool(vk::raii::CommandPool&& pool) : m_pool(std::move(pool)) {
}

auto gse::vulkan::transient_command_pool::create(const device& device, const std::uint32_t family) -> transient_command_pool {
	const vk::CommandPoolCreateInfo pool_info{
		.flags = vk::CommandPoolCreateFlagBits::eTransient,
		.queueFamilyIndex = family,
	};
	return transient_command_pool(device.raii_device().createCommandPool(pool_info));
}

auto gse::vulkan::transient_command_pool::allocate_primary(const device& device) -> gpu::command_buffer_handle {
	if (m_used == m_owned_cbs.size()) {
		const vk::CommandBufferAllocateInfo alloc_info{
			.commandPool = *m_pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = allocation_batch_size,
		};
		auto fresh = device.raii_device().allocateCommandBuffers(alloc_info);
		for (auto& cb : fresh) {
			m_owned_cbs.push_back(std::move(cb));
		}
	}
	return std::bit_cast<gpu::command_buffer_handle>(*m_owned_cbs[m_used++]);
}

auto gse::vulkan::transient_command_pool::try_reset(const std::uint64_t queue_progress) -> void {
	if (m_used == 0) {
		return;
	}
	if (queue_progress < m_high_water_mark) {
		return;
	}
	m_pool.reset();
	m_used = 0;
}

auto gse::vulkan::transient_command_pool::mark_in_use_until(const std::uint64_t value) -> void {
	if (value > m_high_water_mark) {
		m_high_water_mark = value;
	}
}

auto gse::vulkan::transient_command_pool::reset_all() -> void {
	m_pool.reset();
	m_used = 0;
	m_high_water_mark = 0;
}
