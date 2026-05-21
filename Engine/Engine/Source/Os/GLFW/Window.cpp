module gse.os;

import std;
import vulkan;
import gse.glfw;

import :keys;
import :input_events;

import gse.assert;
import gse.math;
import gse.core;
import gse.concurrency;
import gse.ecs;

namespace gse {
	auto refresh_monitor_settings(window::data& d) -> void;

	auto refresh_resolution_settings(window::data& d) -> void;

	auto refresh_display_mode_settings(window::data& d) -> void;

	auto refresh_present_mode_settings(window::data& d) -> void;

	auto apply_cursor_mode(const window::data& d) -> void;

	auto apply_display_mode(window::data& d, display_mode mode) -> void;

	auto move_window_to_monitor(const window::data& d, int monitor_index) -> void;

	auto to_input_key(int glfw_key) -> std::optional<key>;

	auto to_input_mouse_button(int glfw_button) -> std::optional<mouse_button>;

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
	const int target_mode = d.mouse_visible ? glfw::cursor_normal : glfw::cursor_disabled;
	const int current_mode = glfwGetInputMode(d.handle, glfw::cursor);
	if (current_mode == target_mode) {
		return;
	}

	if (current_mode == glfw::cursor_disabled && target_mode == glfw::cursor_normal) {
		const auto dims = window::viewport(d);
		glfwSetCursorPos(d.handle, dims.x() / 2.0, dims.y() / 2.0);
	}

	glfwSetInputMode(d.handle, glfw::cursor, target_mode);
}

auto gse::move_window_to_monitor(const window::data& d, const int monitor_index) -> void {
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
	glfwGetWindowSize(d.handle, &ww, &wh);

	const int new_x = mx + (mw - ww) / 2;
	const int new_y = my + (mh - wh) / 2;
	glfwSetWindowPos(d.handle, new_x, new_y);
}

