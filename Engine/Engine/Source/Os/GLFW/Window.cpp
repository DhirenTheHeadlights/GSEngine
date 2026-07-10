module gse.os:window_impl;

import std;

import :window;
import :keys;
import :input_events;

import gse.glfw;
import gse.win32;

import gse.assert;
import gse.math;
import gse.core;
import gse.concurrency;
import gse.ecs;

namespace gse {
	auto to_glfw_handle(
		native_window_handle handle
	) -> GLFWwindow*;

	auto to_native_handle(
		GLFWwindow* handle
	) -> native_window_handle;

	auto window_handle_open(
		native_window_handle handle
	) -> bool;

	auto window_handle_minimized(
		native_window_handle handle
	) -> bool;

	auto window_handle_viewport(
		native_window_handle handle
	) -> vec2i;

	auto window_handle_show(
		native_window_handle handle
	) -> void;

	auto refresh_monitor_settings(
		window::data& d
	) -> void;

	auto refresh_resolution_settings(
		window::data& d
	) -> void;

	auto refresh_display_mode_settings(
		window::data& d
	) -> void;

	auto refresh_present_mode_settings(
		window::data& d
	) -> void;

	auto apply_cursor_mode(
		const window::data& d
	) -> void;

	auto apply_display_mode(
		window::data& d,
		display_mode mode
	) -> void;

	auto move_window_to_monitor(
		const window::data& d,
		int monitor_index
	) -> void;

	auto to_input_key(
		int glfw_key
	) -> std::optional<key>;

	auto to_input_mouse_button(
		int glfw_button
	) -> std::optional<mouse_button>;

	auto create_window(
		window::data& d
	) -> void;
}

auto gse::to_glfw_handle(const native_window_handle handle) -> GLFWwindow* {
	return static_cast<GLFWwindow*>(handle.value);
}

auto gse::to_native_handle(GLFWwindow* handle) -> native_window_handle {
	return {
		.value = handle
	};
}

auto gse::to_input_key(const int glfw_key) -> std::optional<key> {
	if (glfw_key >= glfw::key_space && glfw_key <= glfw::key_last) {
		return static_cast<key>(glfw_key);
	}
	return std::nullopt;
}

auto gse::to_input_mouse_button(const int glfw_button) -> std::optional<mouse_button> {
	if (glfw_button >= glfw::mouse_button_1 && glfw_button <= glfw::mouse_button_last) {
		return static_cast<mouse_button>(glfw_button);
	}
	return std::nullopt;
}

auto gse::refresh_monitor_settings(window::data& d) -> void {
	const auto monitors = window::enumerate_monitors();

	d.monitor.options.clear();
	for (const auto& monitor : monitors) {
		d.monitor.options.push_back(std::format("{}: {}x{}", monitor.name, monitor.width, monitor.height));
	}

	if (d.monitor.value < 0 || d.monitor.value >= static_cast<int>(monitors.size())) {
		d.monitor.value = 0;
	}
}

auto gse::refresh_resolution_settings(window::data& d) -> void {
	const auto resolutions = window::enumerate_resolutions(d.monitor.value);

	d.resolution.options.clear();
	d.resolution.options.emplace_back("Native");
	for (const auto& resolution : resolutions) {
		d.resolution.options.push_back(std::format("{}", resolution));
	}

	if (d.resolution.value < 0 || d.resolution.value >= static_cast<int>(d.resolution.options.size())) {
		d.resolution.value = 0;
	}
}

auto gse::refresh_display_mode_settings(window::data& d) -> void {
	d.display_mode.options = {
		"Windowed",
		"Borderless Fullscreen",
		"Exclusive Fullscreen",
	};

	if (d.display_mode.value < 0 || d.display_mode.value >= static_cast<int>(d.display_mode.options.size())) {
		d.display_mode.value = 0;
	}
}

auto gse::refresh_present_mode_settings(window::data& d) -> void {
	d.present_mode.options = {
		"FIFO (VSync)",
		"FIFO Relaxed",
		"Mailbox",
		"Immediate",
	};

	if (d.present_mode.value < 0 || d.present_mode.value >= static_cast<int>(d.present_mode.options.size())) {
		d.present_mode.value = 0;
	}
}

