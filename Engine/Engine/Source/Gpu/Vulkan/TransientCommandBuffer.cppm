export module gse.gpu:vulkan_transient_command_buffer;

import std;
import vulkan;

import :handles;
import :vulkan_commands;

import gse.core;

export namespace gse::vulkan {
    class transient_command_buffer final : public non_copyable {
    public:
        transient_command_buffer(
        ) = default;

        explicit transient_command_buffer(
            vk::raii::CommandBuffer&& cmd
        );

        ~transient_command_buffer(
        ) = default;

        transient_command_buffer(
            transient_command_buffer&&
        ) noexcept = default;

        auto operator=(
            transient_command_buffer&&
        ) noexcept -> transient_command_buffer& = default;

        [[nodiscard]] auto handle(
        ) const -> gpu::handle<command_buffer>;

        auto begin_one_time(
        ) -> void;

        auto end(
        ) -> void;

        explicit operator bool(
        ) const;

    private:
        vk::raii::CommandBuffer m_cmd{ nullptr };
    };
}

gse::vulkan::transient_command_buffer::transient_command_buffer(vk::raii::CommandBuffer&& cmd)
    : m_cmd(std::move(cmd)) {}

auto gse::vulkan::transient_command_buffer::handle() const -> gpu::handle<command_buffer> {
    return std::bit_cast<gpu::handle<command_buffer>>(*m_cmd);
}

auto gse::vulkan::transient_command_buffer::begin_one_time() -> void {
    constexpr vk::CommandBufferBeginInfo begin_info{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    };
    m_cmd.begin(begin_info);
}

auto gse::vulkan::transient_command_buffer::end() -> void {
    m_cmd.end();
}

gse::vulkan::transient_command_buffer::operator bool() const {
    return *m_cmd != nullptr;
}