auto gse::apply_display_mode(window::data& d, const display_mode mode) -> void {
	if (d.current_display_mode == mode) {
		return;
	}

	const bool was_windowed = d.current_display_mode == display_mode::windowed;
	const bool will_be_windowed = mode == display_mode::windowed;

	if (was_windowed && !will_be_windowed) {
		int x = 0;
		int y = 0;
		int w = 0;
		int h = 0;
		glfwGetWindowPos(d.handle, &x, &y);
		glfwGetWindowSize(d.handle, &w, &h);
		d.windowed_rect = rect_t<vec2i>::from_position_size({ x, y }, { w, h });
	}

	d.current_display_mode = mode;

	if (will_be_windowed) {
		const auto pos = d.windowed_rect.top_left();
		const auto size = d.windowed_rect.size();
		glfwSetWindowMonitor(d.handle, nullptr, pos.x(), pos.y(), size.x(), size.y(), 0);
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

		if (
			const int res_idx = d.resolution.value - 1; res_idx >= 0 && res_idx < static_cast<int>(resolutions.size())
		) {
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

	glfwSetWindowMonitor(d.handle, target_monitor, 0, 0, target_width, target_height, target_refresh);
}

auto gse::window::run(run_context& ctx, data& d) -> async::task<> {
	assert(glfwInit(), "Error initializing GLFW");
	assert(glfwVulkanSupported(), "Vulkan not supported");

	if (d.title.empty()) {
		d.title = "GSEngine";
	}

	glfwWindowHint(glfw::client_api, glfw::no_api);
	glfwWindowHint(glfw::resizable, glfw::true_);
	glfwWindowHint(glfw::focus_on_show, glfw::true_);
	glfwWindowHint(glfw::visible, glfw::false_);

	const auto initial_size = d.windowed_rect.size();
	d.handle = glfwCreateWindow(initial_size.x(), initial_size.y(), d.title.c_str(), nullptr, nullptr);
	assert(d.handle, "Failed to create GLFW window!");

	glfwSetWindowUserPointer(d.handle, &d);

	glfwSetKeyCallback(d.handle, [](GLFWwindow* w, const int key, int, const int action, int) {
		auto* self = static_cast<data*>(glfwGetWindowUserPointer(w));
		if (!self) {
			return;
		}
		const auto mapped = to_input_key(key);
		if (!mapped) {
			return;
		}
		if (action == glfw::press) {
			self->input_events.push(
				input::key_pressed{
					.key_code = *mapped
				}
			);
		}
		else if (action == glfw::release) {
			self->input_events.push(
				input::key_released{
					.key_code = *mapped
				}
			);
		}
	});

	glfwSetMouseButtonCallback(d.handle, [](GLFWwindow* w, const int button, const int action, int) {
		auto* self = static_cast<data*>(glfwGetWindowUserPointer(w));
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
	});

	glfwSetCursorPosCallback(d.handle, [](GLFWwindow* w, double xpos, double ypos) {
		auto* self = static_cast<data*>(glfwGetWindowUserPointer(w));
		if (!self) {
			return;
		}

		if (self->ui_focus) {
			const auto dims = window::viewport(*self);
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
	});

	glfwSetScrollCallback(d.handle, [](GLFWwindow* w, const double xoffset, const double yoffset) {
		if (auto* self = static_cast<data*>(glfwGetWindowUserPointer(w))) {
			self->input_events.push(input::mouse_scrolled{ xoffset, yoffset });
		}
	});

	glfwSetCharCallback(d.handle, [](GLFWwindow* w, const unsigned int codepoint) {
		if (auto* self = static_cast<data*>(glfwGetWindowUserPointer(w))) {
			self->input_events.push(input::text_entered{ codepoint });
		}
	});

	glfwSetWindowFocusCallback(d.handle, [](GLFWwindow* w, const int focused) {
		auto* self = static_cast<data*>(glfwGetWindowUserPointer(w));
		if (!self) {
			return;
		}
		self->focused = (focused == glfw::true_);
	});

	glfwSetFramebufferSizeCallback(d.handle, [](GLFWwindow* w, const int, const int) {
		if (auto* self = static_cast<data*>(glfwGetWindowUserPointer(w))) {
			self->framebuffer_resized = true;
		}
	});

	const int cursor_mode = d.mouse_visible ? glfw::cursor_normal : glfw::cursor_disabled;
	glfwSetInputMode(d.handle, glfw::cursor, cursor_mode);

	refresh_monitor_settings(d);
	d.last_monitor_index = d.monitor.value;
	refresh_resolution_settings(d);
	refresh_display_mode_settings(d);
	refresh_present_mode_settings(d);

	d.current_present_mode_index = d.present_mode.value;

	glfwFocusWindow(d.handle);

	while (true) {
		for (const auto& [focus] : ctx.read_channel<ui_focus_request>()) {
			set_ui_focus(d, focus);
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

		co_await ctx.next_tick();
	}
}

auto gse::window::shutdown(shutdown_context&, data& d) -> void {
	if (d.handle) {
		glfwDestroyWindow(d.handle);
		d.handle = nullptr;
	}
}

auto gse::window::poll_events() -> void {
	glfwPollEvents();
}

auto gse::window::is_open(const data& d) -> bool {
	return !glfwWindowShouldClose(d.handle);
}

auto gse::window::minimized(const data& d) -> bool {
	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(d.handle, &width, &height);
	return width == 0 || height == 0;
}

auto gse::window::viewport(const data& d) -> vec2i {
	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(d.handle, &width, &height);
	return { width, height };
}

auto gse::window::frame_buffer_resized(data& d) -> bool {
	if (d.framebuffer_resized) {
		d.framebuffer_resized = false;
		return true;
	}
	return false;
}

auto gse::window::create_vulkan_surface(const data& d, const vk::Instance instance) -> vk::SurfaceKHR {
	const auto surface = glfw::create_window_surface(instance, d.handle);
	assert(surface, "Failed to create window surface for Vulkan!");
	return surface;
}

auto gse::window::raw_handle(const data& d) -> GLFWwindow* {
	return d.handle;
}

auto gse::window::show(const data& d) -> void {
	if (d.handle) {
		glfwShowWindow(d.handle);
	}
}

auto gse::window::set_ui_focus(data& d, const bool focus) -> void {
	const bool was = d.ui_focus;
	d.ui_focus = focus;

	if (was || !focus || !d.handle) {
		return;
	}

	const auto dims = viewport(d);
	const double center_x = dims.x() / 2.0;
	const double center_y = dims.y() / 2.0;
	glfwSetCursorPos(d.handle, center_x, center_y);
	d.input_events.push(input::mouse_moved{ center_x, dims.y() - center_y });
}

auto gse::window::ui_focus(const data& d) -> bool {
	return d.ui_focus;
}

auto gse::window::vulkan_instance_extensions() -> std::span<const char* const> {
	std::uint32_t count = 0;
	const char** extensions = glfwGetRequiredInstanceExtensions(&count);
	return { extensions, count };
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
