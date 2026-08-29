export module gse.vulkan:command_pools;

import std;
import vulkan;

import gse.gpu_backend;
import :types;
import :device;
import :sync;

import gse.assert;
import gse.concurrency;
import gse.core;
import gse.log;

export namespace gse::vulkan {
	class command : public non_copyable {
	public:
		~command() = default;

		command(
			command&&
		) noexcept = default;

		auto operator=(
			command&&
		) noexcept -> command& = default;

		[[nodiscard]]
		static auto create(
			const device& device_data,
			const std::array<std::uint32_t, gpu::queue_type_count>& queue_families
		) -> command;

		[[nodiscard]] auto family_index(
			gpu::queue_type queue
		) const -> std::uint32_t;

		[[nodiscard]]
		auto frame_command_buffer(
			gpu::queue_type queue,
			std::uint32_t frame_index
		) const -> gpu::command_buffer_handle;

	private:
		struct family_pool {
			vk::raii::CommandPool pool = nullptr;
			std::vector<vk::raii::CommandBuffer> buffers;
		};

		command(
			std::array<family_pool, gpu::queue_type_count> pools,
			std::array<std::uint32_t, gpu::queue_type_count> families
		);

		static auto make_primary_pool(
			const device& device_data,
			std::uint32_t family,
			std::string_view label
		) -> family_pool;

		std::array<family_pool, gpu::queue_type_count> m_pools;
		std::array<std::uint32_t, gpu::queue_type_count> m_families{};
	};

	class worker_command_pools : public non_copyable {
	public:
		~worker_command_pools();

		worker_command_pools(
			worker_command_pools&&
		) noexcept = default;

		auto operator=(
			worker_command_pools&&
		) noexcept -> worker_command_pools& = default;

		[[nodiscard]]
		static auto create(
			const device& device_data,
			const std::array<std::uint32_t, gpu::queue_type_count>& queue_families,
			std::size_t worker_count,
			std::size_t buffers_per_pool = 128
		) -> worker_command_pools;

		auto reset_frame(
			std::uint32_t frame_index
		) -> void;

		[[nodiscard]]
		auto acquire_command_buffer(
			const device& device_data,
			gpu::queue_type queue,
			std::size_t worker_index,
			std::uint32_t frame_index
		) -> gpu::command_buffer_handle;

		[[nodiscard]] auto worker_count() const -> std::size_t;

	private:
		struct pool_slot {
			vk::raii::CommandPool pool = nullptr;
			std::vector<vk::raii::CommandBuffer> buffers;
			std::size_t used = 0;
			std::size_t worker = 0;
			std::uint32_t frame = 0;
			std::string label;
		};

		struct family_pools {
			std::vector<per_frame_resource<pool_slot>> per_worker;
		};

		worker_command_pools(
			std::array<family_pools, gpu::queue_type_count> pools,
			std::array<std::uint32_t, gpu::queue_type_count> families
		);

		static auto build_family_pools(
			const device& device_data,
			std::uint32_t family,
			std::string_view label,
			std::size_t worker_count,
			std::size_t buffers_per_pool
		) -> family_pools;

		static auto allocate_into_slot(
			const device& device_data,
			pool_slot& slot,
			std::size_t count
		) -> void;

		std::array<family_pools, gpu::queue_type_count> m_pools;
		std::array<std::uint32_t, gpu::queue_type_count> m_families{};
	};
}

gse::vulkan::command::command(std::array<family_pool, gpu::queue_type_count> pools, std::array<std::uint32_t, gpu::queue_type_count> families)
	: m_pools(std::move(pools)), m_families(families) {
}