auto gse::apply_cursor_mode(const window::data& d) -> void {
	auto* handle = to_glfw_handle(d.handle);
	const int target_mode = d.mouse_visible ? glfw::cursor_normal : glfw::cursor_disabled;
	const int current_mode = glfwGetInputMode(handle, glfw::cursor);
	if (current_mode == target_mode) {
		return;
	}

	if (current_mode == glfw::cursor_disabled && target_mode == glfw::cursor_normal) {
		const auto dims = window_handle_viewport(d.handle);
		glfwSetCursorPos(handle, dims.x() / 2.0, dims.y() / 2.0);
	}

	glfwSetInputMode(handle, glfw::cursor, target_mode);
}

auto gse::move_window_to_monitor(const window::data& d, const int monitor_index) -> void {
	auto* handle = to_glfw_handle(d.handle);
	int monitor_count = 0;
	GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
	if (!monitors || monitor_index < 0 || monitor_index >= monitor_count) {
		return;
	}

	int mx = 0;
	int my = 0;
	int mw = 0;
	int mh = 0;
	glfwGetMonitorWorkarea(monitors[monitor_index], &mx, &my, &mw, &mh);

	int ww = 0;
	int wh = 0;
	glfwGetWindowSize(handle, &ww, &wh);

	const int new_x = mx + (mw - ww) / 2;
	const int new_y = my + (mh - wh) / 2;
	glfwSetWindowPos(handle, new_x, new_y);
}

auto gse::apply_display_mode(window::data& d, const display_mode mode) -> void {
	if (d.current_display_mode == mode) {
		return;
	}

	auto* handle = to_glfw_handle(d.handle);
	const bool was_windowed = d.current_display_mode == display_mode::windowed;
	const bool will_be_windowed = mode == display_mode::windowed;

	if (was_windowed && !will_be_windowed) {
		int x = 0;
		int y = 0;
		int w = 0;
		int h = 0;
		glfwGetWindowPos(handle, &x, &y);
		glfwGetWindowSize(handle, &w, &h);
		d.windowed_rect = rect_t<vec2i>::from_position_size(
			{ x, y },
			{ w, h }
		);
	}

	d.current_display_mode = mode;

	if (will_be_windowed) {
		const auto pos = d.windowed_rect.top_left();
		const auto size = d.windowed_rect.size();
		glfwSetWindowMonitor(handle, nullptr, pos.x(), pos.y(), size.x(), size.y(), 0);
		return;
	}

	int monitor_count = 0;
	GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
	assert(monitor_count > 0, "Failed to get monitors!");

	const int selected_monitor = std::clamp(d.monitor.value, 0, monitor_count - 1);
	GLFWmonitor* target_monitor = monitors[selected_monitor];

	int target_width = 0;
	int target_height = 0;
	int target_refresh = 0;

	if (d.resolution.value == 0) {
		const GLFWvidmode* native_mode = glfwGetVideoMode(target_monitor);
		target_width = native_mode->width;
		target_height = native_mode->height;
		target_refresh = native_mode->refreshRate;
	}
	else {
		const auto resolutions = window::enumerate_resolutions(selected_monitor);

		if (const int res_idx = d.resolution.value - 1; res_idx >= 0 && res_idx < static_cast<int>(resolutions.size())) {
			target_width = resolutions[res_idx].width;
			target_height = resolutions[res_idx].height;
			target_refresh = resolutions[res_idx].refresh_rate;
		}
		else {
			const GLFWvidmode* native_mode = glfwGetVideoMode(target_monitor);
			target_width = native_mode->width;
			target_height = native_mode->height;
			target_refresh = native_mode->refreshRate;
		}
	}

	glfwSetWindowMonitor(handle, target_monitor, 0, 0, target_width, target_height, target_refresh);
}

