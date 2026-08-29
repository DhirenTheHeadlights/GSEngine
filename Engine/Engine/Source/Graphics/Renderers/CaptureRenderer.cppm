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
import gse.gpu_record;

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
	struct toggle_recording_request {};

	struct recording_state {
		std::thread thread;
		std::mutex mutex;
		std::condition_variable cv;
		std::queue<gpu::encoded_unit> queue;
		bool running = false;
		std::atomic<bool> active{ false };
		std::filesystem::path path;
		std::chrono::steady_clock::time_point last_toggle{};
	};

	struct [[= system_state<"Capture">{}, = settings::category<"Graphics">{}]] data {
		[[
			= settings::describe<"Keep the instant-replay encoder running. It costs the video engine every "
									  "capture interval whether or not a clip is ever saved; off frees that cost but "
									  "the replay ring is empty, so save-clip has nothing to write until re-enabled. "
									  "Screenshots are unaffected.">{},
			= settings::hot_reloadable
		]]
		bool encode_enabled = true;

		[[
			= settings::describe<"Length of the rolling capture ring buffer. Saving a clip writes the most "
									  "recent N seconds of frames.">{},
			= settings::range<5.f, 120.f>{}
		]]
		time ring_budget = seconds(30.f);

		[[
			= settings::describe<"Shortest gap between encoded capture frames. Raising it encodes fewer "
									  "frames per second, which cuts video-engine cost and stretches the ring "
									  "buffer over more wall time for the same memory.">{},
			= settings::range<0.004f, 0.2f>{}
		]]
		time capture_interval = seconds(1.f / 60.f);

		[[
			= settings::describe<"Target encode bitrate. Without one the driver picks its own budget, which "
									  "lands near 6 Mb/s at 1080p and goes soft the moment the scene moves. Raise "
									  "it for sharper high-motion clips at the cost of file size, which is just "
									  "bitrate times duration: a 16 s clip runs 30 MB at 15 Mb/s, and only 4.5 Mb/s "
									  "keeps it under GitHub's 10 MB attachment cap. Ignored if the driver exposes "
									  "no rate control mode.">{},
			= settings::range<1.f, 60.f>{},
			= settings::hot_reloadable
		]]
		bitrate capture_bitrate = megabits_per_second(15.f);

		actions::handle screenshot_action;
		actions::handle save_clip_action;
		actions::handle toggle_recording_action;

		gpu::shader_program convert_pipeline;
		per_frame_resource<gpu::image> rgba_captures;
		std::array<gpu::bindless_handle, per_frame_resource<gpu::image>::frames_in_flight> rgba_slots;
		gpu::bindless_handle sampler;
		gpu::encode_source encode_target;
		time last_capture_pts{};
		bool captured_once = false;
		bool encode_active = false;

		per_frame_resource<pending_screenshot> screenshots;
		bool screenshot_requested = false;
		std::unique_ptr<std::atomic<bool>> write_in_progress = std::make_unique<std::atomic<bool>>(false);
		std::unique_ptr<std::atomic<bool>> clip_save_in_progress = std::make_unique<std::atomic<bool>>(false);
		gpu::video_encoder encoder;
		ring clip_ring;
		time applied_ring_budget = seconds(30.f);
		bitrate applied_capture_bitrate = megabits_per_second(15.f);
		bool first_ring_push_logged = false;

		[[= stable_shared]] std::unique_ptr<recording_state> recording = std::make_unique<recording_state>();
	};

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_write<actions::add_action_request> actions_out
	) -> async::task<>;

	[[= system_run<>{}]]
	auto run(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<asset::data> assets_s,
		shared_view<actions::data> sys,
		data& d,
		channel_write<screenshot_request, save_clip_request, toggle_recording_request> capture_out
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		const context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out,
		channel_read<toggle_recording_request, save_clip_request, screenshot_request> capture_in
	) -> async::task<>;

	[[= system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;
}