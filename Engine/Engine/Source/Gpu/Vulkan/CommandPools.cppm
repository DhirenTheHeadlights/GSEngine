export module gse.gpu:vulkan_command_pools;

import std;
import vulkan;

import :handles;
import :vulkan_allocation;
import :vulkan_buffer;
import :vulkan_image;
import :vulkan_device;
import :vulkan_sync;

import gse.assert;
import gse.concurrency;
import gse.core;
import gse.log;

export namespace gse::vulkan {
	class command : public non_copyable {
	public:
		~command(
		) override = default;

		command(
			command&&
		) noexcept = default;

		auto operator=(
			command&&
		) noexcept -> command& = default;

		[[nodiscard]] static auto create(
			const device& device_data,
			std::uint32_t graphics_family
		) -> command;

		[[nodiscard]] auto graphics_family_index(
		) const -> std::uint32_t;

		[[nodiscard]] auto frame_command_buffer(
			std::uint32_t frame_index
		) const -> gpu::handle<command_buffer>;

	private:
		command(
			vk::raii::CommandPool&& pool,
			std::vector<vk::raii::CommandBuffer>&& buffers,
			std::uint32_t graphics_family
		);

		vk::raii::CommandPool m_pool;
		std::vector<vk::raii::CommandBuffer> m_buffers;
		std::uint32_t m_graphics_family = 0;
	};

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
			const device& device_data,
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

gse::vulkan::command::command(vk::raii::CommandPool&& pool, std::vector<vk::raii::CommandBuffer>&& buffers, const std::uint32_t graphics_family)
	: m_pool(std::move(pool)),
	  m_buffers(std::move(buffers)),
	  m_graphics_family(graphics_family) {}

auto gse::vulkan::command::create(const device& device_data, const std::uint32_t graphics_family) -> command {
	const vk::CommandPoolCreateInfo pool_info{
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = graphics_family,
	};

	vk::raii::CommandPool pool = device_data.raii_device().createCommandPool(pool_info);

	log::println(log::category::vulkan, "Command Pool Created Successfully!");

	constexpr auto pool_name = "Primary Command Pool";
	const vk::DebugUtilsObjectNameInfoEXT pool_name_info{
		.objectType = vk::ObjectType::eCommandPool,
		.objectHandle = std::bit_cast<std::uint64_t>(*pool),
		.pObjectName = pool_name,
	};
	device_data.raii_device().setDebugUtilsObjectNameEXT(pool_name_info);

	const vk::CommandBufferAllocateInfo alloc_info{
		.commandPool = *pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = max_frames_in_flight,
	};

	std::vector<vk::raii::CommandBuffer> buffers = device_data.raii_device().allocateCommandBuffers(alloc_info);

	log::println(log::category::vulkan, "Command Buffers Created Successfully!");

	for (std::uint32_t i = 0; i < buffers.size(); ++i) {
		const std::string name = "Primary Command Buffer " + std::to_string(i);
		const vk::DebugUtilsObjectNameInfoEXT name_info{
			.objectType = vk::ObjectType::eCommandBuffer,
			.objectHandle = std::bit_cast<std::uint64_t>(*buffers[i]),
			.pObjectName = name.c_str(),
		};
		device_data.raii_device().setDebugUtilsObjectNameEXT(name_info);
	}

	return command(std::move(pool), std::move(buffers), graphics_family);
}

auto gse::vulkan::command::graphics_family_index() const -> std::uint32_t {
	return m_graphics_family;
}

auto gse::vulkan::command::frame_command_buffer(const std::uint32_t frame_index) const -> gpu::handle<command_buffer> {
	return std::bit_cast<gpu::handle<command_buffer>>(*m_buffers[frame_index]);
}

gse::vulkan::worker_command_pools::worker_command_pools(std::vector<per_frame_resource<pool_slot>>&& pools)
	: m_pools(std::move(pools)) {}

gse::vulkan::worker_command_pools::~worker_command_pools() = default;

auto gse::vulkan::worker_command_pools::create(const device& device_data, const std::uint32_t graphics_family, const std::size_t worker_count, const std::size_t secondaries_per_pool) -> worker_command_pools {
	static_assert(max_frames_in_flight == 2, "worker_command_pools::create assumes per_frame_resource default of 2 frames");

	auto build_slot = [&](const std::size_t worker, const std::uint32_t frame) -> pool_slot {
		const vk::CommandPoolCreateInfo pool_info{
			.flags = vk::CommandPoolCreateFlagBits::eTransient,
			.queueFamilyIndex = graphics_family,
		};

		vk::raii::CommandPool pool = device_data.raii_device().createCommandPool(pool_info);

		const std::string pool_name = std::format("Worker {} Frame {} Command Pool", worker, frame);
		const vk::DebugUtilsObjectNameInfoEXT pool_name_info{
			.objectType = vk::ObjectType::eCommandPool,
			.objectHandle = std::bit_cast<std::uint64_t>(*pool),
			.pObjectName = pool_name.c_str(),
		};
		device_data.raii_device().setDebugUtilsObjectNameEXT(pool_name_info);

		const vk::CommandBufferAllocateInfo alloc_info{
			.commandPool = *pool,
			.level = vk::CommandBufferLevel::eSecondary,
			.commandBufferCount = static_cast<std::uint32_t>(secondaries_per_pool),
		};

		std::vector<vk::raii::CommandBuffer> secondaries =
			device_data.raii_device().allocateCommandBuffers(alloc_info);

		for (std::size_t i = 0; i < secondaries.size(); ++i) {
			const std::string buffer_name = std::format("Worker {} Frame {} Secondary {}", worker, frame, i);
			const vk::DebugUtilsObjectNameInfoEXT buffer_name_info{
				.objectType = vk::ObjectType::eCommandBuffer,
				.objectHandle = std::bit_cast<std::uint64_t>(*secondaries[i]),
				.pObjectName = buffer_name.c_str(),
			};
			device_data.raii_device().setDebugUtilsObjectNameEXT(buffer_name_info);
		}

		return pool_slot{
			.pool = std::move(pool),
			.secondaries = std::move(secondaries),
			.used = 0,
		};
	};

	std::vector<per_frame_resource<pool_slot>> pools;
	pools.reserve(worker_count);

	for (std::size_t worker = 0; worker < worker_count; ++worker) {
		pool_slot frame_0 = build_slot(worker, 0);
		pool_slot frame_1 = build_slot(worker, 1);
		pools.emplace_back(std::move(frame_0), std::move(frame_1));
	}

	log::println(
		log::category::vulkan,
		"Worker Command Pools Created (workers: {}, secondaries/pool: {})",
		worker_count,
		secondaries_per_pool
	);

	return worker_command_pools(std::move(pools));
}

auto gse::vulkan::worker_command_pools::reset_frame(const std::uint32_t frame_index) -> void {
	for (auto& worker_pools : m_pools) {
		auto& slot = worker_pools[frame_index];
		slot.pool.reset();
		slot.used = 0;
	}
}

auto gse::vulkan::worker_command_pools::acquire_secondary(const std::size_t worker_index, const std::uint32_t frame_index) -> vk::CommandBuffer {
	assert(
		worker_index < m_pools.size(),
		std::source_location::current(),
		"Worker index {} out of bounds (worker_count: {})",
		worker_index,
		m_pools.size()
	);

	auto& slot = m_pools[worker_index][frame_index];

	assert(
		slot.used < slot.secondaries.size(),
		std::source_location::current(),
		"Worker command pool exhausted (worker: {}, frame: {}, capacity: {})",
		worker_index,
		frame_index,
		slot.secondaries.size()
	);

	return *slot.secondaries[slot.used++];
}

auto gse::vulkan::worker_command_pools::worker_count() const -> std::size_t {
	return m_pools.size();
}

