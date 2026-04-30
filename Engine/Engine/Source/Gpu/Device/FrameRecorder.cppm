export module gse.gpu:frame_recorder;

import std;

import :handles;
import :vulkan_commands;

import gse.core;

export namespace gse::gpu {
    using frame_record_fn = std::move_only_function<void(handle<vulkan::command_buffer>)>;

    class frame_recorder final : public non_copyable {
    public:
        frame_recorder(
        ) = default;

        ~frame_recorder(
        );

        frame_recorder(
            frame_recorder&&
        ) noexcept = default;

        auto operator=(
            frame_recorder&&
        ) noexcept -> frame_recorder& = default;

        auto pre_frame(
            frame_record_fn commands
        ) -> void;

        auto post_frame(
            frame_record_fn commands
        ) -> void;

        auto run_pre_frame(
            handle<vulkan::command_buffer> cmd
        ) -> void;

        auto run_post_frame(
            handle<vulkan::command_buffer> cmd
        ) -> void;

        auto clear(
        ) -> void;

    private:
        std::vector<frame_record_fn> m_pre;
        std::vector<frame_record_fn> m_post;
    };
}

gse::gpu::frame_recorder::~frame_recorder() = default;

auto gse::gpu::frame_recorder::pre_frame(frame_record_fn commands) -> void {
    m_pre.push_back(std::move(commands));
}

auto gse::gpu::frame_recorder::post_frame(frame_record_fn commands) -> void {
    m_post.push_back(std::move(commands));
}

auto gse::gpu::frame_recorder::run_pre_frame(handle<vulkan::command_buffer> cmd) -> void {
    for (auto& fn : m_pre) {
        fn(cmd);
    }
}

auto gse::gpu::frame_recorder::run_post_frame(handle<vulkan::command_buffer> cmd) -> void {
    for (auto& fn : m_post) {
        fn(cmd);
    }
}

auto gse::gpu::frame_recorder::clear() -> void {
    m_pre.clear();
    m_post.clear();
}
