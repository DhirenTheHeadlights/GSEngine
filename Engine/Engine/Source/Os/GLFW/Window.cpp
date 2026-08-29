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
import gse.diag;
import gse.log;
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

	auto window_handle_content_scale(
		native_window_handle handle
	) -> float;

	auto monitor_key_for_index(
		int index
	) -> std::string;

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

	auto desired_present_mode_index(
		const window::data& d
	) -> int;

	auto apply_cursor_mode(
		window::data& d
	) -> void;

	auto set_cursor_capture(
		const window::window_surface& s,
		bool capture
	) -> void;

	auto apply_display_mode(
		window::data& d,
		display_mode mode
	) -> void;

	auto move_window_to_monitor(
		const window::data& d,
		int monitor_index
	) -> void;

	auto monitor_index_for_window(
		vec2i position,
		vec2i size
	) -> int;

	auto monitor_work_area(
		int monitor_index
	) -> std::optional<rect_t<vec2i>>;

	auto reanchor_saved_geometry(
		window::data& d,
		int monitor_index
	) -> void;

	auto set_window_frame_rect(
		const window::data& d,
		vec2i position,
		vec2i size
	) -> void;

	auto set_surface_frame_rect(
		const window::window_surface& s,
		vec2i position,
		vec2i size
	) -> void;

	[[nodiscard]] auto surface_at(
		window::data& d,
		vec2i screen_point
	) -> window::window_surface*;

	[[nodiscard]] auto pointer_event_rank(
		const input::event& e
	) -> int;

	auto os_restore_geometry(
		const window::data& d
	) -> std::optional<window::geometry>;

	auto probe_composition(
		const window::data& d
	) -> window::composition_probe;

	auto plausible_restore_geometry(
		const window::geometry& g
	) -> bool;

	auto effective_restore_geometry(
		const window::data& d
	) -> window::geometry;

	auto restore_window_geometry(
		window::data& d
	) -> void;

	auto record_window_geometry(
		window::data& d
	) -> void;

	constexpr vec2i default_window_size{ 1920, 1080 };
	constexpr vec2i minimum_restore_size{ 320, 240 };

	auto to_input_key(
		int glfw_key
	) -> std::optional<key>;

	auto to_input_mouse_button(
		int glfw_button
	) -> std::optional<mouse_button>;

	auto create_window(
		window::data& d
	) -> void;

	auto attach_surface_callbacks(
		GLFWwindow* handle,
		window::window_surface& surface
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

auto gse::desired_present_mode_index(const window::data& d) -> int {
	constexpr int mailbox_index = 2;
	return d.attached ? mailbox_index : d.present_mode.value;
}

auto gse::apply_cursor_mode(window::data& d) -> void {
	const bool want_capture = d.primary.focused
		&& !d.cursor_suppressed
		&& (d.primary.cursor_captured || !d.mouse_visible);

#ifdef _WIN32
	set_cursor_capture(d.primary, want_capture);
	d.current_cursor_captured = want_capture;
#else
	if (want_capture == d.current_cursor_captured) {
		return;
	}
	d.current_cursor_captured = want_capture;

	auto* handle = to_glfw_handle(d.primary.handle);
	if (!want_capture) {
		const auto dims = window_handle_viewport(d.primary.handle);
		glfwSetCursorPos(handle, dims.x() / 2.0, dims.y() / 2.0);
	}
	glfwSetInputMode(handle, glfw::cursor, want_capture ? glfw::cursor_disabled : glfw::cursor_normal);
#endif
}

auto gse::monitor_work_area(const int monitor_index) -> std::optional<rect_t<vec2i>> {
	int monitor_count = 0;
	GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
	if (!monitors || monitor_index < 0 || monitor_index >= monitor_count) {
		return std::nullopt;
	}

	int mx = 0;
	int my = 0;
	int mw = 0;
	int mh = 0;
	glfwGetMonitorWorkarea(monitors[monitor_index], &mx, &my, &mw, &mh);

	return rect_t<vec2i>::from_position_size(
		{ mx, my },
		{ mw, mh }
	);
}

auto gse::move_window_to_monitor(const window::data& d, const int monitor_index) -> void {
	const auto work_area = monitor_work_area(monitor_index);
	if (!work_area) {
		return;
	}

	auto* handle = to_glfw_handle(d.primary.handle);
	int ww = 0;
	int wh = 0;
	glfwGetWindowSize(handle, &ww, &wh);

	const vec2i origin = work_area->top_left();
	const vec2i extent = work_area->size();
	const int new_x = origin.x() + (extent.x() - ww) / 2;
	const int new_y = origin.y() + (extent.y() - wh) / 2;
	set_window_frame_rect(d, { new_x, new_y }, { ww, wh });
}

auto gse::monitor_index_for_window(const vec2i position, const vec2i size) -> int {
	int monitor_count = 0;
	GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
	if (!monitors || monitor_count == 0) {
		return -1;
	}

	int best_index = -1;
	int best_overlap = 0;

	for (int i = 0; i < monitor_count; ++i) {
		int mx = 0;
		int my = 0;
		int mw = 0;
		int mh = 0;
		glfwGetMonitorWorkarea(monitors[i], &mx, &my, &mw, &mh);

		const int overlap_x = std::min(position.x() + size.x(), mx + mw) - std::max(position.x(), mx);
		const int overlap_y = std::min(position.y() + size.y(), my + mh) - std::max(position.y(), my);
		if (overlap_x <= 0 || overlap_y <= 0) {
			continue;
		}

		if (const int overlap = overlap_x * overlap_y; overlap > best_overlap) {
			best_overlap = overlap;
			best_index = i;
		}
	}

	return best_index;
}

auto gse::set_window_frame_rect(const window::data& d, const vec2i position, const vec2i size) -> void {
	auto* handle = to_glfw_handle(d.primary.handle);

#ifdef _WIN32
	if (d.native_frame) {
		win32::SetWindowPos(
			win32::hwnd_from_glfw_window(handle),
			nullptr,
			position.x(),
			position.y(),
			size.x(),
			size.y(),
			win32::swp_no_zorder | win32::swp_no_activate
		);
		return;
	}
#endif

	glfwSetWindowSize(handle, size.x(), size.y());
	glfwSetWindowPos(handle, position.x(), position.y());
}

auto gse::set_surface_frame_rect(const window::window_surface& s, const vec2i position, const vec2i size) -> void {
	auto* handle = to_glfw_handle(s.handle);

#ifdef _WIN32
	win32::SetWindowPos(
		win32::hwnd_from_glfw_window(handle),
		nullptr,
		position.x(),
		position.y(),
		size.x(),
		size.y(),
		win32::swp_no_zorder | win32::swp_no_activate
	);
#else
	glfwSetWindowSize(handle, size.x(), size.y());
	glfwSetWindowPos(handle, position.x(), position.y());
#endif
}

auto gse::surface_at(window::data& d, const vec2i screen_point) -> window::window_surface* {
#ifdef _WIN32
	const win32::HWND under = win32::WindowFromPoint({ .x = screen_point.x(), .y = screen_point.y() });
	if (under == nullptr) {
		return nullptr;
	}
	const win32::HWND root = win32::GetAncestor(under, win32::ga_root);
	for (const auto& surface : d.secondaries) {
		if (win32::hwnd_from_glfw_window(to_glfw_handle(surface->handle)) == root) {
			return surface.get();
		}
	}
	return win32::hwnd_from_glfw_window(to_glfw_handle(d.primary.handle)) == root ? &d.primary : nullptr;
#else
	auto covers = [screen_point](const window::window_surface& s) {
		const vec2i local = screen_point - s.position;
		const auto client = window_handle_viewport(s.handle);
		return local.x() >= 0 && local.y() >= 0 && local.x() < client.x() && local.y() < client.y();
	};
	for (const auto& surface : d.secondaries) {
		if (covers(*surface)) {
			return surface.get();
		}
	}
	return covers(d.primary) ? &d.primary : nullptr;
#endif
}

auto gse::pointer_event_rank(const input::event& e) -> int {
	if (std::holds_alternative<input::mouse_scrolled>(e)
		|| std::holds_alternative<input::mouse_button_pressed>(e)
		|| std::holds_alternative<input::mouse_button_released>(e)) {
		return 2;
	}
	return std::holds_alternative<input::mouse_moved>(e) ? 1 : 0;
}

auto gse::os_restore_geometry(const window::data& d) -> std::optional<window::geometry> {
#ifdef _WIN32
	if (!d.native_frame) {
		return std::nullopt;
	}

	const auto hwnd = win32::hwnd_from_glfw_window(to_glfw_handle(d.primary.handle));
	if (hwnd == nullptr) {
		return std::nullopt;
	}

	win32::WINDOWPLACEMENT placement{ .length = static_cast<win32::UINT>(sizeof(win32::WINDOWPLACEMENT)) };
	if (!win32::GetWindowPlacement(hwnd, &placement)) {
		return std::nullopt;
	}

	const auto& normal = placement.rcNormalPosition;
	return window::geometry{
		.x = static_cast<int>(normal.left),
		.y = static_cast<int>(normal.top),
		.width = static_cast<int>(normal.right - normal.left),
		.height = static_cast<int>(normal.bottom - normal.top),
		.maximized = placement.showCmd == win32::sw_show_maximized
			|| (placement.flags & win32::wpf_restore_to_maximized) != 0,
	};
#else
	(void)d;
	return std::nullopt;
#endif
}

auto gse::probe_composition(const window::data& d) -> window::composition_probe {
#ifdef _WIN32
	const auto hwnd = win32::hwnd_from_glfw_window(to_glfw_handle(d.primary.handle));
	if (hwnd == nullptr) {
		return {};
	}

	win32::DWORD cloaked = 0;
	(void)win32::DwmGetWindowAttribute(hwnd, win32::dwmwa_cloaked, &cloaked, static_cast<win32::DWORD>(sizeof(cloaked)));

	return {
		.iconified = win32::IsIconic(hwnd) != 0,
		.visible = win32::IsWindowVisible(hwnd) != 0,
		.cloaked = static_cast<unsigned int>(cloaked),
	};
#else
	(void)d;
	return {};
#endif
}

auto gse::plausible_restore_geometry(const window::geometry& g) -> bool {
	return g.width >= minimum_restore_size.x() && g.height >= minimum_restore_size.y();
}

auto gse::effective_restore_geometry(const window::data& d) -> window::geometry {
	window::geometry g = d.saved_geometry;
	if (!plausible_restore_geometry(g)) {
		g.width = default_window_size.x();
		g.height = default_window_size.y();
	}
	return g;
}

auto gse::restore_window_geometry(window::data& d) -> void {
	const window::geometry saved = effective_restore_geometry(d);
	d.restore_maximized = saved.maximized;

	const auto work_area = monitor_work_area(monitor_index_for_window({ saved.x, saved.y }, { saved.width, saved.height }));
	if (!work_area) {
		return;
	}

	const vec2i origin = work_area->top_left();
	const vec2i extent = work_area->size();

	const int width = std::min(saved.width, extent.x());
	const int height = std::min(saved.height, extent.y());
	const int x = std::clamp(saved.x, origin.x(), origin.x() + extent.x() - width);
	const int y = std::clamp(saved.y, origin.y(), origin.y() + extent.y() - height);

	set_window_frame_rect(d, { x, y }, { width, height });
}

auto gse::reanchor_saved_geometry(window::data& d, const int monitor_index) -> void {
	window::geometry& saved = d.saved_geometry;
	if (saved.width <= 0 || saved.height <= 0) {
		return;
	}

	const int saved_index = monitor_index_for_window({ saved.x, saved.y }, { saved.width, saved.height });
	if (saved_index == monitor_index) {
		return;
	}

	const auto target = monitor_work_area(monitor_index);
	if (!target) {
		return;
	}

	const vec2i origin = target->top_left();
	const vec2i extent = target->size();
	const int width = std::min(saved.width, extent.x());
	const int height = std::min(saved.height, extent.y());

	vec2i offset{ (extent.x() - width) / 2, (extent.y() - height) / 2 };
	if (const auto source = monitor_work_area(saved_index)) {
		offset = vec2i{ saved.x, saved.y } - source->top_left();
	}

	saved.width = width;
	saved.height = height;
	saved.x = std::clamp(origin.x() + offset.x(), origin.x(), origin.x() + extent.x() - width);
	saved.y = std::clamp(origin.y() + offset.y(), origin.y(), origin.y() + extent.y() - height);
}

auto gse::record_window_geometry(window::data& d) -> void {
	if (d.restore_maximized || d.current_display_mode != display_mode::windowed) {
		return;
	}

	const int monitor_index = monitor_index_for_window(d.primary.position, d.primary.size);

	if (const auto os = os_restore_geometry(d)) {
		if (plausible_restore_geometry(*os)) {
			d.saved_geometry = *os;
		}
	}
	else if (!window_handle_minimized(d.primary.handle)) {
		if (glfwGetWindowAttrib(to_glfw_handle(d.primary.handle), glfw::maximized) != 0) {
			d.saved_geometry.maximized = true;
			reanchor_saved_geometry(d, monitor_index);
		}
		else {
			const window::geometry current{
				.x = d.primary.position.x(),
				.y = d.primary.position.y(),
				.width = d.primary.size.x(),
				.height = d.primary.size.y(),
				.maximized = false,
			};
			if (plausible_restore_geometry(current)) {
				d.saved_geometry = current;
			}
		}
	}

	if (monitor_index >= 0 && monitor_index != d.monitor.value) {
		d.monitor.value = monitor_index;
		d.last_monitor_index = monitor_index;
	}
}

auto gse::apply_display_mode(window::data& d, const display_mode mode) -> void {
	if (d.current_display_mode == mode) {
		return;
	}

	auto* handle = to_glfw_handle(d.primary.handle);
	const bool was_windowed = d.current_display_mode == display_mode::windowed;
	const bool will_be_windowed = mode == display_mode::windowed;

	if (was_windowed && !will_be_windowed) {
		record_window_geometry(d);
	}

	d.current_display_mode = mode;

	if (will_be_windowed) {
		const window::geometry saved = effective_restore_geometry(d);
		const vec2i position{ saved.x, saved.y };
		const vec2i size{ saved.width, saved.height };

		glfwSetWindowMonitor(handle, nullptr, position.x(), position.y(), size.x(), size.y(), 0);
		set_window_frame_rect(d, position, size);

		if (saved.maximized) {
			glfwMaximizeWindow(handle);
		}
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

	const window::geometry initial = effective_restore_geometry(d);
	auto* handle = glfwCreateWindow(initial.width, initial.height, d.title.c_str(), nullptr, nullptr);
	assert(handle != nullptr, "Failed to create GLFW window!");
	d.primary.id = find_or_generate_id("primary_window");
	d.primary.handle = to_native_handle(handle);

	attach_surface_callbacks(handle, d.primary);

	glfwSetInputMode(handle, glfw::cursor, glfw::cursor_normal);

	refresh_monitor_settings(d);
	d.last_monitor_index = d.monitor.value;
	refresh_resolution_settings(d);
	refresh_display_mode_settings(d);
	refresh_present_mode_settings(d);

	d.primary.content_scale = window_handle_content_scale(d.primary.handle);
	d.current_present_mode_index = desired_present_mode_index(d);

	window::install_window_hook(d.primary, d.native_frame);

	restore_window_geometry(d);
}

auto gse::attach_surface_callbacks(GLFWwindow* handle, window::window_surface& surface) -> void {
	glfwSetWindowUserPointer(handle, &surface);

	glfwSetKeyCallback(
		handle,
		[](GLFWwindow* w, const int key, int, const int action, int) {
			auto* self = static_cast<window::window_surface*>(glfwGetWindowUserPointer(w));
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
			auto* self = static_cast<window::window_surface*>(glfwGetWindowUserPointer(w));
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
			if (self->ui_focus) {
				y = static_cast<double>(window_handle_viewport(self->handle).y()) - y;
			}
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
			auto* self = static_cast<window::window_surface*>(glfwGetWindowUserPointer(w));
			if (!self) {
				return;
			}

			if (self->ui_focus) {
				const auto dims = window_handle_viewport(self->handle);
				if (self->cursor_captured) {
					self->input_events.push(input::mouse_moved{ .x_pos = xpos, .y_pos = static_cast<double>(dims.y()) - ypos });
					return;
				}

				if (glfwGetMouseButton(w, glfw::mouse_button_1) == glfw::press) {
					self->input_events.push(input::mouse_moved{ xpos, static_cast<double>(dims.y()) - ypos });
					return;
				}

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
			if (auto* self = static_cast<window::window_surface*>(glfwGetWindowUserPointer(w))) {
				self->input_events.push(input::mouse_scrolled{ xoffset, yoffset });
			}
		}
	);

	glfwSetCharCallback(
		handle,
		[](GLFWwindow* w, const unsigned int codepoint) {
			if (auto* self = static_cast<window::window_surface*>(glfwGetWindowUserPointer(w))) {
				self->input_events.push(input::text_entered{ codepoint });
			}
		}
	);

	glfwSetWindowFocusCallback(
		handle,
		[](GLFWwindow* w, const int focused) {
			auto* self = static_cast<window::window_surface*>(glfwGetWindowUserPointer(w));
			if (!self) {
				return;
			}
			self->focused = (focused == glfw::true_);
		}
	);

	glfwSetFramebufferSizeCallback(
		handle,
		[](GLFWwindow* w, const int, const int) {
			if (auto* self = static_cast<window::window_surface*>(glfwGetWindowUserPointer(w))) {
				self->framebuffer_resized = true;
			}
		}
	);

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
		const char* contents = glfwGetClipboardString(nullptr);
		const std::scoped_lock lock(clipboard_mutex);
		clipboard_cache.assign(contents ? contents : "");
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

namespace gse::window {
	std::mutex clipboard_image_mutex;
	std::optional<clipboard_image> clipboard_image_ready;
	std::atomic<bool> clipboard_image_present{ false };
	std::atomic<bool> clipboard_image_wanted{ false };

#ifdef _WIN32
	auto read_clipboard_dib() -> std::optional<clipboard_image> {
		win32::HANDLE handle = win32::GetClipboardData(win32::cf_dibv5);
		if (!handle) {
			handle = win32::GetClipboardData(win32::cf_dib);
		}
		if (!handle) {
			return std::nullopt;
		}

		const auto* memory = static_cast<const std::byte*>(win32::GlobalLock(static_cast<win32::HGLOBAL>(handle)));
		if (!memory) {
			return std::nullopt;
		}

		const std::size_t available = win32::GlobalSize(static_cast<win32::HGLOBAL>(handle));
		const auto* header = reinterpret_cast<const win32::BITMAPINFOHEADER*>(memory);
		const std::int32_t width = header->biWidth;
		const std::int32_t signed_height = header->biHeight;
		const std::int32_t height = signed_height < 0 ? -signed_height : signed_height;
		const std::uint32_t bits = header->biBitCount;
		const bool top_down = signed_height < 0;
		const bool layout_supported = (bits == 24 || bits == 32)
			&& (header->biCompression == win32::bi_rgb || header->biCompression == win32::bi_bitfields);

		if (width <= 0 || height <= 0 || !layout_supported) {
			win32::GlobalUnlock(static_cast<win32::HGLOBAL>(handle));
			return std::nullopt;
		}

		std::size_t offset = header->biSize;
		if (header->biCompression == win32::bi_bitfields && header->biSize == win32::bitmap_info_header_size) {
			offset += 12;
		}
		offset += static_cast<std::size_t>(header->biClrUsed) * 4;

		const std::size_t stride = ((static_cast<std::size_t>(width) * bits + 31) / 32) * 4;
		if (offset + stride * static_cast<std::size_t>(height) > available) {
			win32::GlobalUnlock(static_cast<win32::HGLOBAL>(handle));
			return std::nullopt;
		}

		const std::size_t source_pixel = bits / 8;
		std::vector<std::byte> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
		bool any_opaque = false;

		for (std::int32_t y = 0; y < height; ++y) {
			const std::size_t source_row = static_cast<std::size_t>(top_down ? y : height - 1 - y);
			const std::byte* source = memory + offset + source_row * stride;
			std::byte* destination = pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4;
			for (std::int32_t x = 0; x < width; ++x) {
				const std::byte* texel = source + static_cast<std::size_t>(x) * source_pixel;
				destination[x * 4 + 0] = texel[2];
				destination[x * 4 + 1] = texel[1];
				destination[x * 4 + 2] = texel[0];
				destination[x * 4 + 3] = bits == 32 ? texel[3] : std::byte{ 0xff };
				any_opaque = any_opaque || destination[x * 4 + 3] != std::byte{ 0 };
			}
		}

		win32::GlobalUnlock(static_cast<win32::HGLOBAL>(handle));

		if (!any_opaque) {
			for (std::size_t i = 3; i < pixels.size(); i += 4) {
				pixels[i] = std::byte{ 0xff };
			}
		}

		return clipboard_image{
			.size = { static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height) },
			.pixels = std::move(pixels),
		};
	}

	auto read_clipboard_file() -> std::optional<clipboard_image> {
		const win32::HANDLE handle = win32::GetClipboardData(win32::cf_hdrop);
		if (!handle) {
			return std::nullopt;
		}

		constexpr std::array image_extensions = {
			std::string_view(".png"),
			std::string_view(".jpg"),
			std::string_view(".jpeg"),
			std::string_view(".bmp"),
			std::string_view(".tga"),
			std::string_view(".gif"),
			std::string_view(".webp"),
		};

		const auto drop = static_cast<win32::HDROP>(handle);
		const win32::UINT count = win32::DragQueryFileW(drop, win32::drag_query_count, nullptr, 0);

		for (win32::UINT i = 0; i < count; ++i) {
			const win32::UINT length = win32::DragQueryFileW(drop, i, nullptr, 0);
			if (length == 0) {
				continue;
			}
			std::wstring name(length + 1, L'\0');
			if (win32::DragQueryFileW(drop, i, name.data(), length + 1) == 0) {
				continue;
			}
			name.resize(length);

			std::filesystem::path path(name);
			std::string extension = path.extension().display_string();
			std::ranges::transform(extension, extension.begin(), [](const char c) {
				return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			});
			if (std::ranges::contains(image_extensions, extension)) {
				return clipboard_image{ .path = std::move(path) };
			}
		}

		return std::nullopt;
	}

	auto sync_clipboard_image() -> void {
		clipboard_image_present.store(
			win32::IsClipboardFormatAvailable(win32::cf_dibv5) != 0
				|| win32::IsClipboardFormatAvailable(win32::cf_dib) != 0,
			std::memory_order_release
		);

		if (!clipboard_image_wanted.exchange(false, std::memory_order_acq_rel)) {
			return;
		}

		if (!win32::OpenClipboard(nullptr)) {
			return;
		}

		std::optional<clipboard_image> found = read_clipboard_dib();
		if (!found) {
			found = read_clipboard_file();
		}
		win32::CloseClipboard();

		const std::scoped_lock lock(clipboard_image_mutex);
		clipboard_image_ready = std::move(found);
	}
#else
	auto sync_clipboard_image() -> void {
	}
#endif
}

auto gse::window::clipboard_image_available() -> bool {
	return clipboard_image_present.load(std::memory_order_acquire);
}

auto gse::window::request_clipboard_image() -> void {
	clipboard_image_wanted.store(true, std::memory_order_release);
}

auto gse::window::take_clipboard_image() -> std::optional<clipboard_image> {
	const std::scoped_lock lock(clipboard_image_mutex);
	return std::exchange(clipboard_image_ready, std::nullopt);
}

namespace gse {
	GLFWcursor* g_cursor_arrow = nullptr;
	GLFWcursor* g_cursor_hand = nullptr;
	GLFWcursor* g_cursor_resize_ew = nullptr;
	GLFWcursor* g_cursor_resize_ns = nullptr;
	GLFWcursor* g_cursor_resize_nwse = nullptr;
	GLFWcursor* g_cursor_resize_nesw = nullptr;
	cursor_shape g_cursor_shape = cursor_shape::arrow;

	auto glfw_cursor_slot(const cursor_shape shape) -> GLFWcursor*& {
		switch (shape) {
			case cursor_shape::hand:
				return g_cursor_hand;
			case cursor_shape::resize_ew:
				return g_cursor_resize_ew;
			case cursor_shape::resize_ns:
				return g_cursor_resize_ns;
			case cursor_shape::resize_nwse:
				return g_cursor_resize_nwse;
			case cursor_shape::resize_nesw:
				return g_cursor_resize_nesw;
			case cursor_shape::arrow:
			default:
				return g_cursor_arrow;
		}
	}

	auto glfw_standard_cursor(const cursor_shape shape) -> int {
		switch (shape) {
			case cursor_shape::hand:
				return glfw::pointing_hand_cursor;
			case cursor_shape::resize_ew:
				return glfw::resize_ew_cursor;
			case cursor_shape::resize_ns:
				return glfw::resize_ns_cursor;
			case cursor_shape::resize_nwse:
				return glfw::resize_nwse_cursor;
			case cursor_shape::resize_nesw:
				return glfw::resize_nesw_cursor;
			case cursor_shape::arrow:
			default:
				return glfw::arrow_cursor;
		}
	}
}

auto gse::window::tick(scheduler& sched, data& d) -> void {
	if (!d.primary.handle) {
		create_window(d);
	}

	{
		trace::scope_guard sg{ trace_id<"window::poll">() };
		poll_events();
	}

	{
		trace::scope_guard sg{ trace_id<"window::clipboard">() };
		sync_clipboard(d.primary.focused);
		sync_clipboard_image();
	}

	{
		trace::scope_guard sg{ trace_id<"window::content_scale">() };
		d.primary.content_scale = window_handle_content_scale(d.primary.handle);
	}

	for (const auto& [focus] : sched.read_channel<ui_focus_request>()) {
		set_ui_focus(d, focus);
	}

	for (const auto& [capture] : sched.read_channel<cursor_capture_request>()) {
		d.primary.cursor_captured = capture;
	}

	for (const auto& req : sched.read_channel<window_popout_request>()) {
		window_surface* created = create_secondary(d, {
			.title = req.title.empty() ? req.menu_name : req.title,
			.size = req.size,
			.position = req.screen_position,
			.use_position = true,
		});
		if (!created) {
			continue;
		}

		sched.make_channel_writer().push<window_opened>({
			.id = created->id,
			.handle = created->handle,
			.position = created->position,
			.size = created->size,
			.present_mode_index = created->present_mode_index,
			.for_menu = req.menu_name,
		});
	}

	for (const auto& req : sched.read_channel<window_minimize_request>()) {
		window_surface* surface = find_surface(d, req.window);
		if (surface == &d.primary) {
			d.cmd_minimize = true;
		}
		else if (surface && surface->handle) {
			glfwIconifyWindow(to_glfw_handle(surface->handle));
		}
	}

	for (const auto& req : sched.read_channel<window_toggle_maximize_request>()) {
		window_surface* surface = find_surface(d, req.window);
		if (surface == &d.primary) {
			d.cmd_toggle_maximize = true;
		}
		else if (surface && surface->handle) {
			GLFWwindow* handle = to_glfw_handle(surface->handle);
			if (glfwGetWindowAttrib(handle, glfw::maximized)) {
				glfwRestoreWindow(handle);
			}
			else {
				glfwMaximizeWindow(handle);
			}
		}
	}

	for (const auto& req : sched.read_channel<window_close_request>()) {
		window_surface* surface = find_surface(d, req.window);
		if (surface == &d.primary) {
			d.cmd_close = true;
		}
		else if (surface && surface->handle) {
			glfwSetWindowShouldClose(to_glfw_handle(surface->handle), glfw::true_);
		}
	}

	for (const auto& req : sched.read_channel<window_open_file_request>()) {
		d.cmd_open_file = true;
		d.cmd_open_file_title = req.title;
		d.cmd_open_file_filter_name = req.filter_name;
		d.cmd_open_file_filter_pattern = req.filter_pattern;
	}

	for (const auto& req : sched.read_channel<window_launcher_mode_request>()) {
		d.cmd_launcher_pending = true;
		d.cmd_launcher_active = req.active;
		d.cmd_launcher_size = { req.width, req.height };
	}

	for (const auto& req : sched.read_channel<window_chrome_metrics_request>()) {
		window_surface* surface = find_surface(d, req.window);
		if (!surface) {
			continue;
		}
		surface->chrome_caption_height = req.caption_height;
		surface->chrome_controls_width = req.controls_width;
		surface->chrome_resize_exclude_y0 = req.resize_exclude_y0;
		surface->chrome_resize_exclude_y1 = req.resize_exclude_y1;
	}

	for (const auto& req : sched.read_channel<window_locate_cursor_request>()) {
		const window_surface* source = find_surface(d, req.source);
		if (!source) {
			continue;
		}

		const auto source_client = window_handle_viewport(source->handle);
		const vec2i screen{
			source->position.x() + static_cast<int>(req.client_cursor.x()),
			source->position.y() + (source_client.y() - static_cast<int>(req.client_cursor.y())),
		};

		window_cursor_located located{ .source = req.source, .screen_cursor = screen };
		if (const window_surface* under = surface_at(d, screen)) {
			const auto client = window_handle_viewport(under->handle);
			const vec2i local = screen - under->position;
			located.window = under == &d.primary ? id() : under->id;
			located.client_cursor = vec2f{
				static_cast<float>(local.x()),
				static_cast<float>(client.y() - local.y()),
			};
		}

		sched.make_channel_writer().push<window_cursor_located>(located);
	}

	cursor_shape desired_cursor = cursor_shape::arrow;
	for (const auto& req : sched.read_channel<set_cursor_shape_request>()) {
		if (req.shape != cursor_shape::arrow) {
			desired_cursor = req.shape;
		}
	}
	if (desired_cursor != g_cursor_shape) {
		GLFWcursor*& slot = glfw_cursor_slot(desired_cursor);
		if (!slot) {
			slot = glfwCreateStandardCursor(glfw_standard_cursor(desired_cursor));
		}
		glfwSetCursor(to_glfw_handle(d.primary.handle), slot);
		for (const auto& surface : d.secondaries) {
			if (surface->handle) {
				glfwSetCursor(to_glfw_handle(surface->handle), slot);
			}
		}
		g_cursor_shape = desired_cursor;
	}

	if (d.monitor.value != d.last_monitor_index) {
		const int old_monitor = d.last_monitor_index;
		d.last_monitor_index = d.monitor.value;

		if (d.current_display_mode == display_mode::windowed && old_monitor != d.monitor.value) {
			move_window_to_monitor(d, d.monitor.value);
		}
	}

	{
		trace::scope_guard sg{ trace_id<"window::cursor_mode">() };
		apply_cursor_mode(d);
	}

	if (d.primary.focused) {
		trace::scope_guard sg{ trace_id<"window::modes">() };
		const auto desired_display_mode = static_cast<display_mode>(d.display_mode.value);
		if (d.current_display_mode != desired_display_mode) {
			apply_display_mode(d, desired_display_mode);
		}

		if (const int desired_present_mode = desired_present_mode_index(d); d.current_present_mode_index != desired_present_mode) {
			d.current_present_mode_index = desired_present_mode;
			d.primary.framebuffer_resized = true;
		}
	}

	d.primary.present_mode_index = d.current_present_mode_index;
	d.primary.attached = d.attached;

	{
		trace::scope_guard sg{ trace_id<"window::commands">() };
		apply_commands(d);
	}

	{
		trace::scope_guard sg{ trace_id<"window::monitor_scan">() };
		if (const int monitor_index = monitor_index_for_window(d.primary.position, d.primary.size); monitor_index != d.current_monitor_index) {
			d.current_monitor_index = monitor_index;
			d.primary.monitor_key = monitor_key_for_index(monitor_index);
		}
	}

	if (d.cmd_open_file) {
		d.cmd_open_file = false;
		sched.make_channel_writer().push<window_open_file_result>({
			.path = prompt_for_file(d),
		});
	}

	{
		trace::scope_guard sg{ trace_id<"window::focus">() };
		const window_surface* focused = &d.primary;
		for (const auto& surface : d.secondaries) {
			if (surface->handle && surface->focused) {
				focused = surface.get();
				break;
			}
		}
		d.focused_window = focused == &d.primary ? id{} : focused->id;

		std::vector<std::pair<const window_surface*, std::vector<input::event>>> drained;
		drained.emplace_back(&d.primary, d.primary.input_events.drain());
		for (const auto& surface : d.secondaries) {
			drained.emplace_back(surface.get(), surface->input_events.drain());
		}

		const window_surface* pointer = nullptr;
		int best_rank = 0;
		for (const auto& [surface, events] : drained) {
			int rank = 0;
			for (const auto& event : events) {
				rank = std::max(rank, pointer_event_rank(event));
			}
			if (rank > 0 && rank >= best_rank) {
				pointer = surface;
				best_rank = rank;
			}
		}

		if (pointer) {
			d.cursor_window = pointer == &d.primary ? id{} : pointer->id;
		}
		else if (const window_surface* held = find_surface(d, d.cursor_window)) {
			pointer = held;
		}
		else {
			pointer = &d.primary;
			d.cursor_window = id{};
		}

		for (const auto& [surface, events] : drained) {
			for (const auto& event : events) {
				if (surface == (pointer_event_rank(event) > 0 ? pointer : focused)) {
					d.primary.input_events.push(event);
				}
			}
		}
	}

	{
		trace::scope_guard sg{ trace_id<"window::secondaries">() };
		for (std::size_t i = d.secondaries.size(); i-- > 0;) {
			if (close_requested(*d.secondaries[i])) {
				sched.make_channel_writer().push<window_closed>({ .id = d.secondaries[i]->id });
				destroy_secondary(d, d.secondaries[i].get());
			}
		}

		for (const auto& surface : d.secondaries) {
			if (!surface->handle) {
				continue;
			}
			GLFWwindow* handle = to_glfw_handle(surface->handle);
			int win_x = 0;
			int win_y = 0;
			int win_w = 0;
			int win_h = 0;
			glfwGetWindowPos(handle, &win_x, &win_y);
			glfwGetWindowSize(handle, &win_w, &win_h);
			if (const vec2i position{ win_x, win_y }; position != surface->position) {
				surface->position = position;
				sched.make_channel_writer().push<window_moved>({ .id = surface->id, .position = position });
			}
			if (const vec2i size{ win_w, win_h }; size != surface->size) {
				surface->size = size;
				sched.make_channel_writer().push<window_resized>({ .id = surface->id, .size = size });
			}
			surface->content_scale = window_handle_content_scale(surface->handle);
		}
	}
}

auto gse::window::prompt_for_file(data& d) -> std::filesystem::path {
#ifdef _WIN32
	if (!d.primary.handle) {
		return {};
	}

	std::wstring filter = std::filesystem::path(d.cmd_open_file_filter_name).wstring();
	filter.push_back(L'\0');
	filter.append(std::filesystem::path(d.cmd_open_file_filter_pattern).wstring());
	filter.push_back(L'\0');
	filter.push_back(L'\0');

	const std::wstring title = std::filesystem::path(d.cmd_open_file_title).wstring();

	std::wstring buffer(win32::max_path, L'\0');
	if (!win32::open_file_dialog(
		win32::hwnd_from_glfw_window(to_glfw_handle(d.primary.handle)),
		title.c_str(),
		filter.c_str(),
		buffer.data(),
		static_cast<win32::DWORD>(buffer.size())
	)) {
		return {};
	}

	buffer.resize(std::wcslen(buffer.c_str()));
	return buffer;
#else
	return {};
#endif
}

auto gse::window::shutdown(data& d) -> void {
	for (const auto& surface : d.secondaries) {
		if (surface->handle) {
			glfwDestroyWindow(to_glfw_handle(surface->handle));
			surface->handle = {};
		}
	}
	d.secondaries.clear();

	if (d.primary.handle) {
		glfwDestroyWindow(to_glfw_handle(d.primary.handle));
		d.primary.handle = {};
	}
}

auto gse::window::poll_events() -> void {
	glfwPollEvents();
}

auto gse::window::apply_commands(data& d) -> void {
	if (!d.primary.handle) {
		return;
	}

	auto* handle = to_glfw_handle(d.primary.handle);

	if (d.cmd_close) {
		glfwSetWindowShouldClose(handle, glfw::true_);
		d.cmd_close = false;
	}

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

	if (d.cmd_launcher_pending) {
		if (d.cmd_launcher_active) {
			const bool was_maximized = glfwGetWindowAttrib(handle, glfw::maximized) != 0;
			if (was_maximized) {
				glfwRestoreWindow(handle);
			}

			int cur_x = 0;
			int cur_y = 0;
			int cur_w = 0;
			int cur_h = 0;
			glfwGetWindowPos(handle, &cur_x, &cur_y);
			glfwGetWindowSize(handle, &cur_w, &cur_h);

			if (d.launcher_saved_size.x() <= 0) {
				d.launcher_saved_position = { cur_x, cur_y };
				d.launcher_saved_size = { cur_w, cur_h };
				d.launcher_saved_maximized = was_maximized;
			}

			const int width = std::max(1, d.cmd_launcher_size.x());
			const int height = std::max(1, d.cmd_launcher_size.y());
			set_window_frame_rect(
				d,
				{ cur_x + (cur_w - width) / 2, cur_y + (cur_h - height) / 2 },
				{ width, height }
			);
		}
		else if (d.launcher_saved_size.x() > 0) {
			set_window_frame_rect(d, d.launcher_saved_position, d.launcher_saved_size);
			if (d.launcher_saved_maximized) {
				glfwMaximizeWindow(handle);
			}
			d.launcher_saved_size = { 0, 0 };
			d.launcher_saved_maximized = false;
		}
		d.cmd_launcher_pending = false;
	}

	int win_x = 0;
	int win_y = 0;
	int win_w = 0;
	int win_h = 0;
	glfwGetWindowPos(handle, &win_x, &win_y);
	glfwGetWindowSize(handle, &win_w, &win_h);

	const bool changed = win_x != d.primary.position.x() || win_y != d.primary.position.y() || win_w != d.primary.size.x() || win_h != d.primary.size.y();

	if (const composition_probe composition = probe_composition(d); composition != d.primary.last_composition) {
		const auto framebuffer = window_handle_viewport(d.primary.handle);
		log::println(
			log::category::render,
			"[window] composition iconified {}->{} visible {}->{} cloaked {}->{} rect={},{} {}x{} fb={}x{}",
			d.primary.last_composition.iconified,
			composition.iconified,
			d.primary.last_composition.visible,
			composition.visible,
			d.primary.last_composition.cloaked,
			composition.cloaked,
			win_x,
			win_y,
			win_w,
			win_h,
			framebuffer.x(),
			framebuffer.y()
		);
		d.primary.last_composition = composition;
	}

	const int previous_monitor = monitor_index_for_window(d.primary.position, d.primary.size);
	const int current_monitor = monitor_index_for_window({ win_x, win_y }, { win_w, win_h });
	const bool resized = win_w != d.primary.size.x() || win_h != d.primary.size.y();

	if (resized || current_monitor != previous_monitor) {
		const auto framebuffer = window_handle_viewport(d.primary.handle);
		log::println(
			log::category::render,
			"[window] rect {},{} {}x{} -> {},{} {}x{} fb={}x{} iconified={} zoomed={} monitor={}->{} setting={}",
			d.primary.position.x(),
			d.primary.position.y(),
			d.primary.size.x(),
			d.primary.size.y(),
			win_x,
			win_y,
			win_w,
			win_h,
			framebuffer.x(),
			framebuffer.y(),
			glfwGetWindowAttrib(handle, glfw::iconified) != 0,
			glfwGetWindowAttrib(handle, glfw::maximized) != 0,
			previous_monitor,
			current_monitor,
			d.monitor.value
		);
	}

	d.primary.position = vec2i{ win_x, win_y };
	d.primary.size = vec2i{ win_w, win_h };

	if (changed && d.launcher_saved_size.x() <= 0) {
		record_window_geometry(d);
	}
}

#ifdef _WIN32
namespace gse {
	using namespace gse::win32;

	struct window_hook_state {
		WNDPROC original_proc = nullptr;
		window::window_surface* surface = nullptr;
		bool custom_frame = false;
		bool captured = false;
	};

	constexpr long minimized_rect_coordinate = -30000;
	constexpr wchar_t window_hook_prop[] = L"gse_window_hook";

	LRESULT window_hook_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		auto* state = static_cast<window_hook_state*>(GetPropW(hwnd, window_hook_prop));
		if (state == nullptr) {
			return DefWindowProcW(hwnd, msg, wparam, lparam);
		}

		if (msg == wm_setcursor && state->captured && low_word(lparam) == ht_client) {
			SetCursor(nullptr);
			return 1;
		}

		if (msg == wm_input && state->captured && state->surface != nullptr) {
			RAWINPUT raw{};
			UINT size = sizeof(raw);
			const UINT written = GetRawInputData(
				reinterpret_cast<HRAWINPUT>(lparam),
				rid_input,
				&raw,
				&size,
				raw_input_header_size
			);

			if (written != static_cast<UINT>(-1)
				&& raw.header.dwType == rim_type_mouse
				&& (raw.data.mouse.usFlags & mouse_move_absolute) == 0) {
				state->surface->input_events.push(input::mouse_raw_moved{
					.x_delta = static_cast<double>(raw.data.mouse.lLastX),
					.y_delta = -static_cast<double>(raw.data.mouse.lLastY),
				});
			}
		}

		if (!state->custom_frame) {
			return CallWindowProcW(state->original_proc, hwnd, msg, wparam, lparam);
		}

		if (msg == wm_nccalcsize && wparam != 0) {
			auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);
			const bool offscreen_rect = params->rgrc[0].left <= minimized_rect_coordinate ||
				params->rgrc[0].top <= minimized_rect_coordinate;

			if (!IsIconic(hwnd) && !offscreen_rect) {
				if (IsZoomed(hwnd)) {
					if (const HMONITOR monitor = MonitorFromRect(&params->rgrc[0], monitor_default_to_nearest)) {
						MONITORINFO info{};
						info.cbSize = sizeof(info);
						if (GetMonitorInfoW(monitor, &info)) {
							params->rgrc[0].left = std::max(params->rgrc[0].left, info.rcWork.left);
							params->rgrc[0].top = std::max(params->rgrc[0].top, info.rcWork.top);
							params->rgrc[0].right = std::min(params->rgrc[0].right, info.rcWork.right);
							params->rgrc[0].bottom = std::min(params->rgrc[0].bottom, info.rcWork.bottom);
						}
					}
				}
				return 0;
			}
		}

		if (msg == wm_ncmousemove && state->surface != nullptr && state->surface->ui_focus) {
			POINT cursor{ get_x_lparam(lparam), get_y_lparam(lparam) };
			ScreenToClient(hwnd, &cursor);
			const auto dims = window_handle_viewport(state->surface->handle);
			state->surface->input_events.push(input::mouse_moved{
				.x_pos = static_cast<double>(cursor.x),
				.y_pos = static_cast<double>(dims.y() - cursor.y),
			});
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
					const int exclude_y0 = state->surface ? state->surface->chrome_resize_exclude_y0 : 0;
					const int exclude_y1 = state->surface ? state->surface->chrome_resize_exclude_y1 : 0;
					if (exclude_y1 > exclude_y0 && cursor.y >= exclude_y0 && cursor.y < exclude_y1) {
						return ht_client;
					}
					return ht_right;
				}
				if (top) {
					return ht_top;
				}
				if (bottom) {
					return ht_bottom;
				}
			}

			const int caption = state->surface ? state->surface->chrome_caption_height : 0;
			const int controls = state->surface ? state->surface->chrome_controls_width : 0;
			if (cursor.y < caption) {
				if (cursor.x >= client.right - controls) {
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

auto gse::window::install_window_hook(window_surface& surface, const bool custom_frame) -> void {
#ifdef _WIN32
	using namespace gse::win32;

	const HWND hwnd = glfwGetWin32Window(to_glfw_handle(surface.handle));
	if (hwnd == nullptr) {
		return;
	}

	auto* state = new window_hook_state{ .surface = &surface, .custom_frame = custom_frame };
	state->original_proc = reinterpret_cast<WNDPROC>(
		SetWindowLongPtrW(hwnd, gwlp_wndproc, reinterpret_cast<LONG_PTR>(&window_hook_proc))
	);
	SetPropW(hwnd, window_hook_prop, state);

	if (custom_frame) {
		SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, swp_frame_changed | swp_no_move | swp_no_size | swp_no_zorder | swp_no_activate);
	}
#else
	(void)surface;
	(void)custom_frame;
#endif
}

auto gse::set_cursor_capture(const window::window_surface& s, const bool capture) -> void {
#ifdef _WIN32
	using namespace gse::win32;

	const HWND hwnd = glfwGetWin32Window(to_glfw_handle(s.handle));
	if (hwnd == nullptr) {
		return;
	}

	auto* state = static_cast<window_hook_state*>(GetPropW(hwnd, window_hook_prop));
	if (state == nullptr) {
		return;
	}

	if (state->captured != capture) {
		state->captured = capture;

		const RAWINPUTDEVICE mouse{
			.usUsagePage = hid_usage_page_generic,
			.usUsage = hid_usage_generic_mouse,
			.dwFlags = capture ? 0u : ridev_remove,
			.hwndTarget = capture ? hwnd : nullptr,
		};
		RegisterRawInputDevices(&mouse, 1, sizeof(mouse));

		if (capture) {
			SetCursor(nullptr);
		}
		else {
			ClipCursor(nullptr);
			SendMessageW(hwnd, wm_setcursor, reinterpret_cast<WPARAM>(hwnd), make_lparam(static_cast<int>(ht_client), static_cast<int>(wm_mousemove)));
		}
	}

	if (capture) {
		RECT client{};
		GetClientRect(hwnd, &client);

		POINT top_left{ .x = client.left, .y = client.top };
		POINT bottom_right{ .x = client.right, .y = client.bottom };
		ClientToScreen(hwnd, &top_left);
		ClientToScreen(hwnd, &bottom_right);

		const RECT clip{
			.left = top_left.x,
			.top = top_left.y,
			.right = bottom_right.x,
			.bottom = bottom_right.y,
		};
		ClipCursor(&clip);
	}
#else
	(void)s;
	(void)capture;
#endif
}

auto gse::window::is_open(const data& d) -> bool {
	return window_handle_open(d.primary.handle);
}

auto gse::window::is_open(const shared_view<data> d) -> bool {
	return window_handle_open(d.primary.handle);
}

auto gse::window::minimized(const data& d) -> bool {
	return window_handle_minimized(d.primary.handle);
}

auto gse::window::minimized(const shared_view<data> d) -> bool {
	return window_handle_minimized(d.primary.handle);
}

auto gse::window::viewport(const data& d) -> vec2i {
	return window_handle_viewport(d.primary.handle);
}

auto gse::window::viewport(const shared_view<data> d) -> vec2i {
	return window_handle_viewport(d.primary.handle);
}

auto gse::window::viewport(const window_surface& s) -> vec2i {
	return window_handle_viewport(s.handle);
}

auto gse::window::minimized(const window_surface& s) -> bool {
	return window_handle_minimized(s.handle);
}

auto gse::window::raw_handle(const window_surface& s) -> native_window_handle {
	return s.handle;
}

auto gse::window::frame_buffer_resized(window_surface& s) -> bool {
	if (s.framebuffer_resized) {
		s.framebuffer_resized = false;
		return true;
	}
	return false;
}

auto gse::window::frame_rect(const window_surface& s) -> rect_t<vec2i> {
	return rect_t<vec2i>::from_position_size(s.position, s.size);
}

auto gse::window::close_requested(const window_surface& s) -> bool {
	return s.handle && glfwWindowShouldClose(to_glfw_handle(s.handle));
}

auto gse::window::create_secondary(data& d, const secondary_window_desc& desc) -> window_surface* {
	glfwWindowHint(glfw::client_api, glfw::no_api);
	glfwWindowHint(glfw::resizable, glfw::true_);
	glfwWindowHint(glfw::focus_on_show, glfw::true_);
	glfwWindowHint(glfw::visible, glfw::false_);
	glfwWindowHint(glfw::decorated, glfw::true_);

	GLFWwindow* handle = glfwCreateWindow(
		desc.size.x(),
		desc.size.y(),
		desc.title.c_str(),
		nullptr,
		nullptr
	);
	if (!handle) {
		return nullptr;
	}

	auto surface = std::make_unique<window_surface>();
	surface->id = generate_temp_id(stable_id(desc.title));
	surface->handle = to_native_handle(handle);
	surface->size = desc.size;
	surface->ui_focus = true;
	surface->content_scale = window_handle_content_scale(surface->handle);

	attach_surface_callbacks(handle, *surface);
	glfwSetInputMode(handle, glfw::cursor, glfw::cursor_normal);
	install_window_hook(*surface, true);

	if (desc.use_position) {
		set_surface_frame_rect(*surface, desc.position, desc.size);
		surface->position = desc.position;
	}

	window_handle_show(surface->handle);
	surface->shown = true;

	window_surface* raw = surface.get();
	d.secondaries.push_back(std::move(surface));
	return raw;
}

auto gse::window::find_surface(data& d, const id id) -> window_surface* {
	if (!id.exists() || d.primary.id == id) {
		return &d.primary;
	}
	const auto it = std::ranges::find_if(d.secondaries, [id](const auto& held) {
		return held->id == id;
	});
	return it == d.secondaries.end() ? nullptr : it->get();
}

auto gse::window::destroy_secondary(data& d, window_surface* surface) -> void {
	const auto it = std::ranges::find_if(d.secondaries, [surface](const auto& held) {
		return held.get() == surface;
	});
	if (it == d.secondaries.end()) {
		return;
	}
	if ((*it)->handle) {
		glfwDestroyWindow(to_glfw_handle((*it)->handle));
		(*it)->handle = {};
	}
	d.secondaries.erase(it);
}

auto gse::window::raw_handle(const data& d) -> native_window_handle {
	return d.primary.handle;
}

auto gse::window::raw_handle(const shared_view<data> d) -> native_window_handle {
	return d.primary.handle;
}

auto gse::window::show(data& d) -> void {
	window_handle_show(d.primary.handle);
	d.primary.shown = true;

	if (std::exchange(d.restore_maximized, false)) {
		glfwMaximizeWindow(to_glfw_handle(d.primary.handle));
	}
}

auto gse::window::ui_focus(const shared_view<data> d) -> bool {
	return d.primary.ui_focus;
}

auto gse::window_handle_open(const native_window_handle handle) -> bool {
	return !glfwWindowShouldClose(to_glfw_handle(handle));
}

auto gse::window_handle_minimized(const native_window_handle handle) -> bool {
	auto* glfw_handle = to_glfw_handle(handle);
	if (glfwGetWindowAttrib(glfw_handle, glfw::iconified) != 0) {
		return true;
	}
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
	return frame_buffer_resized(d.primary);
}

auto gse::monitor_key_for_index(const int index) -> std::string {
	if (index < 0) {
		return {};
	}

	const auto monitors = window::enumerate_monitors();
	if (index >= static_cast<int>(monitors.size())) {
		return {};
	}

	const auto& info = monitors[index];
	return std::format("{} {}x{}", info.name, info.width, info.height);
}

auto gse::window_handle_content_scale(const native_window_handle handle) -> float {
	if (!handle) {
		return 1.f;
	}

	float x_scale = 1.f;
	float y_scale = 1.f;
	glfwGetWindowContentScale(to_glfw_handle(handle), &x_scale, &y_scale);
	return x_scale > 0.f ? x_scale : 1.f;
}

auto gse::window_handle_show(const native_window_handle handle) -> void {
	if (handle) {
		glfwShowWindow(to_glfw_handle(handle));
	}
}

auto gse::window::set_ui_focus(data& d, const bool focus) -> void {
	const bool was = d.primary.ui_focus;
	d.primary.ui_focus = focus;

	if (was || !focus || !d.primary.handle) {
		return;
	}

	const auto dims = window_handle_viewport(d.primary.handle);
	const double center_x = dims.x() / 2.0;
	const double center_y = dims.y() / 2.0;
	glfwSetCursorPos(to_glfw_handle(d.primary.handle), center_x, center_y);
	d.primary.input_events.push(input::mouse_moved{ center_x, dims.y() - center_y });
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