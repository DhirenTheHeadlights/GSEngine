export module gse.os:window;

import std;

import :input_events;

import gse.core;
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

	struct window_minimize_request {
		id window;
	};

	struct window_toggle_maximize_request {
		id window;
	};

	struct window_close_request {
		id window;
	};

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
		id window;
		int caption_height = 0;
		int controls_width = 0;
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

	struct window_popout_request {
		std::string menu_name;
		std::string title;
		vec2i screen_position;
		vec2i size{ 640, 480 };
	};

	struct window_opened {
		id id;
		native_window_handle handle;
		vec2i position;
		vec2i size;
		int present_mode_index = 0;
		std::string for_menu;
	};

	struct window_closed {
		id id;
	};

	struct window_locate_cursor_request {
		id source;
		vec2f client_cursor;
	};

	struct window_cursor_located {
		id source;
		std::optional<id> window;
		vec2f client_cursor;
		vec2i screen_cursor;
	};

	struct window_resized {
		id id;
		vec2i size;
	};

	struct window_moved {
		id id;
		vec2i position;
	};
}

template <>
struct std::formatter<gse::resolution_info> : std::formatter<std::string> {
	auto format(const gse::resolution_info& info, format_context& ctx) const {
		return std::formatter<string>::format(
			std::format("{}x{} @{}Hz", info.width, info.height, info.refresh_rate),
			ctx
		);
	}
};

export namespace gse::window {
	struct geometry {
		[[= settings::describe<"Left edge of the restored window, in virtual desktop coordinates.">{}]]
		int x = 0;

		[[= settings::describe<"Top edge of the restored window, in virtual desktop coordinates.">{}]]
		int y = 0;

		[[= settings::describe<"Width of the restored window.">{}]]
		int width = 0;

		[[= settings::describe<"Height of the restored window.">{}]]
		int height = 0;

		[[= settings::describe<"Whether the window was maximized when it was last closed.">{}]]
		bool maximized = false;
	};

	struct composition_probe {
		bool iconified = false;
		bool visible = false;
		unsigned int cloaked = 0;

		[[nodiscard]] auto operator==(const composition_probe&) const -> bool = default;
	};

	struct window_surface {
		[[= shared]] gse::id id;
		[[= shared]] native_window_handle handle;
		[[= shared]] bool focused = true;
		[[= shared]] bool shown = false;
		bool framebuffer_resized = false;
		[[= shared]] bool ui_focus = false;
		[[= shared]] bool cursor_captured = false;
		[[= shared]] float content_scale = 1.f;
		[[= shared]] std::string monitor_key;
		vec2i position{ 0, 0 };
		vec2i size{ 0, 0 };
		int chrome_caption_height = 0;
		int chrome_controls_width = 0;
		int chrome_resize_exclude_y0 = 0;
		int chrome_resize_exclude_y1 = 0;
		composition_probe last_composition;
		[[= shared]] int present_mode_index = 0;
		bool attached = false;
		[[= shared]] task::concurrent_queue<input::event> input_events;
	};

	struct [[= settings::category<"Window">{}, = system_state<"Window">{}]] data {
		[[
			= settings::
				describe<"Windowed, borderless fullscreen, or exclusive fullscreen on the selected monitor.">{}
		]]
		settings::choice<int> display_mode;

		[[
			= settings::describe<"Show the system mouse cursor over the window.">{},
			= shared
		]]
		bool mouse_visible = false;

		[[
			= settings::describe<"Monitor that hosts the window in fullscreen mode.">{}
		]]
		settings::choice<int> monitor;

		[[
			= settings::describe<"Resolution and refresh rate used when fullscreen.">{}
		]]
		settings::choice<int> resolution;

		[[
			= settings::describe<"Vulkan present mode. FIFO is vsync (no tearing). Mailbox is low-latency "
									  "vsync. Immediate has tearing "
									  "but lowest latency. FIFO Relaxed is FIFO with tear-on-late-frame.">{}
		]]
		settings::choice<int> present_mode;

		[[
			= settings::describe<"Position and size the window is restored to on launch.">{}
		]]
		geometry saved_geometry;

		[[
			= settings::describe<"Text shown in the window title bar and the taskbar entry.">{},
			= settings::app_scope{}
		]]
		std::string title;

		gse::display_mode current_display_mode = gse::display_mode::windowed;
		[[= shared]] int current_present_mode_index = 0;
		[[
			= settings::describe<"Hide the cursor because another process hosts the rendered surface.">{},
			= settings::app_scope{}
		]]
		bool cursor_suppressed = false;

		[[
			= settings::describe<"Render into a surface shared with a host process instead of presenting a swapchain.">{},
			= settings::app_scope{}
		]]
		bool attached = false;

		bool decorated = true;
		bool restore_maximized = false;
		int last_monitor_index = 0;
		int current_monitor_index = -1;
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
			= settings::describe<"Keep the OS window frame instead of drawing application chrome inside the client area.">{},
			= settings::app_scope{}
		]]
		bool native_frame = false;

		[[= shared]] gse::id focused_window;
		[[= shared]] gse::id cursor_window;
		[[= shared]] window_surface primary;
		std::vector<std::unique_ptr<window_surface>> secondaries;
	};

	auto tick(
		scheduler& sched,
		data& d
	) -> void;

	[[= system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;

	auto poll_events() -> void;

	auto clipboard_text() -> std::string;

	auto set_clipboard_text(
		std::string text
	) -> void;

	struct clipboard_image {
		std::filesystem::path path;
		vec2u size;
		std::vector<std::byte> pixels;
	};

	auto clipboard_image_available() -> bool;

	auto request_clipboard_image() -> void;

	auto take_clipboard_image() -> std::optional<clipboard_image>;

	auto prompt_for_file(
		data& d
	) -> std::filesystem::path;

	auto apply_commands(
		data& d
	) -> void;

	auto install_native_frame(
		window_surface& surface
	) -> void;

	struct secondary_window_desc {
		std::string title;
		vec2i size{ 800, 600 };
		vec2i position{ 0, 0 };
		bool use_position = false;
	};

	[[nodiscard]] auto create_secondary(
		data& d,
		const secondary_window_desc& desc
	) -> window_surface*;

	auto destroy_secondary(
		data& d,
		window_surface* surface
	) -> void;

	[[nodiscard]] auto find_surface(
		data& d,
		id id
	) -> window_surface*;

	[[nodiscard]] auto close_requested(
		const window_surface& s
	) -> bool;

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

	[[nodiscard]] auto minimized(
		const window_surface& s
	) -> bool;

	[[nodiscard]] auto raw_handle(
		const window_surface& s
	) -> native_window_handle;

	[[nodiscard]] auto frame_buffer_resized(
		window_surface& s
	) -> bool;

	[[nodiscard]] auto viewport(
		const data& d
	) -> vec2i;

	[[nodiscard]] auto viewport(
		shared_view<data> d
	) -> vec2i;

	[[nodiscard]] auto viewport(
		const window_surface& s
	) -> vec2i;

	[[nodiscard]] auto frame_rect(
		const window_surface& s
	) -> rect_t<vec2i>;

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