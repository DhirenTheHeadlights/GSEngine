export module gse.os:window;

import std;

import :input_events;

import gse.math;
import gse.concurrency;
import gse.ecs;
import gse.meta;

export namespace gse {
	struct ui_focus_request {
		bool focus = false;
	};

	struct cursor_capture_request {
		bool capture = false;
	};

	struct window_minimize_request {};

	struct window_toggle_maximize_request {};

	struct window_close_request {};

	struct window_open_file_request {
		std::string title;
		std::string filter_name;
		std::string filter_pattern;
	};

	struct window_open_file_result {
		std::filesystem::path path;
	};

	struct window_launcher_mode_request {
		bool active = false;
		int width = 0;
		int height = 0;
	};

	struct window_chrome_metrics_request {
		int caption_height = 0;
		int controls_width = 0;
		int interactive_x0 = 0;
		int interactive_x1 = 0;
		int resize_exclude_y0 = 0;
		int resize_exclude_y1 = 0;
	};

	enum class cursor_shape : std::uint8_t {
		arrow = 0,
		hand = 1,
		resize_ew = 2,
		resize_ns = 3,
		resize_nwse = 4,
		resize_nesw = 5,
	};

	struct set_cursor_shape_request {
		cursor_shape shape = cursor_shape::arrow;
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

	struct native_window_handle {
		void* value = nullptr;

		[[nodiscard]] explicit operator bool() const {
			return value != nullptr;
		}
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

export namespace gse::window {
	struct geometry {
		[[= gse::settings::describe<"Left edge of the restored window, in virtual desktop coordinates.">{}]]
		int x = 0;

		[[= gse::settings::describe<"Top edge of the restored window, in virtual desktop coordinates.">{}]]
		int y = 0;

		[[= gse::settings::describe<"Width of the restored window.">{}]]
		int width = 0;

		[[= gse::settings::describe<"Height of the restored window.">{}]]
		int height = 0;

		[[= gse::settings::describe<"Whether the window was maximized when it was last closed.">{}]]
		bool maximized = false;
	};

	struct [[= gse::settings::category<"Window">{}, = gse::system_state<"Window">{}]] data {
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

		[[
			= gse::settings::describe<"Position and size the window is restored to on launch.">{}
		]]
		geometry saved_geometry;

		[[= gse::shared]] native_window_handle handle;

		[[
			= gse::settings::describe<"Text shown in the window title bar and the taskbar entry.">{},
			= gse::settings::app_scope{}
		]]
		std::string title;

		gse::display_mode current_display_mode = gse::display_mode::windowed;
		[[= gse::shared]] int current_present_mode_index = 0;
		[[= gse::shared]] bool focused = true;
		[[= gse::shared]] bool shown = false;
		bool framebuffer_resized = false;
		[[= gse::shared]] bool ui_focus = false;
		[[= gse::shared]] bool cursor_captured = false;
		[[
			= gse::settings::describe<"Hide the cursor because another process hosts the rendered surface.">{},
			= gse::settings::app_scope{}
		]]
		bool cursor_suppressed = false;

		[[
			= gse::settings::describe<"Render into a surface shared with a host process instead of presenting a swapchain.">{},
			= gse::settings::app_scope{}
		]]
		bool attached = false;

		bool decorated = true;
		bool maximized = false;
		int last_monitor_index = 0;
		int current_monitor_index = -1;
		vec2i position{ 0, 0 };
		vec2i size{ 0, 0 };
		[[= gse::shared]] float content_scale = 1.f;
		[[= gse::shared]] std::string monitor_key;
		bool cmd_minimize = false;
		bool cmd_toggle_maximize = false;
		bool cmd_close = false;
		bool cmd_open_file = false;
		std::string cmd_open_file_title;
		std::string cmd_open_file_filter_name;
		std::string cmd_open_file_filter_pattern;
		bool cmd_launcher_pending = false;
		bool cmd_launcher_active = false;
		vec2i cmd_launcher_size{ 0, 0 };
		vec2i launcher_saved_position{ 0, 0 };
		vec2i launcher_saved_size{ 0, 0 };
		bool launcher_saved_maximized = false;

		[[
			= gse::settings::describe<"Keep the OS window frame instead of drawing application chrome inside the client area.">{},
			= gse::settings::app_scope{}
		]]
		bool native_frame = false;
		int chrome_caption_height = 0;
		int chrome_controls_width = 0;
		int chrome_interactive_x0 = 0;
		int chrome_interactive_x1 = 0;
		int chrome_resize_exclude_y0 = 0;
		int chrome_resize_exclude_y1 = 0;

		rect_t<vec2i> windowed_rect = rect_t<vec2i>::from_position_size(
			{ 100, 100 },
			{ 1920, 1080 }
		);

		[[= gse::shared]] task::concurrent_queue<input::event> input_events;
	};

	auto tick(
		scheduler& sched,
		data& d
	) -> void;

	[[= gse::system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;

	auto poll_events() -> void;

	auto clipboard_text() -> std::string;

	auto set_clipboard_text(
		std::string text
	) -> void;

	auto prompt_for_file(
		data& d
	) -> std::filesystem::path;

	auto apply_commands(
		data& d
	) -> void;

	auto install_native_frame(
		native_window_handle handle,
		const int* caption_height,
		const int* controls_width,
		const int* interactive_x0,
		const int* interactive_x1,
		const int* resize_exclude_y0,
		const int* resize_exclude_y1
	) -> void;

	[[nodiscard]] auto is_open(
		const data& d
	) -> bool;

	[[nodiscard]] auto is_open(
		shared_view<data> d
	) -> bool;

	[[nodiscard]] auto minimized(
		const data& d
	) -> bool;

	[[nodiscard]] auto minimized(
		shared_view<data> d
	) -> bool;

	[[nodiscard]] auto viewport(
		const data& d
	) -> vec2i;

	[[nodiscard]] auto viewport(
		shared_view<data> d
	) -> vec2i;

	[[nodiscard]] auto frame_buffer_resized(
		data& d
	) -> bool;

	[[nodiscard]] auto raw_handle(
		const data& d
	) -> native_window_handle;

	[[nodiscard]] auto raw_handle(
		shared_view<data> d
	) -> native_window_handle;

	auto show(
		data& d
	) -> void;

	auto set_ui_focus(
		data& d,
		bool focus
	) -> void;

	[[nodiscard]] auto ui_focus(
		shared_view<data> d
	) -> bool;

	[[nodiscard]] auto enumerate_monitors() -> std::vector<monitor_info>;

	[[nodiscard]] auto enumerate_resolutions(
		int monitor_index
	) -> std::vector<resolution_info>;
}

