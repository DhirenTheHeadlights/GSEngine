export module gse.gpu:context;

import std;

import :types;
import :vulkan_device;
import :descriptor_heap;
import :shader_registry;
import :device;
import :swap_chain;
import :frame;
import :render_graph;
import :render_pass;
import :bindless;

import gse.os;

import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.meta;

export namespace gse {
	enum class render_layer : std::uint8_t {
		background = 0,
		content = 1,
		overlay = 2,
		popup = 3,
		modal = 4,
		cursor = 5,
		debug = 6,
	};
}

export namespace gse::gpu {
	struct context {
		struct settings {
			static constexpr std::string_view category = "Graphics";

			[[=gse::settings::describe<"Enable Vulkan validation layers. Catches API misuse but adds significant overhead. Requires a restart.">{}, =gse::settings::restart_required{}]]
			bool validation_layers_enabled = false;

			[[=gse::settings::describe<"Vulkan device tracking and naming options.">{}]]
			vulkan::device::settings device;
		};

		struct state {
			std::unique_ptr<gpu::device> device;
			std::unique_ptr<gpu::shader_registry> shader_registry;
			std::unique_ptr<swap_chain> swapchain;
			std::unique_ptr<gpu::frame> frame;
			std::unique_ptr<vulkan::render_graph> render_graph;
			std::unique_ptr<bindless_texture_set> bindless_textures;
			concurrency::frame_scheduler scheduler;
			std::function<void(class shader_registry&)> on_registry_created;
		};

		using swap_chain_recreate_callback = std::function<void()>;

		static auto run(
			run_context& ctx,
			const window::state& window_s,
			settings& cfg,
			state& s
		) -> async::task<>;

		static auto shutdown(
			shutdown_context& phase,
			state& s
		) -> void;

		[[nodiscard]] static auto begin_frame(
			state& s,
			window::state& window_s
		) -> std::expected<frame_token, frame_status>;

		static auto execute_frame(
			state& s,
			std::vector<render_pass_request> requests
		) -> void;

		static auto end_frame(
			state& s,
			window::state& window_s
		) -> void;

		static auto on_swap_chain_recreate(
			const state& s,
			swap_chain_recreate_callback callback
		) -> void;

		static auto wait_idle(
			const state& s
		) -> void;

		[[nodiscard]] static auto device_handle(
			const state& s
		) -> handle<vulkan::device>;
	};

	struct gpu_resume_request {
		std::coroutine_handle<> handle;
		context::state** out_state = nullptr;
	};

	struct on_gpu_awaitable {
		channel_writer& channels;
		context::state* state = nullptr;

		auto await_ready(
		) const noexcept -> bool;

		auto await_suspend(
			std::coroutine_handle<> h
		) -> void;

		auto await_resume(
		) -> context::state&;
	};

	[[nodiscard]] auto on_gpu(
		channel_writer& channels
	) -> on_gpu_awaitable;
}

auto gse::gpu::on_gpu_awaitable::await_ready() const noexcept -> bool {
	return false;
}

auto gse::gpu::on_gpu_awaitable::await_suspend(std::coroutine_handle<> h) -> void {
	channels.push<gpu_resume_request>({ .handle = h, .out_state = &state });
}

auto gse::gpu::on_gpu_awaitable::await_resume() -> context::state& {
	return *state;
}

auto gse::gpu::on_gpu(channel_writer& channels) -> on_gpu_awaitable {
	return { channels };
}
