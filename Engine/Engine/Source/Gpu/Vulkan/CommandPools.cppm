export module gse.gpu:vulkan_command_pools;

import std;
import vulkan;

import :handles;
import :types;
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
			std::uint32_t graphics_family,
			std::uint32_t compute_family
		) -> command;

		[[nodiscard]] auto graphics_family_index(
		) const -> std::uint32_t;

		[[nodiscard]] auto compute_family_index(
		) const -> std::uint32_t;

		[[nodiscard]] auto frame_command_buffer(
			gpu::queue_type queue,
			std::uint32_t frame_index
		) const -> gpu::handle<command_buffer>;

	private:
		struct family_pool {
			vk::raii::CommandPool pool = nullptr;
			std::vector<vk::raii::CommandBuffer> buffers;
		};

		command(
			family_pool graphics,
			family_pool compute,
			std::uint32_t graphics_family,
			std::uint32_t compute_family
		);

		static auto make_primary_pool(
			const device& device_data,
			std::uint32_t family,
			std::string_view label
		) -> family_pool;

		family_pool m_graphics_pool;
		family_pool m_compute_pool;
		std::uint32_t m_graphics_family = 0;
		std::uint32_t m_compute_family = 0;
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
			std::uint32_t compute_family,
			std::size_t worker_count,
			std::size_t secondaries_per_pool = 32
		) -> worker_command_pools;

		auto reset_frame(
			std::uint32_t frame_index
		) -> void;

		[[nodiscard]] auto acquire_secondary(
			gpu::queue_type queue,
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

		struct family_pools {
			std::vector<per_frame_resource<pool_slot>> per_worker;
		};

		worker_command_pools(
			family_pools graphics,
			family_pools compute,
			std::uint32_t graphics_family,
			std::uint32_t compute_family
		);

		static auto build_family_pools(
			const device& device_data,
			std::uint32_t family,
			std::string_view label,
			std::size_t worker_count,
			std::size_t secondaries_per_pool
		) -> family_pools;

		family_pools m_graphics;
		family_pools m_compute;
		std::uint32_t m_graphics_family = 0;
		std::uint32_t m_compute_family = 0;
	};
}

gse::vulkan::command::command(family_pool graphics, family_pool compute, const std::uint32_t graphics_family, const std::uint32_t compute_family)
	: m_graphics_pool(std::move(graphics)),
	  m_compute_pool(std::move(compute)),
	  m_graphics_family(graphics_family),
	  m_compute_family(compute_family) {}

auto gse::vulkan::command::make_primary_pool(const device& device_data, const std::uint32_t family, const std::string_view label) -> family_pool {
	const vk::CommandPoolCreateInfo pool_info{
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = family,
	};
	vk::raii::CommandPool pool = device_data.raii_device().createCommandPool(pool_info);

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
		.commandBufferCount = max_frames_in_flight,
	};

	std::vector<vk::raii::CommandBuffer> buffers = device_data.raii_device().allocateCommandBuffers(alloc_info);

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

auto gse::vulkan::command::create(const device& device_data, const std::uint32_t graphics_family, const std::uint32_t compute_family) -> command {
	auto graphics_pool = make_primary_pool(device_data, graphics_family, "graphics");
	auto compute_pool = (graphics_family == compute_family)
		? family_pool{}
		: make_primary_pool(device_data, compute_family, "compute");

	log::println(log::category::vulkan, "Primary command pools created (graphics_family={}, compute_family={}, distinct={})",
		graphics_family, compute_family, graphics_family != compute_family);

	return command(std::move(graphics_pool), std::move(compute_pool), graphics_family, compute_family);
}

auto gse::vulkan::command::graphics_family_index() const -> std::uint32_t {
	return m_graphics_family;
}

auto gse::vulkan::command::compute_family_index() const -> std::uint32_t {
	return m_compute_family;
}

auto gse::vulkan::command::frame_command_buffer(const gpu::queue_type queue, const std::uint32_t frame_index) const -> gpu::handle<command_buffer> {
	const auto& pool = (queue == gpu::queue_type::compute && m_graphics_family != m_compute_family)
		? m_compute_pool
		: m_graphics_pool;
	return std::bit_cast<gpu::handle<command_buffer>>(*pool.buffers[frame_index]);
}

