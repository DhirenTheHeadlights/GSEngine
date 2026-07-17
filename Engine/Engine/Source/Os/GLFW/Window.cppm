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

	struct window_minimize_request {};

	struct window_toggle_maximize_request {};

	struct window_chrome_metrics_request {
		int caption_height = 0;
		int controls_width = 0;
		int interactive_x0 = 0;
		int interactive_x1 = 0;
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

		[[= gse::shared]] native_window_handle handle;
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
		int chrome_interactive_x0 = 0;
		int chrome_interactive_x1 = 0;

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

	auto apply_commands(
		data& d
	) -> void;

	auto install_native_frame(
		native_window_handle handle,
		const int* caption_height,
		const int* controls_width,
		const int* interactive_x0,
		const int* interactive_x1
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
		shared_view<data> d
	) -> native_window_handle;

	auto show(
		const data& d
	) -> void;

	auto show(
		shared_view<data> d
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