auto gse::create_window(window::data& d) -> void {
	assert(glfwInit(), "Error initializing GLFW");

	if (d.title.empty()) {
		d.title = "GSEngine";
	}

	glfwWindowHint(glfw::client_api, glfw::no_api);
	glfwWindowHint(glfw::resizable, glfw::true_);
	glfwWindowHint(glfw::focus_on_show, glfw::true_);
	glfwWindowHint(glfw::visible, glfw::false_);
	glfwWindowHint(glfw::decorated, d.decorated ? glfw::true_ : glfw::false_);

	const auto initial_size = d.windowed_rect.size();
	auto* handle = glfwCreateWindow(initial_size.x(), initial_size.y(), d.title.c_str(), nullptr, nullptr);
	assert(handle != nullptr, "Failed to create GLFW window!");
	d.handle = to_native_handle(handle);

	glfwSetWindowUserPointer(handle, &d);

	glfwSetKeyCallback(
		handle,
		[](GLFWwindow* w, const int key, int, const int action, int) {
			auto* self = static_cast<window::data*>(glfwGetWindowUserPointer(w));
			if (!self) {
				return;
			}
			const auto mapped = to_input_key(key);
			if (!mapped) {
				return;
			}
			if (action == glfw::press) {
				self->input_events.push(input::key_pressed{
					.key_code = *mapped
				});
			}
			else if (action == glfw::release) {
				self->input_events.push(input::key_released{
					.key_code = *mapped
				});
			}
		}
	);

	glfwSetMouseButtonCallback(
		handle,
		[](GLFWwindow* w, const int button, const int action, int) {
			auto* self = static_cast<window::data*>(glfwGetWindowUserPointer(w));
			if (!self) {
				return;
			}
			const auto mapped = to_input_mouse_button(button);
			if (!mapped) {
				return;
			}
			double x = 0.0;
			double y = 0.0;
			glfwGetCursorPos(w, &x, &y);
			if (action == glfw::press) {
				self->input_events.push(input::mouse_button_pressed{ *mapped, x, y });
			}
			else if (action == glfw::release) {
				self->input_events.push(input::mouse_button_released{ *mapped, x, y });
			}
		}
	);

	glfwSetCursorPosCallback(
		handle,
		[](GLFWwindow* w, double xpos, double ypos) {
			auto* self = static_cast<window::data*>(glfwGetWindowUserPointer(w));
			if (!self) {
				return;
			}

			if (self->ui_focus) {
				const auto dims = window_handle_viewport(self->handle);
				const double clamped_x = std::clamp(xpos, 0.0, static_cast<double>(dims.x()));
				const double clamped_y = std::clamp(ypos, 0.0, static_cast<double>(dims.y()));

				if (clamped_x != xpos || clamped_y != ypos) {
					glfwSetCursorPos(w, clamped_x, clamped_y);
				}

				const double inverted_y = static_cast<double>(dims.y()) - clamped_y;
				self->input_events.push(input::mouse_moved{ clamped_x, inverted_y });
			}
			else {
				self->input_events.push(input::mouse_moved{ xpos, ypos });
			}
		}
	);

	glfwSetScrollCallback(
		handle,
		[](GLFWwindow* w, const double xoffset, const double yoffset) {
			if (auto* self = static_cast<window::data*>(glfwGetWindowUserPointer(w))) {
				self->input_events.push(input::mouse_scrolled{ xoffset, yoffset });
			}
		}
	);

	glfwSetCharCallback(
		handle,
		[](GLFWwindow* w, const unsigned int codepoint) {
			if (auto* self = static_cast<window::data*>(glfwGetWindowUserPointer(w))) {
				self->input_events.push(input::text_entered{ codepoint });
			}
		}
	);

	glfwSetWindowFocusCallback(
		handle,
		[](GLFWwindow* w, const int focused) {
			auto* self = static_cast<window::data*>(glfwGetWindowUserPointer(w));
			if (!self) {
				return;
			}
			self->focused = (focused == glfw::true_);
		}
	);

	glfwSetFramebufferSizeCallback(
		handle,
		[](GLFWwindow* w, const int, const int) {
			if (auto* self = static_cast<window::data*>(glfwGetWindowUserPointer(w))) {
				self->framebuffer_resized = true;
			}
		}
	);

	const int cursor_mode = d.mouse_visible ? glfw::cursor_normal : glfw::cursor_disabled;
	glfwSetInputMode(handle, glfw::cursor, cursor_mode);

	refresh_monitor_settings(d);
	d.last_monitor_index = d.monitor.value;
	refresh_resolution_settings(d);
	refresh_display_mode_settings(d);
	refresh_present_mode_settings(d);

	d.current_present_mode_index = d.present_mode.value;

	glfwFocusWindow(handle);

	if (d.native_frame) {
		window::install_native_frame(d.handle, &d.chrome_caption_height, &d.chrome_controls_width, &d.chrome_interactive_x0, &d.chrome_interactive_x1);
	}
}

namespace gse::window {
	std::mutex clipboard_mutex;
	std::string clipboard_cache;
	std::optional<std::string> clipboard_pending;
	bool clipboard_primed = false;
	bool clipboard_was_focused = false;

