export module gse.os:window;

import std;
import vulkan;
import gse.glfw;

import :input_events;

import gse.assert;
import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.meta;

export namespace gse {
	struct ui_focus_request {
		bool focus = false;
	};

	struct monitor_info {
		std::string name;
		int width = 0;
		int height = 0;
		int refresh_rate = 0;
	};

	struct resolution_info {
		int width = 0;
		int height = 0;
		int refresh_rate = 0;
	};
}

template <>
struct std::formatter<gse::resolution_info> : std::formatter<std::string> {
	auto format(const gse::resolution_info& info, std::format_context& ctx) const {
		return std::formatter<std::string>::format(
			std::format("{}x{} @{}Hz", info.width, info.height, info.refresh_rate),
			ctx
		);
	}
};

export namespace gse {
	struct window {
		struct settings {
			static constexpr std::string_view category = "Window";

			[[=gse::settings::describe<"Run in fullscreen on the selected monitor. When off, the window uses its last windowed rect.">{}]]
			bool fullscreen = false;

			[[=gse::settings::describe<"Show the system mouse cursor over the window.">{}]]
			bool mouse_visible = false;

			[[=gse::settings::describe<"Monitor that hosts the window in fullscreen mode.">{}]]
			gse::settings::choice<int> monitor;

			[[=gse::settings::describe<"Resolution and refresh rate used when fullscreen.">{}]]
			gse::settings::choice<int> resolution;
		};

		struct state {
			GLFWwindow* handle = nullptr;
			std::string title;

			bool current_fullscreen = false;
			bool focused = true;
			bool framebuffer_resized = false;
			bool ui_focus = false;
			int last_monitor_index = 0;

			rect_t<vec2i> windowed_rect = rect_t<vec2i>::from_position_size({ 100, 100 }, { 1920, 1080 });

			task::concurrent_queue<input::event> input_events;
		};

		static auto run(
			run_context& ctx,
			settings& cfg,
			state& s
		) -> async::task<>;

		static auto shutdown(
			shutdown_context& phase,
			state& s
		) -> void;

		static auto poll_events(
		) -> void;

		[[nodiscard]] static auto is_open(
			const state& s
		) -> bool;

		[[nodiscard]] static auto minimized(
			const state& s
		) -> bool;

		[[nodiscard]] static auto viewport(
			const state& s
		) -> vec2i;

		[[nodiscard]] static auto frame_buffer_resized(
			state& s
		) -> bool;

		[[nodiscard]] static auto create_vulkan_surface(
			const state& s,
			vk::Instance instance
		) -> vk::SurfaceKHR;

		[[nodiscard]] static auto raw_handle(
			const state& s
		) -> GLFWwindow*;

		static auto set_ui_focus(
			state& s,
			bool focus
		) -> void;

		[[nodiscard]] static auto ui_focus(
			const state& s
		) -> bool;

		[[nodiscard]] static auto vulkan_instance_extensions(
		) -> std::span<const char* const>;

		[[nodiscard]] static auto enumerate_monitors(
		) -> std::vector<monitor_info>;

		[[nodiscard]] static auto enumerate_resolutions(
			int monitor_index
		) -> std::vector<resolution_info>;
	};
}
