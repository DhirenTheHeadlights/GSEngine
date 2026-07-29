export module gse.gpu:context;

import std;

import :device;
import :swap_chain;
import :frame;
import :transient_pool;
import :render_graph;

import gse.gpu_backend;
import gse.os;

import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.meta;
import gse.save;

export namespace gse::gpu::context {
	struct [[= gse::system_state<"Gpu">{}, = gse::settings::category<"Graphics">{}]] data {
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

		[[= stable_shared]] std::unique_ptr<gpu::device> device;
		[[= stable_shared]] std::unique_ptr<swap_chain> swapchain;
		[[= stable_shared]] std::unique_ptr<gpu::frame> frame;
		[[= stable_shared]] std::unique_ptr<gpu::render_graph> render_graph;
		concurrency::frame_scheduler scheduler;

		color_clear swapchain_clear{};
	};

	using swap_chain_recreate_callback = std::function<void()>;

	[[= system_init{}]] auto init(
		std::optional<shared_view<window::data>> window_s,
		const save::registry* save_reg,
		data& d
	) -> async::task<>;

	[[= system_run<>{}]] auto run(
		gse::context& ctx,
		data& d
	) -> async::task<>;

	[[= system_shutdown{}]] auto shutdown(
		data& d
	) -> void;

	[[nodiscard]]
	auto begin_frame(
		data& d,
		window::data* window_s
	) -> std::expected<frame_token, frame_status>;

	auto end_frame(
		data& d,
		window::data* window_s
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
		channel_writer& channels;
		context::data* state = nullptr;

		auto await_ready() const noexcept -> bool;

		auto await_suspend(
			std::coroutine_handle<> h
		) -> void;

		auto await_resume() -> context::data&;
	};

	[[nodiscard]] auto on_gpu(
		channel_writer& channels
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

auto gse::gpu::on_gpu(channel_writer& channels) -> on_gpu_awaitable {
	return { channels };
}
