export module gse.graphics:capture_renderer;

import std;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.time;
import gse.meta;

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

    struct system {
        struct settings {
            static constexpr std::string_view category = "Graphics";

            [[=gse::settings::describe<"Length of the rolling capture ring buffer. Saving a clip writes the most recent N seconds of frames.">{}, =gse::settings::range<seconds(5.f), seconds(120.f)>{}]]
            time ring_budget = seconds(30.f);
        };

        struct state {
            actions::handle screenshot_action;
            actions::handle save_clip_action;
        };

        struct resources {
            gpu::pipeline convert_pipeline;
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
            time applied_ring_budget = seconds(30.f);
            bool first_ring_push_logged = false;
        };

        static auto run(
            run_context& ctx,
            const gpu::context::state& gpu_s,
            const asset::state& assets_s,
            const actions::system::state& sys,
            settings& cfg,
            resources& r,
            frame_data& fd,
            state& s
        ) -> async::task<>;

        static auto frame(
            const frame_context& ctx,
            const gpu::context::state& gpu_s,
            const settings& cfg,
            const resources& r,
            frame_data& fd,
            const state& s
        ) -> async::task<>;
    };
}