auto gse::vulkan::command::make_primary_pool(const device& device_data, const std::uint32_t family, const std::string_view label) -> family_pool {
	const vk::CommandPoolCreateInfo pool_info{
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = family,
	};
	auto [pool_result, pool] = device_data.raii_device().createCommandPool(pool_info);
	assert(pool_result == vk::Result::eSuccess, "failed to create command pool: {}", vk::to_string(pool_result));

	const std::string pool_name = std::format("Primary Command Pool ({})", label);
	const vk::DebugUtilsObjectNameInfoEXT pool_name_info{
		.objectType = vk::ObjectType::eCommandPool,
		.objectHandle = std::bit_cast<std::uint64_t>(*pool),
		.pObjectName = pool_name.c_str(),
	};
	device_data.raii_device().setDebugUtilsObjectNameEXT(pool_name_info);

	const vk::CommandBufferAllocateInfo alloc_info{
		.commandPool = *pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = gpu::max_frames_in_flight,
	};

	auto [buffers_result, buffers] = device_data.raii_device().allocateCommandBuffers(alloc_info);
	assert(buffers_result == vk::Result::eSuccess, "failed to allocate command buffers: {}", vk::to_string(buffers_result));

	for (std::uint32_t i = 0; i < buffers.size(); ++i) {
		const std::string name = std::format("Primary Command Buffer ({}) frame={}", label, i);
		const vk::DebugUtilsObjectNameInfoEXT name_info{
			.objectType = vk::ObjectType::eCommandBuffer,
			.objectHandle = std::bit_cast<std::uint64_t>(*buffers[i]),
			.pObjectName = name.c_str(),
		};
		device_data.raii_device().setDebugUtilsObjectNameEXT(name_info);
	}

	return family_pool{
		.pool = std::move(pool),
		.buffers = std::move(buffers),
	};
}

auto gse::vulkan::command::create(const device& device_data, const std::array<std::uint32_t, gpu::queue_type_count>& queue_families) -> command {
	std::array<family_pool, gpu::queue_type_count> pools;
	pools[0] = make_primary_pool(device_data, queue_families[0], std::format("queue_{}", 0));
	for (std::size_t i = 1; i < gpu::queue_type_count; ++i) {
		if (queue_families[i] == queue_families[0]) {
			continue;
		}
		bool duplicate = false;
		for (std::size_t j = 1; j < i; ++j) {
			if (queue_families[j] == queue_families[i]) {
				duplicate = true;
				break;
			}
		}
		if (!duplicate) {
			pools[i] = make_primary_pool(device_data, queue_families[i], std::format("queue_{}", i));
		}
	}

	log::println(log::category::vulkan, "Primary command pools created");

	return command(std::move(pools), queue_families);
}

auto gse::vulkan::command::family_index(const gpu::queue_type queue) const -> std::uint32_t {
	return m_families[static_cast<std::size_t>(queue)];
}

auto gse::vulkan::command::frame_command_buffer(const gpu::queue_type queue, const std::uint32_t frame_index) const -> gpu::command_buffer_handle {
	const auto idx = static_cast<std::size_t>(queue);
	const auto& pool = *m_pools[idx].pool ? m_pools[idx] : m_pools[0];
	return std::bit_cast<gpu::command_buffer_handle>(*pool.buffers[frame_index]);
}

gse::vulkan::worker_command_pools::worker_command_pools(std::array<family_pools, gpu::queue_type_count> pools, std::array<std::uint32_t, gpu::queue_type_count> families)
	: m_pools(std::move(pools)), m_families(families) {
}

gse::vulkan::worker_command_pools::~worker_command_pools() = default;

auto gse::vulkan::worker_command_pools::allocate_into_slot(const device& device_data, pool_slot& slot, const std::size_t count) -> void {
	const auto first = slot.buffers.size();

	const vk::CommandBufferAllocateInfo alloc_info{
		.commandPool = *slot.pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = static_cast<std::uint32_t>(count),
	};

	auto [buffers_result, buffers] = device_data.raii_device().allocateCommandBuffers(alloc_info);
	assert(buffers_result == vk::Result::eSuccess, "failed to allocate command buffers: {}", vk::to_string(buffers_result));

	for (std::size_t i = 0; i < buffers.size(); ++i) {
		const std::string buffer_name = std::format("Worker {} Frame {} Buffer {} ({})", slot.worker, slot.frame, first + i, slot.label);
		const vk::DebugUtilsObjectNameInfoEXT buffer_name_info{
			.objectType = vk::ObjectType::eCommandBuffer,
			.objectHandle = std::bit_cast<std::uint64_t>(*buffers[i]),
			.pObjectName = buffer_name.c_str(),
		};
		device_data.raii_device().setDebugUtilsObjectNameEXT(buffer_name_info);
	}

	slot.buffers.reserve(first + buffers.size());
	slot.buffers.insert(
		slot.buffers.end(),
		std::make_move_iterator(buffers.begin()),
		std::make_move_iterator(buffers.end())
	);
}