	auto sync_clipboard(const bool focused) -> void {
		std::optional<std::string> to_write;
		{
			const std::scoped_lock lock(clipboard_mutex);
			to_write = std::exchange(clipboard_pending, std::nullopt);
		}
		if (to_write) {
			glfwSetClipboardString(nullptr, to_write->c_str());
			const std::scoped_lock lock(clipboard_mutex);
			clipboard_cache = std::move(*to_write);
		}

		const bool gained_focus = focused && !clipboard_was_focused;
		clipboard_was_focused = focused;
		if (clipboard_primed && !gained_focus) {
			return;
		}
		clipboard_primed = true;
		if (const char* contents = glfwGetClipboardString(nullptr)) {
			const std::scoped_lock lock(clipboard_mutex);
			clipboard_cache.assign(contents);
		}
	}
}

auto gse::window::clipboard_text() -> std::string {
	const std::scoped_lock lock(clipboard_mutex);
	if (clipboard_pending) {
		return *clipboard_pending;
	}
	return clipboard_cache;
}

auto gse::window::set_clipboard_text(std::string text) -> void {
	const std::scoped_lock lock(clipboard_mutex);
	clipboard_pending = std::move(text);
}

namespace gse {
	GLFWcursor* g_cursor_arrow = nullptr;
	GLFWcursor* g_cursor_hand = nullptr;
	cursor_shape g_cursor_shape = cursor_shape::arrow;
}

auto gse::window::tick(scheduler& sched, data& d) -> void {
	if (!d.handle) {
		create_window(d);
	}

	poll_events();
	sync_clipboard(d.focused);

	for (const auto& [focus] : sched.read_channel<ui_focus_request>()) {
		set_ui_focus(d, focus);
	}

	for ([[maybe_unused]] const auto& req : sched.read_channel<window_minimize_request>()) {
		d.cmd_minimize = true;
	}

	for ([[maybe_unused]] const auto& req : sched.read_channel<window_toggle_maximize_request>()) {
		d.cmd_toggle_maximize = true;
	}

	for (const auto& req : sched.read_channel<window_chrome_metrics_request>()) {
		d.chrome_caption_height = req.caption_height;
		d.chrome_controls_width = req.controls_width;
		d.chrome_interactive_x0 = req.interactive_x0;
		d.chrome_interactive_x1 = req.interactive_x1;
	}

	for (const auto& req : sched.read_channel<set_cursor_shape_request>()) {
		if (req.shape != g_cursor_shape) {
			if (!g_cursor_arrow) {
				g_cursor_arrow = glfwCreateStandardCursor(glfw::arrow_cursor);
			}
			if (!g_cursor_hand) {
				g_cursor_hand = glfwCreateStandardCursor(glfw::pointing_hand_cursor);
			}
			glfwSetCursor(to_glfw_handle(d.handle), req.shape == cursor_shape::hand ? g_cursor_hand : g_cursor_arrow);
			g_cursor_shape = req.shape;
		}
	}

	if (d.monitor.value != d.last_monitor_index) {
		const int old_monitor = d.last_monitor_index;
		d.last_monitor_index = d.monitor.value;

		if (d.current_display_mode == display_mode::windowed && old_monitor != d.monitor.value) {
			move_window_to_monitor(d, d.monitor.value);
		}
	}

	if (d.focused) {
		apply_cursor_mode(d);

		const auto desired_display_mode = static_cast<display_mode>(d.display_mode.value);
		if (d.current_display_mode != desired_display_mode) {
			apply_display_mode(d, desired_display_mode);
		}

		if (d.current_present_mode_index != d.present_mode.value) {
			d.current_present_mode_index = d.present_mode.value;
			d.framebuffer_resized = true;
		}
	}

	apply_commands(d);
}

auto gse::window::shutdown(data& d) -> void {
	if (d.handle) {
		glfwDestroyWindow(to_glfw_handle(d.handle));
		d.handle = {};
	}
}

auto gse::window::poll_events() -> void {
	glfwPollEvents();
}