gse::vulkan::worker_command_pools::worker_command_pools(family_pools graphics, family_pools compute, const std::uint32_t graphics_family, const std::uint32_t compute_family)
	: m_graphics(std::move(graphics)),
	  m_compute(std::move(compute)),
	  m_graphics_family(graphics_family),
	  m_compute_family(compute_family) {}

gse::vulkan::worker_command_pools::~worker_command_pools() = default;

auto gse::vulkan::worker_command_pools::build_family_pools(const device& device_data, const std::uint32_t family, const std::string_view label, const std::size_t worker_count, const std::size_t secondaries_per_pool) -> family_pools {
	auto build_slot = [&](const std::size_t worker, const std::uint32_t frame) -> pool_slot {
		const vk::CommandPoolCreateInfo pool_info{
			.flags = vk::CommandPoolCreateFlagBits::eTransient,
			.queueFamilyIndex = family,
		};

		vk::raii::CommandPool pool = device_data.raii_device().createCommandPool(pool_info);

		const std::string pool_name = std::format("Worker {} Frame {} Command Pool ({})", worker, frame, label);
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
			const std::string buffer_name = std::format("Worker {} Frame {} Secondary {} ({})", worker, frame, i, label);
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
		auto frame_0 = build_slot(worker, 0);
		auto frame_1 = build_slot(worker, 1);
		pools.emplace_back(std::move(frame_0), std::move(frame_1));
	}

	return family_pools{ .per_worker = std::move(pools) };
}

auto gse::vulkan::worker_command_pools::create(const device& device_data, const std::uint32_t graphics_family, const std::uint32_t compute_family, const std::size_t worker_count, const std::size_t secondaries_per_pool) -> worker_command_pools {
	static_assert(max_frames_in_flight == 2, "worker_command_pools::create assumes per_frame_resource default of 2 frames");

	auto graphics_pools = build_family_pools(device_data, graphics_family, "graphics", worker_count, secondaries_per_pool);
	auto compute_pools = (graphics_family == compute_family)
		? family_pools{}
		: build_family_pools(device_data, compute_family, "compute", worker_count, secondaries_per_pool);

	log::println(
		log::category::vulkan,
		"Worker Command Pools Created (workers: {}, secondaries/pool: {}, graphics_family: {}, compute_family: {}, distinct: {})",
		worker_count,
		secondaries_per_pool,
		graphics_family,
		compute_family,
		graphics_family != compute_family
	);

	return worker_command_pools(std::move(graphics_pools), std::move(compute_pools), graphics_family, compute_family);
}

auto gse::vulkan::worker_command_pools::reset_frame(const std::uint32_t frame_index) -> void {
	auto reset_family = [frame_index](family_pools& fp) {
		for (auto& worker_pools : fp.per_worker) {
			auto& slot = worker_pools[frame_index];
			slot.pool.reset();
			slot.used = 0;
		}
	};
	reset_family(m_graphics);
	if (m_graphics_family != m_compute_family) {
		reset_family(m_compute);
	}
}

auto gse::vulkan::worker_command_pools::acquire_secondary(const gpu::queue_type queue, const std::size_t worker_index, const std::uint32_t frame_index) -> vk::CommandBuffer {
	auto& family = (queue == gpu::queue_type::compute && m_graphics_family != m_compute_family) ? m_compute : m_graphics;

	assert(
		worker_index < family.per_worker.size(),
		"Worker index {} out of bounds (worker_count: {})",
		worker_index,
		family.per_worker.size()
	);

	auto& slot = family.per_worker[worker_index][frame_index];

	assert(
		slot.used < slot.secondaries.size(),
		"Worker command pool exhausted (worker: {}, frame: {}, capacity: {})",
		worker_index,
		frame_index,
		slot.secondaries.size()
	);

	return *slot.secondaries[slot.used++];
}

auto gse::vulkan::worker_command_pools::worker_count() const -> std::size_t {
	return m_graphics.per_worker.size();
}