auto gse::vulkan::worker_command_pools::build_family_pools(const device& device_data, const std::uint32_t family, const std::string_view label, const std::size_t worker_count, const std::size_t buffers_per_pool) -> family_pools {
	auto build_slot = [&](const std::size_t worker, const std::uint32_t frame) -> pool_slot {
		const vk::CommandPoolCreateInfo pool_info{
			.flags = vk::CommandPoolCreateFlagBits::eTransient,
			.queueFamilyIndex = family,
		};

		auto [pool_result, pool] = device_data.raii_device().createCommandPool(pool_info);
		assert(pool_result == vk::Result::eSuccess, "failed to create command pool: {}", vk::to_string(pool_result));

		const std::string pool_name = std::format("Worker {} Frame {} Command Pool ({})", worker, frame, label);
		const vk::DebugUtilsObjectNameInfoEXT pool_name_info{
			.objectType = vk::ObjectType::eCommandPool,
			.objectHandle = std::bit_cast<std::uint64_t>(*pool),
			.pObjectName = pool_name.c_str(),
		};
		device_data.raii_device().setDebugUtilsObjectNameEXT(pool_name_info);

		pool_slot slot{
			.pool = std::move(pool),
			.used = 0,
			.worker = worker,
			.frame = frame,
			.label = std::string(label),
		};

		allocate_into_slot(device_data, slot, buffers_per_pool);

		return slot;
	};

	std::vector<per_frame_resource<pool_slot>> pools;
	pools.reserve(worker_count);

	for (std::size_t worker = 0; worker < worker_count; ++worker) {
		auto frame_0 = build_slot(worker, 0);
		auto frame_1 = build_slot(worker, 1);
		pools.emplace_back(std::move(frame_0), std::move(frame_1));
	}

	return family_pools{
		.per_worker = std::move(pools)
	};
}

auto gse::vulkan::worker_command_pools::create(const device& device_data, const std::array<std::uint32_t, gpu::queue_type_count>& queue_families, const std::size_t worker_count, const std::size_t buffers_per_pool) -> worker_command_pools {
	static_assert(
		gpu::max_frames_in_flight == 2,
		"worker_command_pools::create assumes per_frame_resource default of 2 frames"
	);

	std::array<family_pools, gpu::queue_type_count> pools;
	pools[0] = build_family_pools(
		device_data,
		queue_families[0],
		std::format("queue_{}", 0),
		worker_count,
		buffers_per_pool
	);
	for (std::size_t i = 1; i < gpu::queue_type_count; ++i) {
		bool duplicate = false;
		for (std::size_t j = 0; j < i; ++j) {
			if (queue_families[j] == queue_families[i]) {
				duplicate = true;
				break;
			}
		}
		if (!duplicate) {
			pools[i] = build_family_pools(
				device_data,
				queue_families[i],
				std::format("queue_{}", i),
				worker_count,
				buffers_per_pool
			);
		}
	}

	log::println(
		log::category::vulkan,
		"Worker Command Pools Created (workers: {}, buffers/pool: {})",
		worker_count,
		buffers_per_pool
	);

	return worker_command_pools(std::move(pools), queue_families);
}

auto gse::vulkan::worker_command_pools::reset_frame(const std::uint32_t frame_index) -> void {
	for (auto& fp : m_pools) {
		for (auto& worker_pools : fp.per_worker) {
			auto& slot = worker_pools[frame_index];
			slot.pool.reset();
			slot.used = 0;
		}
	}
}

auto gse::vulkan::worker_command_pools::acquire_command_buffer(const device& device_data, const gpu::queue_type queue, const std::size_t worker_index, const std::uint32_t frame_index) -> gpu::command_buffer_handle {
	const auto idx = static_cast<std::size_t>(queue);
	auto& family = m_pools[idx].per_worker.empty() ? m_pools[0] : m_pools[idx];

	assert(
		worker_index < family.per_worker.size(),
		"Worker index {} out of bounds (worker_count: {})",
		worker_index,
		family.per_worker.size()
	);

	auto& slot = family.per_worker[worker_index][frame_index];

	if (slot.used == slot.buffers.size()) {
		allocate_into_slot(device_data, slot, slot.buffers.size());
		log::println(
			log::level::debug,
			log::category::vulkan,
			"Worker command pool grown (worker: {}, frame: {}, capacity: {})",
			slot.worker,
			slot.frame,
			slot.buffers.size()
		);
	}

	return std::bit_cast<gpu::command_buffer_handle>(*slot.buffers[slot.used++]);
}

auto gse::vulkan::worker_command_pools::worker_count() const -> std::size_t {
	return m_pools[0].per_worker.size();
}
