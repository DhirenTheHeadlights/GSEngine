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

	enum class display_mode : std::uint8_t {
		windowed = 0,
		borderless_fullscreen = 1,
		exclusive_fullscreen = 2,
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
		struct [[= gse::settings::category<"Window">{}]] data {
			[[
				= gse::settings::
					describe<"Windowed, borderless fullscreen, or exclusive fullscreen on the selected monitor.">{}
			]]
			gse::settings::choice<int> display_mode;

			[[
				= gse::settings::describe<"Show the system mouse cursor over the window.">{}
			]]
			bool mouse_visible = false;

			[[
				= gse::settings::describe<"Monitor that hosts the window in fullscreen mode.">{}
			]]
			gse::settings::choice<int> monitor;

			[[
				= gse::settings::describe<"Resolution and refresh rate used when fullscreen.">{}
			]]
			gse::settings::choice<int> resolution;

			[[
				= gse::settings::describe<"Vulkan present mode. FIFO is vsync (no tearing). Mailbox is low-latency "
										  "vsync. Immediate has tearing "
										  "but lowest latency. FIFO Relaxed is FIFO with tear-on-late-frame.">{}
			]]
			gse::settings::choice<int> present_mode;

			GLFWwindow* handle = nullptr;
			std::string title;

			gse::display_mode current_display_mode = gse::display_mode::windowed;
			int current_present_mode_index = 0;
			bool focused = true;
			bool framebuffer_resized = false;
			bool ui_focus = false;
			int last_monitor_index = 0;

			rect_t<vec2i> windowed_rect = rect_t<vec2i>::from_position_size({ 100, 100 }, { 1920, 1080 });

			task::concurrent_queue<input::event> input_events;
		};

		static auto run(
			run_context& ctx,
			data& d
		) -> async::task<>;

		static auto shutdown(
			shutdown_context& phase,
			data& d
		) -> void;

		static auto poll_events() -> void;

		[[nodiscard]] static auto is_open(
			const data& d
		) -> bool;

		[[nodiscard]] static auto minimized(
			const data& d
		) -> bool;

		[[nodiscard]] static auto viewport(
			const data& d
		) -> vec2i;

		[[nodiscard]] static auto frame_buffer_resized(
			data& d
		) -> bool;

		[[nodiscard]]
		static auto create_vulkan_surface(
			const data& d,
			vk::Instance instance
		) -> vk::SurfaceKHR;

		[[nodiscard]] static auto raw_handle(
			const data& d
		) -> GLFWwindow*;

		static auto show(
			const data& d
		) -> void;

		static auto set_ui_focus(
			data& d,
			bool focus
		) -> void;

		[[nodiscard]] static auto ui_focus(
			const data& d
		) -> bool;

		[[nodiscard]] static auto vulkan_instance_extensions() -> std::span<const char* const>;

		[[nodiscard]] static auto enumerate_monitors() -> std::vector<monitor_info>;

		[[nodiscard]] static auto enumerate_resolutions(
			int monitor_index
		) -> std::vector<resolution_info>;
	};
}
