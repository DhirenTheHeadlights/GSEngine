export module gse.gpu:vulkan_transient_command_pool;

import std;
import vulkan;

import :vulkan_device;
import :vulkan_transient_command_buffer;

import gse.core;

export namespace gse::vulkan {
    class transient_command_pool final : public non_copyable {
    public:
        transient_command_pool(
        ) = default;

        ~transient_command_pool(
        ) = default;

        transient_command_pool(
            transient_command_pool&&
        ) noexcept = default;

        auto operator=(
            transient_command_pool&&
        ) noexcept -> transient_command_pool& = default;

        [[nodiscard]] static auto create(
            const device& device,
            std::uint32_t family
        ) -> transient_command_pool;

        [[nodiscard]] auto allocate_primary(
            const device& device
        ) -> transient_command_buffer;

    private:
        explicit transient_command_pool(
            vk::raii::CommandPool&& pool
        );

        vk::raii::CommandPool m_pool{ nullptr };
    };
}

gse::vulkan::transient_command_pool::transient_command_pool(vk::raii::CommandPool&& pool)
    : m_pool(std::move(pool)) {}

auto gse::vulkan::transient_command_pool::create(const device& device, const std::uint32_t family) -> transient_command_pool {
    const vk::CommandPoolCreateInfo pool_info{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient,
        .queueFamilyIndex = family,
    };
    return transient_command_pool(device.raii_device().createCommandPool(pool_info));
}

auto gse::vulkan::transient_command_pool::allocate_primary(const device& device) -> transient_command_buffer {
    const vk::CommandBufferAllocateInfo alloc_info{
        .commandPool = *m_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };
    auto buffers = device.raii_device().allocateCommandBuffers(alloc_info);
    return transient_command_buffer(std::move(buffers[0]));
}
