export module gse.gpu:command_pools;

import std;
import vulkan;

import :vulkan_runtime;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;

export namespace gse::vulkan {
	class worker_command_pools : public non_copyable {
	public:
		~worker_command_pools() override;

		worker_command_pools(
			worker_command_pools&&
		) noexcept = default;

		auto operator=(
			worker_command_pools&&
		) noexcept -> worker_command_pools& = default;

		[[nodiscard]] static auto create(
			const device_config& device_data,
			std::uint32_t graphics_family,
			std::size_t worker_count,
			std::size_t secondaries_per_pool = 32
		) -> worker_command_pools;

		auto reset_frame(
			std::uint32_t frame_index
		) -> void;

		[[nodiscard]] auto acquire_secondary(
			std::size_t worker_index,
			std::uint32_t frame_index
		) -> vk::CommandBuffer;

		[[nodiscard]] auto worker_count(
		) const -> std::size_t;

	private:
		struct pool_slot {
			vk::raii::CommandPool pool = nullptr;
			std::vector<vk::raii::CommandBuffer> secondaries;
			std::size_t used = 0;
		};

		worker_command_pools(
			std::vector<per_frame_resource<pool_slot>>&& pools
		);

		std::vector<per_frame_resource<pool_slot>> m_pools;
	};
}

gse::vulkan::worker_command_pools::worker_command_pools(std::vector<per_frame_resource<pool_slot>>&& pools)
	: m_pools(std::move(pools)) {}

gse::vulkan::worker_command_pools::~worker_command_pools() {}

auto gse::vulkan::worker_command_pools::create(const device_config& device_data, const std::uint32_t graphics_family, const std::size_t worker_count, const std::size_t secondaries_per_pool) -> worker_command_pools {
	std::vector<per_frame_resource<pool_slot>> pools;
	pools.reserve(worker_count);

	for (std::size_t w = 0; w < worker_count; ++w) {
		per_frame_resource<pool_slot> frames{ pool_slot{}, pool_slot{} };
		for (std::size_t f = 0; f < per_frame_resource<pool_slot>::frames_in_flight; ++f) {
			auto& slot = frames[f];
			slot.pool = device_data.device.createCommandPool({
				.flags = vk::CommandPoolCreateFlagBits::eTransient,
				.queueFamilyIndex = graphics_family
			});
			slot.secondaries = device_data.device.allocateCommandBuffers({
				.commandPool = *slot.pool,
				.level = vk::CommandBufferLevel::eSecondary,
				.commandBufferCount = static_cast<std::uint32_t>(secondaries_per_pool)
			});
		}
		pools.push_back(std::move(frames));
	}

	return worker_command_pools{ std::move(pools) };
}

auto gse::vulkan::worker_command_pools::reset_frame(const std::uint32_t frame_index) -> void {
	for (auto& frames : m_pools) {
		auto& slot = frames[frame_index];
		slot.pool.reset();
		slot.used = 0;
	}
}

auto gse::vulkan::worker_command_pools::acquire_secondary(const std::size_t worker_index, const std::uint32_t frame_index) -> vk::CommandBuffer {
	assert(worker_index < m_pools.size(), std::source_location::current(), "worker_command_pools: worker_index out of range");
	auto& slot = m_pools[worker_index][frame_index];
	assert(slot.used < slot.secondaries.size(), std::source_location::current(), "worker_command_pools: per-pool secondary count exceeded; increase secondaries_per_pool");
	return *slot.secondaries[slot.used++];
}

auto gse::vulkan::worker_command_pools::worker_count() const -> std::size_t {
	return m_pools.size();
}