auto gse::window::apply_commands(data& d) -> void {
	if (!d.handle) {
		return;
	}

	auto* handle = to_glfw_handle(d.handle);

	if (d.cmd_minimize) {
		glfwIconifyWindow(handle);
		d.cmd_minimize = false;
	}

	if (d.cmd_toggle_maximize) {
		if (glfwGetWindowAttrib(handle, glfw::maximized)) {
			glfwRestoreWindow(handle);
		}
		else {
			glfwMaximizeWindow(handle);
		}
		d.cmd_toggle_maximize = false;
	}

	int win_x = 0;
	int win_y = 0;
	int win_w = 0;
	int win_h = 0;
	glfwGetWindowPos(handle, &win_x, &win_y);
	glfwGetWindowSize(handle, &win_w, &win_h);
	d.position = vec2i{ win_x, win_y };
	d.size = vec2i{ win_w, win_h };
	d.maximized = glfwGetWindowAttrib(handle, glfw::maximized) != 0;
}

#ifdef _WIN32
namespace gse {
	using namespace gse::win32;

	struct native_frame_state {
		WNDPROC original_proc = nullptr;
		const int* caption_height = nullptr;
		const int* controls_width = nullptr;
		const int* interactive_x0 = nullptr;
		const int* interactive_x1 = nullptr;
	};

	LRESULT native_frame_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		auto* state = static_cast<native_frame_state*>(GetPropW(hwnd, L"gse_native_frame"));
		if (state == nullptr) {
			return DefWindowProcW(hwnd, msg, wparam, lparam);
		}

		if (msg == wm_nccalcsize && wparam != 0) {
			if (IsZoomed(hwnd)) {
				if (const HMONITOR monitor = MonitorFromWindow(hwnd, monitor_default_to_nearest)) {
					MONITORINFO info{};
					info.cbSize = sizeof(info);
					if (GetMonitorInfoW(monitor, &info)) {
						reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam)->rgrc[0] = info.rcWork;
					}
				}
			}
			return 0;
		}

		if (msg == wm_nchittest) {
			POINT cursor{ get_x_lparam(lparam), get_y_lparam(lparam) };
			ScreenToClient(hwnd, &cursor);
			RECT client{};
			GetClientRect(hwnd, &client);

			constexpr int border = 8;
			const bool left = cursor.x < border;
			const bool right = cursor.x >= client.right - border;
			const bool top = cursor.y < border;
			const bool bottom = cursor.y >= client.bottom - border;

			if (!IsZoomed(hwnd)) {
				if (top && left) {
					return ht_top_left;
				}
				if (top && right) {
					return ht_top_right;
				}
				if (bottom && left) {
					return ht_bottom_left;
				}
				if (bottom && right) {
					return ht_bottom_right;
				}
				if (left) {
					return ht_left;
				}
				if (right) {
					return ht_right;
				}
				if (top) {
					return ht_top;
				}
				if (bottom) {
					return ht_bottom;
				}
			}

			const int caption = state->caption_height ? *state->caption_height : 0;
			const int controls = state->controls_width ? *state->controls_width : 0;
			if (cursor.y < caption) {
				if (cursor.x >= client.right - controls) {
					return ht_client;
				}
				const int interactive_x0 = state->interactive_x0 ? *state->interactive_x0 : 0;
				const int interactive_x1 = state->interactive_x1 ? *state->interactive_x1 : 0;
				if (interactive_x1 > interactive_x0 && cursor.x >= interactive_x0 && cursor.x < interactive_x1) {
					return ht_client;
				}
				return ht_caption;
			}
			return ht_client;
		}

		return CallWindowProcW(state->original_proc, hwnd, msg, wparam, lparam);
	}
}
#endif

auto gse::window::install_native_frame(const native_window_handle handle, const int* caption_height, const int* controls_width, const int* interactive_x0, const int* interactive_x1) -> void {
#ifdef _WIN32
	using namespace gse::win32;

	const HWND hwnd = glfwGetWin32Window(to_glfw_handle(handle));
	if (hwnd == nullptr) {
		return;
	}

	auto* state = new native_frame_state{};
	state->caption_height = caption_height;
	state->controls_width = controls_width;
	state->interactive_x0 = interactive_x0;
	state->interactive_x1 = interactive_x1;
	state->original_proc = reinterpret_cast<WNDPROC>(
		SetWindowLongPtrW(hwnd, gwlp_wndproc, reinterpret_cast<LONG_PTR>(&native_frame_proc))
	);
	SetPropW(hwnd, L"gse_native_frame", state);
	SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, swp_frame_changed | swp_no_move | swp_no_size | swp_no_zorder | swp_no_activate);
