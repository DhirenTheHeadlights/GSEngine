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

	struct window_minimize_request {};

	struct window_toggle_maximize_request {};

	struct window_chrome_metrics_request {
		int caption_height = 0;
		int controls_width = 0;
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
				= gse::settings::describe<"Show the system mouse cursor over the window.">{},
				= gse::shared
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

			[[= gse::shared]] GLFWwindow* handle = nullptr;
			std::string title;

			gse::display_mode current_display_mode = gse::display_mode::windowed;
			[[= gse::shared]] int current_present_mode_index = 0;
			bool focused = true;
			bool framebuffer_resized = false;
			[[= gse::shared]] bool ui_focus = false;
			bool decorated = true;
			bool maximized = false;
			int last_monitor_index = 0;
			vec2i position{ 0, 0 };
			vec2i size{ 0, 0 };
			bool cmd_minimize = false;
			bool cmd_toggle_maximize = false;
			bool native_frame = false;
			int chrome_caption_height = 0;
			int chrome_controls_width = 0;

			rect_t<vec2i> windowed_rect = rect_t<vec2i>::from_position_size(
				{ 100, 100 },
				{ 1920, 1080 }
			);

			[[= gse::shared]] task::concurrent_queue<input::event> input_events;
		};

		static auto tick(
			scheduler& sched,
			data& d
		) -> void;

		static auto shutdown(
			data& d
		) -> void;

		static auto poll_events() -> void;

		static auto apply_commands(
			data& d
		) -> void;

		static auto install_native_frame(
			GLFWwindow* handle,
			const int* caption_height,
			const int* controls_width
		) -> void;

		[[nodiscard]] static auto is_open(
			const data& d
		) -> bool;

		[[nodiscard]] static auto is_open(
			shared_view<window> d
		) -> bool;

		[[nodiscard]] static auto minimized(
			const data& d
		) -> bool;

		[[nodiscard]] static auto minimized(
			shared_view<window> d
		) -> bool;

		[[nodiscard]] static auto viewport(
			const data& d
		) -> vec2i;

		[[nodiscard]] static auto viewport(
			shared_view<window> d
		) -> vec2i;

		[[nodiscard]] static auto frame_buffer_resized(
			data& d
		) -> bool;

		[[nodiscard]]
		static auto create_vulkan_surface(
			shared_view<window> d,
			vk::Instance instance
		) -> vk::SurfaceKHR;

		[[nodiscard]] static auto raw_handle(
			shared_view<window> d
		) -> GLFWwindow*;

		static auto show(
			const data& d
		) -> void;

		static auto show(
			shared_view<window> d
		) -> void;

		static auto set_ui_focus(
			data& d,
			bool focus
		) -> void;

		[[nodiscard]] static auto ui_focus(
			shared_view<window> d
		) -> bool;

		[[nodiscard]] static auto vulkan_instance_extensions() -> std::span<const char* const>;

		[[nodiscard]] static auto enumerate_monitors() -> std::vector<monitor_info>;

		[[nodiscard]] static auto enumerate_resolutions(
			int monitor_index
		) -> std::vector<resolution_info>;
	};
}

namespace gse {
	auto window_handle_open(
		GLFWwindow* handle
	) -> bool;

	auto window_handle_minimized(
		GLFWwindow* handle
	) -> bool;

	auto window_handle_viewport(
		GLFWwindow* handle
	) -> vec2i;

	auto window_handle_surface(
		GLFWwindow* handle,
		vk::Instance instance
	) -> vk::SurfaceKHR;

	auto window_handle_show(
		GLFWwindow* handle
	) -> void;
}

