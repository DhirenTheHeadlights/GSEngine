export module gse.gpu:vulkan_command_pools;

import std;
import vulkan;

import :handles;
import :vulkan_allocation;
import :vulkan_buffer;
import :vulkan_image;
import :vulkan_device;
import :vulkan_sync;

import gse.concurrency;
import gse.core;
import gse.log;

export namespace gse::vulkan {
	class command : public non_copyable {
	public:
		~command(
		) = default;

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

