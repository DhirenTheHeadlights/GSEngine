export module gse.gpu:context;

import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.gpu_backend;
import gse.log;
import gse.math;
import gse.meta;
import gse.os;
import gse.save;
import gse.time;
import std;

import :device;
import :frame;
import :render_graph;
import :swap_chain;

export namespace gse::gpu {
	struct gpu_resume_request;
}

export namespace gse::gpu::context {
	constexpr std::string_view default_gpu_perf_metrics =
		"tpc__warps_active_shader_cs_queue_sync_realtime.avg.pct_of_peak_sustained_elapsed,"
		"sm__inst_executed_realtime.avg.per_cycle_active,"
		"sm__pipe_fma_cycles_active_realtime.avg.pct_of_peak_sustained_elapsed,"
		"lts__t_sectors_realtime.sum.per_second,"
		"dram__bytes.sum.per_second";

	struct window_presentation {
		id window;
		gpu::surface surface;
		std::unique_ptr<swap_chain> swapchain;
	};

	struct [[= system_state<"Gpu">{}, = settings::category<"Graphics">{}]] data {
		[[
			= settings::describe<"Enable Vulkan validation layers. Catches API misuse but adds significant "
									  "overhead. Requires a restart.">{},
			= settings::restart_required{}
		]]
		bool validation_layers_enabled = false;

		[[
			= settings::describe<"GPU backend to initialize on startup. Vulkan falls back to dx12 if it is unsupported. Requires a restart.">{},
			= settings::restart_required{}
		]]
		backend_kind backend = backend_kind::vulkan;

		[[
			= settings::describe<"Vulkan device tracking and naming options.">{}
		]]
		gpu::device_settings device_settings;

		[[
			= settings::describe<"Record GPU timestamp queries around each render pass for the profiler.">{}
		]]
		bool gpu_timestamps_enabled = true;

		[[
			= settings::describe<"Collect pipeline statistics (invocations, primitives) per pass. Has measurable overhead.">{}
		]]
		bool gpu_pipeline_stats_enabled = false;

		[[
			= settings::describe<"Record intra-pass GPU timestamp marks for passes that place them. Adds one query per mark.">{}
		]]
		bool gpu_intra_pass_marks_enabled = false;

		[[
			= settings::describe<"Sample NVIDIA GPU hardware counters (SM throughput, warp occupancy, stall reasons) and "
								  "attribute them to render graph passes and marks. Needs the Nsight Perf SDK at build time, "
								  "GPU timestamps at run time, and performance counter permission from the driver.">{}
		]]
		bool gpu_perf_metrics_enabled = false;

		[[
			= settings::describe<"Comma separated Nsight Perf metric names to sample. The set must fit a single "
								  "configuration pass; the periodic sampler cannot replay, so an oversized set is refused "
								  "whole rather than trimmed.">{}
		]]
		std::string gpu_perf_metrics{ default_gpu_perf_metrics };

		[[
			= settings::describe<"Index of the NVIDIA device to sample, for machines holding more than one.">{}
		]]
		std::uint32_t gpu_perf_metrics_device = 0;

		[[
			= settings::describe<"Hardware counter sampling period. Shorter periods resolve shorter dispatches and cost "
								  "more decode work; a pass shorter than two periods gets no row.">{}
		]]
		time gpu_perf_metrics_interval = microseconds(10.f);

		[[
			= settings::describe<"Lock GPU clocks to rated TDP while sampling. Makes stall ratios repeatable but moves "
								  "every microsecond figure away from the profile tables the rest of the work is measured "
								  "against.">{}
		]]
		bool gpu_perf_metrics_lock_clocks = false;

		[[= stable_shared]] std::unique_ptr<gpu::device> device;
		[[= stable_shared]] std::unique_ptr<swap_chain> swapchain;
		[[= stable_shared]] std::unique_ptr<gpu::frame> frame;
		[[= stable_shared]] std::unique_ptr<gpu::render_graph> render_graph;
		std::vector<std::unique_ptr<window_presentation>> secondaries;
		concurrency::frame_scheduler scheduler;

		[[
			= settings::describe<"Clear the swapchain to a dark neutral tone instead of black.">{},
			= settings::app_scope{}
		]]
		bool dark_background = false;
	};

	using swap_chain_recreate_callback = std::function<void()>;

	[[= system_init{}]] auto init(
		std::optional<shared_view<window::data>> window_s,
		const save::registry* save_reg,
		data& d
	) -> async::task<>;

	[[= system_run<>{}]] auto run(
		gse::context& ctx,
		data& d,
		channel_read<gpu_resume_request, window_opened, window_closed> resume_in
	) -> async::task<>;

	[[= system_shutdown{}]] auto shutdown(
		data& d
	) -> void;

	[[nodiscard]]
	auto begin_frame(
		data& d,
		window::window_surface* window_s
	) -> std::expected<frame_token, frame_status>;

	auto end_frame(
		data& d
	) -> void;

	[[nodiscard]] auto create_presentation(
		data& d,
		const window_opened& win
	) -> window_presentation*;

	auto destroy_presentation(
		data& d,
		id window
	) -> void;

	[[nodiscard]] auto find_presentation(
		data& d,
		id window
	) -> window_presentation*;

	auto sync_present_targets(
		data& d,
		window::data& windows
	) -> void;


	auto on_swap_chain_recreate(
		shared_view<data> d,
		swap_chain_recreate_callback callback
	) -> void;

	auto wait_idle(
		const data& d
	) -> void;
}

export namespace gse::gpu {
	struct gpu_resume_request {
		std::coroutine_handle<> handle;
		context::data** out_state = nullptr;
	};

	struct on_gpu_awaitable {
		channel_write<gpu_resume_request> channels;
		context::data* state = nullptr;

		auto await_ready() const noexcept -> bool;

		auto await_suspend(
			std::coroutine_handle<> h
		) -> void;

		auto await_resume() -> context::data&;
	};

	[[nodiscard]] auto on_gpu(
		channel_write<gpu_resume_request> channels
	) -> on_gpu_awaitable;
}

auto gse::gpu::on_gpu_awaitable::await_ready() const noexcept -> bool {
	return false;
}

auto gse::gpu::on_gpu_awaitable::await_suspend(std::coroutine_handle<> h) -> void {
	channels.push<gpu_resume_request>({
		.handle = h,
		.out_state = &state
	});
}

auto gse::gpu::on_gpu_awaitable::await_resume() -> context::data& {
	return *state;
}

auto gse::gpu::on_gpu(const channel_write<gpu_resume_request> channels) -> on_gpu_awaitable {
	return { channels };
}