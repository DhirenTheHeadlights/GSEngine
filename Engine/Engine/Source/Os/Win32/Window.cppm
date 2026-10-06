export module gse.os:window;

import std;

import :input_events;

import gse.core;
import gse.math;
import gse.concurrency;
import gse.ecs;
import gse.meta;
import gse.gpu_backend;

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

	struct window_focus_request {
		id window;
	};

	struct window_resize_request {
		id window;
		vec2i size{ 0, 0 };
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
		gpu::present_mode present_mode = gpu::present_mode::fifo;
		std::string for_menu;
	};

	struct window_popout_failed {
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
		return formatter<string>::format(
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
		[[= shared]] bool should_close = false;
		bool custom_frame = false;
		bool cursor_capture_active = false;
		std::uint8_t mouse_button_mask = 0;
		std::uint32_t pending_high_surrogate = 0;
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
		[[= shared]] gpu::present_mode present_mode = gpu::present_mode::fifo;
		bool attached = false;
		[[= shared]] task::concurrent_queue<input::event> input_events;
	};

	struct [[= settings::category<"Window">{}, = system_state<"Window">{}]] data {
		[[
			= settings::
				describe<"Windowed, borderless fullscreen, or exclusive fullscreen on the selected monitor.">{}
		]]
		gse::display_mode display_mode = gse::display_mode::windowed;

		[[
			= settings::describe<"Show the system mouse cursor over the window.">{},
			= shared
		]]
		bool mouse_visible = false;

		[[
			= settings::describe<"Monitor that hosts the window in fullscreen mode.">{}
		]]
		settings::choice monitor;

		[[
			= settings::describe<"Resolution and refresh rate used when fullscreen.">{}
		]]
		settings::choice resolution;

		[[
			= settings::describe<"Vulkan present mode. FIFO is vsync (no tearing). Mailbox is low-latency "
									  "vsync. Immediate has tearing "
									  "but lowest latency. FIFO Relaxed is FIFO with tear-on-late-frame.">{}
		]]
		gpu::present_mode present_mode = gpu::present_mode::fifo;

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
		[[= shared]] gpu::present_mode current_present_mode = gpu::present_mode::fifo;
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

		[[= shared]] bool current_cursor_captured = false;
		bool restore_maximized = false;
		int last_monitor_index = 0;
		int current_monitor_index = -1;
		bool launcher_active = false;
		vec2i launch_launcher_size{ 0, 0 };
		std::optional<geometry> launcher_restore;

		[[
			= settings::describe<"Keep the OS window frame instead of drawing application chrome inside the client area.">{},
			= settings::app_scope{}
		]]
		bool native_frame = false;

		[[= shared]] id focused_window;
		[[= shared]] id cursor_window;
		[[= shared]] window_surface primary;
		std::vector<std::unique_ptr<window_surface>> secondaries;
		std::uint32_t housekeeping_frame = 0;
	};

	auto tick(
		scheduler& sched,
		data& d
	) -> void;

	[[= system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;

	auto wait_events(
		time timeout
	) -> void;

	auto install_resize_pump(
		std::function<void()> pump
	) -> void;

	auto post_wake() -> void;

	[[nodiscard]] auto find_surface(
		data& d,
		id id
	) -> window_surface*;

	[[nodiscard]] auto minimized(
		native_window_handle handle
	) -> bool;

	[[nodiscard]] auto viewport(
		native_window_handle handle
	) -> vec2i;

	[[nodiscard]] auto frame_buffer_resized(
		window_surface& s
	) -> bool;

	auto show(
		data& d
	) -> void;
}