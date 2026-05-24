export module gse.gpu:context;

import std;

import :types;
import :vulkan_device;
import :descriptor_heap;
import :shader_registry;
import :device;
import :swap_chain;
import :frame;
import :transient_pool;
import :render_graph;
import :render_pass;
import :bindless;
import :bindless_heap;

import gse.os;

import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.meta;

export namespace gse::gpu {
	struct context {
		struct [[= gse::settings::category<"Graphics">{}]] data {
			[[
				= gse::settings::describe<"Enable Vulkan validation layers. Catches API misuse but adds significant "
										  "overhead. Requires a restart.">{},
				= gse::settings::restart_required{}
			]]
			bool validation_layers_enabled = false;

			[[
				= gse::settings::describe<"Vulkan device tracking and naming options.">{}
			]]
			vulkan::device::settings device_settings;

			[[= gse::shared]] std::unique_ptr<gpu::device> device;
			[[= gse::shared]] std::unique_ptr<gpu::shader_registry> shader_registry;
			[[= gse::shared]] std::unique_ptr<swap_chain> swapchain;
			[[= gse::shared]] std::unique_ptr<gpu::frame> frame;
			[[= gse::shared]] std::unique_ptr<gpu::render_graph> render_graph;
			[[= gse::shared]] std::unique_ptr<bindless_texture_set> bindless_textures;
			[[= gse::shared]] std::unique_ptr<vulkan::bindless_heaps> bindless_heaps;
			[[= gse::shared]] concurrency::frame_scheduler scheduler;
		};

		using swap_chain_recreate_callback = std::function<void()>;

		static auto run(
			run_context& ctx,
			const window::data& window_s,
			data& d
		) -> async::task<>;

		static auto shutdown(
			shutdown_context& phase,
			data& d
		) -> void;

		[[nodiscard]]
		static auto begin_frame(
			data& d,
			window::data& window_s
		) -> std::
			expected<frame_token, frame_status>;

		static auto execute_frame(
			data& d,
			scheduler& s
		) -> void;

		static auto end_frame(
			data& d,
			window::data& window_s
		) -> void;

		static auto on_swap_chain_recreate(
			const data& d,
			swap_chain_recreate_callback callback
		) -> void;

		static auto wait_idle(
			const data& d
		) -> void;
	};

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