#else
	(void)handle;
	(void)caption_height;
	(void)controls_width;
	(void)interactive_x0;
	(void)interactive_x1;
#endif
}

auto gse::window::is_open(const data& d) -> bool {
	return window_handle_open(d.handle);
}

auto gse::window::is_open(const shared_view<data> d) -> bool {
	return window_handle_open(d.handle);
}

auto gse::window::minimized(const data& d) -> bool {
	return window_handle_minimized(d.handle);
}

auto gse::window::minimized(const shared_view<data> d) -> bool {
	return window_handle_minimized(d.handle);
}

auto gse::window::viewport(const data& d) -> vec2i {
	return window_handle_viewport(d.handle);
}

auto gse::window::viewport(const shared_view<data> d) -> vec2i {
	return window_handle_viewport(d.handle);
}

auto gse::window::raw_handle(const shared_view<data> d) -> native_window_handle {
	return d.handle;
}

auto gse::window::show(const data& d) -> void {
	window_handle_show(d.handle);
}

auto gse::window::show(const shared_view<data> d) -> void {
	window_handle_show(d.handle);
}

auto gse::window::ui_focus(const shared_view<data> d) -> bool {
	return d.ui_focus;
}

auto gse::window_handle_open(const native_window_handle handle) -> bool {
	return !glfwWindowShouldClose(to_glfw_handle(handle));
}

auto gse::window_handle_minimized(const native_window_handle handle) -> bool {
	auto* glfw_handle = to_glfw_handle(handle);
	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(glfw_handle, &width, &height);
	return width == 0 || height == 0;
}

auto gse::window_handle_viewport(const native_window_handle handle) -> vec2i {
	auto* glfw_handle = to_glfw_handle(handle);
	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(glfw_handle, &width, &height);
	return { width, height };
}

auto gse::window::frame_buffer_resized(data& d) -> bool {
	if (d.framebuffer_resized) {
		d.framebuffer_resized = false;
		return true;
	}
	return false;
}

auto gse::window_handle_show(const native_window_handle handle) -> void {
	if (handle) {
		glfwShowWindow(to_glfw_handle(handle));
	}
}

auto gse::window::set_ui_focus(data& d, const bool focus) -> void {
	const bool was = d.ui_focus;
	d.ui_focus = focus;

	if (was || !focus || !d.handle) {
		return;
	}

	const auto dims = window_handle_viewport(d.handle);
	const double center_x = dims.x() / 2.0;
	const double center_y = dims.y() / 2.0;
	glfwSetCursorPos(to_glfw_handle(d.handle), center_x, center_y);
	d.input_events.push(input::mouse_moved{ center_x, dims.y() - center_y });
}

auto gse::window::enumerate_monitors() -> std::vector<monitor_info> {
	std::vector<monitor_info> result;

	int monitor_count = 0;
	GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);

	if (!monitors || monitor_count == 0) {
		return result;
	}

	for (int i = 0; i < monitor_count; ++i) {
		GLFWmonitor* monitor = monitors[i];
		const char* name = glfwGetMonitorName(monitor);
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);

		result.push_back({
			.name = name ? name : std::format("Monitor {}", i + 1),
			.width = mode ? mode->width : 0,
			.height = mode ? mode->height : 0,
			.refresh_rate = mode ? mode->refreshRate : 0,
		});
	}

	return result;
}

auto gse::window::enumerate_resolutions(const int monitor_index) -> std::vector<resolution_info> {
	std::vector<resolution_info> result;

	int monitor_count = 0;
	GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);

	if (!monitors || monitor_count == 0 || monitor_index < 0 || monitor_index >= monitor_count) {
		return result;
	}

	GLFWmonitor* monitor = monitors[monitor_index];
	int mode_count = 0;
	const GLFWvidmode* modes = glfwGetVideoModes(monitor, &mode_count);

	if (!modes || mode_count == 0) {
		return result;
	}

	std::set<std::tuple<int, int, int>> seen;

	for (int i = mode_count - 1; i >= 0; --i) {
		const auto& mode = modes[i];
		auto key = std::make_tuple(mode.width, mode.height, mode.refreshRate);

		if (seen.contains(key)) {
			continue;
		}
		seen.insert(key);

		result.push_back({
			.width = mode.width,
			.height = mode.height,
			.refresh_rate = mode.refreshRate,
		});
	}

	return result;
}
