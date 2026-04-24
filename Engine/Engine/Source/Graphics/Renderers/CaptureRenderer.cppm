export module gse.graphics:capture_renderer;

import std;

import gse.os;
import gse.gpu;
import gse.core;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.time;

import :ui_renderer;
import :capture_ring;
import :mp4_muxer;

export namespace gse::renderer::capture {
    struct pending_screenshot {
        gpu::buffer staging;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        bool pending = false;
    };

    struct screenshot_request {};
    struct save_clip_request {};

    struct state {
        actions::handle screenshot_action;
        actions::handle save_clip_action;
    };

    struct system {
        struct resources {
            gpu::pipeline convert_pipeline;
            resource::handle<shader> convert_shader;
            per_frame_resource<gpu::descriptor_region> convert_descriptors;
            per_frame_resource<gpu::image> rgba_captures;
            per_frame_resource<gpu::image> y_planes;
            per_frame_resource<gpu::image> uv_planes;
            gpu::sampler capture_sampler;
            bool encode_active = false;
        };

        struct frame_data {
            per_frame_resource<pending_screenshot> screenshots;
            bool screenshot_requested = false;
            std::unique_ptr<std::atomic<bool>> write_in_progress = std::make_unique<std::atomic<bool>>(false);
            std::unique_ptr<std::atomic<bool>> clip_save_in_progress = std::make_unique<std::atomic<bool>>(false);
            gpu::video_encoder encoder;
            ring clip_ring;
            time ring_budget = seconds(30.f);
            bool first_ring_push_logged = false;
        };

        static auto initialize(
            const init_context& phase,
            resources& r,
            frame_data& fd,
            state& s
        ) -> void;

        static auto update(
            const update_context& ctx,
            state& s
        ) -> async::task<>;

        static auto frame(
            const frame_context& ctx,
            const resources& r,
            frame_data& fd,
            const state& s
        ) -> async::task<>;
    };
}
