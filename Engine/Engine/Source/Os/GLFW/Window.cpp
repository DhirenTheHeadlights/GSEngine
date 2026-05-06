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
	auto refresh_monitor_settings(
		window::settings& cfg
	) -> void;

	auto refresh_resolution_settings(
		window::settings& cfg
	) -> void;

	auto apply_cursor_mode(
		const window::state& s,
		const window::settings& cfg
	) -> void;

	auto apply_fullscreen(
		window::state& s,
		const window::settings& cfg,
		bool fullscreen
	) -> void;

	auto move_window_to_monitor(
		const window::state& s,
		int monitor_index
	) -> void;

	auto to_input_key(
		int glfw_key
	) -> std::optional<key>;

	auto to_input_mouse_button(
		int glfw_button
	) -> std::optional<mouse_button>;
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

auto gse::refresh_monitor_settings(window::settings& cfg) -> void {
	const auto monitors = window::enumerate_monitors();

	cfg.monitor.options.clear();
	for (const auto& monitor : monitors) {
		cfg.monitor.options.push_back(std::format("{}: {}x{}", monitor.name, monitor.width, monitor.height));
	}

	if (cfg.monitor.value < 0 || cfg.monitor.value >= static_cast<int>(monitors.size())) {
		cfg.monitor.value = 0;
	}
}

auto gse::refresh_resolution_settings(window::settings& cfg) -> void {
	const auto resolutions = window::enumerate_resolutions(cfg.monitor.value);

	cfg.resolution.options.clear();
	cfg.resolution.options.emplace_back("Native");
	for (const auto& resolution : resolutions) {
		cfg.resolution.options.push_back(std::format("{}", resolution));
	}

	if (cfg.resolution.value < 0 || cfg.resolution.value >= static_cast<int>(cfg.resolution.options.size())) {
		cfg.resolution.value = 0;
	}
}

auto gse::apply_cursor_mode(const window::state& s, const window::settings& cfg) -> void {
	const int target_mode = cfg.mouse_visible ? glfw::cursor_normal : glfw::cursor_disabled;
	const int current_mode = glfwGetInputMode(s.handle, glfw::cursor);
	if (current_mode == target_mode) {
		return;
	}

	if (current_mode == glfw::cursor_disabled && target_mode == glfw::cursor_normal) {
		const auto dims = window::viewport(s);
		glfwSetCursorPos(s.handle, dims.x() / 2.0, dims.y() / 2.0);
	}

	glfwSetInputMode(s.handle, glfw::cursor, target_mode);
}

auto gse::move_window_to_monitor(const window::state& s, const int monitor_index) -> void {
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
	glfwGetWindowSize(s.handle, &ww, &wh);

	const int new_x = mx + (mw - ww) / 2;
	const int new_y = my + (mh - wh) / 2;
	glfwSetWindowPos(s.handle, new_x, new_y);
}

auto gse::apply_fullscreen(window::state& s, const window::settings& cfg, const bool fullscreen) -> void {
	if (s.current_fullscreen == fullscreen) {
		return;
	}

	if (fullscreen) {
		int x = 0;
		int y = 0;
		int w = 0;
		int h = 0;
		glfwGetWindowPos(s.handle, &x, &y);
		glfwGetWindowSize(s.handle, &w, &h);
		s.windowed_rect = rect_t<vec2i>::from_position_size({ x, y }, { w, h });
	}

	s.current_fullscreen = fullscreen;

	if (!fullscreen) {
		const auto pos = s.windowed_rect.top_left();
		const auto size = s.windowed_rect.size();
		glfwSetWindowMonitor(s.handle, nullptr, pos.x(), pos.y(), size.x(), size.y(), 0);
		return;
	}

	int monitor_count = 0;
	GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
	assert(monitor_count > 0, "Failed to get monitors!");

	const int selected_monitor = std::clamp(cfg.monitor.value, 0, monitor_count - 1);
	GLFWmonitor* target_monitor = monitors[selected_monitor];

	int target_width = 0;
	int target_height = 0;
	int target_refresh = 0;

	if (cfg.resolution.value == 0) {
		const GLFWvidmode* native_mode = glfwGetVideoMode(target_monitor);
		target_width = native_mode->width;
		target_height = native_mode->height;
		target_refresh = native_mode->refreshRate;
	}
	else {
		const auto resolutions = window::enumerate_resolutions(selected_monitor);

		if (const int res_idx = cfg.resolution.value - 1; res_idx >= 0 && res_idx < static_cast<int>(resolutions.size())) {
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

	glfwSetWindowMonitor(s.handle, target_monitor, 0, 0, target_width, target_height, target_refresh);
}

auto gse::window::initialize(const init_context&, settings& cfg, state& s) -> void {
	assert(glfwInit(), "Error initializing GLFW");
	assert(glfwVulkanSupported(), "Vulkan not supported");

	if (s.title.empty()) {
		s.title = "GSEngine";
	}

	glfwWindowHint(glfw::client_api, glfw::no_api);
	glfwWindowHint(glfw::resizable, glfw::true_);
	glfwWindowHint(glfw::focus_on_show, glfw::true_);

	const auto initial_size = s.windowed_rect.size();
	s.handle = glfwCreateWindow(initial_size.x(), initial_size.y(), s.title.c_str(), nullptr, nullptr);
	assert(s.handle, "Failed to create GLFW window!");

	glfwSetWindowUserPointer(s.handle, &s);

	glfwSetKeyCallback(s.handle, [](GLFWwindow* w, const int key, int, const int action, int) {
		auto* self = static_cast<state*>(glfwGetWindowUserPointer(w));
		if (!self) {
			return;
		}
		const auto mapped = to_input_key(key);
		if (!mapped) {
			return;
		}
		if (action == glfw::press) {
			self->input_events.push(input::key_pressed{ .key_code = *mapped });
		}
		else if (action == glfw::release) {
			self->input_events.push(input::key_released{ .key_code = *mapped });
		}
	});

	glfwSetMouseButtonCallback(s.handle, [](GLFWwindow* w, const int button, const int action, int) {
		auto* self = static_cast<state*>(glfwGetWindowUserPointer(w));
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

	glfwSetCursorPosCallback(s.handle, [](GLFWwindow* w, double xpos, double ypos) {
		auto* self = static_cast<state*>(glfwGetWindowUserPointer(w));
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

	glfwSetScrollCallback(s.handle, [](GLFWwindow* w, const double xoffset, const double yoffset) {
		if (auto* self = static_cast<state*>(glfwGetWindowUserPointer(w))) {
			self->input_events.push(input::mouse_scrolled{ xoffset, yoffset });
		}
	});

	glfwSetCharCallback(s.handle, [](GLFWwindow* w, const unsigned int codepoint) {
		if (auto* self = static_cast<state*>(glfwGetWindowUserPointer(w))) {
			self->input_events.push(input::text_entered{ codepoint });
		}
	});

	glfwSetWindowFocusCallback(s.handle, [](GLFWwindow* w, const int focused) {
		auto* self = static_cast<state*>(glfwGetWindowUserPointer(w));
		if (!self) {
			return;
		}
		self->focused = (focused == glfw::true_);
	});

	glfwSetFramebufferSizeCallback(s.handle, [](GLFWwindow* w, const int, const int) {
		if (auto* self = static_cast<state*>(glfwGetWindowUserPointer(w))) {
			self->framebuffer_resized = true;
		}
	});

	const int cursor_mode = cfg.mouse_visible ? glfw::cursor_normal : glfw::cursor_disabled;
	glfwSetInputMode(s.handle, glfw::cursor, cursor_mode);

	refresh_monitor_settings(cfg);
	s.last_monitor_index = cfg.monitor.value;
	refresh_resolution_settings(cfg);

	glfwFocusWindow(s.handle);
}

auto gse::window::update(update_context& ctx, const settings& cfg, state& s) -> async::task<> {
	for (const auto& [focus] : ctx.read_channel<ui_focus_request>()) {
		set_ui_focus(s, focus);
	}

	if (cfg.monitor.value != s.last_monitor_index) {
		const int old_monitor = s.last_monitor_index;
		s.last_monitor_index = cfg.monitor.value;

		if (!s.current_fullscreen && old_monitor != cfg.monitor.value) {
			move_window_to_monitor(s, cfg.monitor.value);
		}
	}

	if (s.focused) {
		apply_cursor_mode(s, cfg);

		if (s.current_fullscreen != cfg.fullscreen) {
			apply_fullscreen(s, cfg, cfg.fullscreen);
		}
	}

	co_return;
}

auto gse::window::shutdown(shutdown_context&, state& s) -> void {
	if (s.handle) {
		glfwDestroyWindow(s.handle);
		s.handle = nullptr;
	}
}

auto gse::window::poll_events() -> void {
	glfwPollEvents();
}

auto gse::window::is_open(const state& s) -> bool {
	return !glfwWindowShouldClose(s.handle);
}

auto gse::window::minimized(const state& s) -> bool {
	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(s.handle, &width, &height);
	return width == 0 || height == 0;
}

auto gse::window::viewport(const state& s) -> vec2i {
	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(s.handle, &width, &height);
	return { width, height };
}

auto gse::window::frame_buffer_resized(state& s) -> bool {
	if (s.framebuffer_resized) {
		s.framebuffer_resized = false;
		return true;
	}
	return false;
}

auto gse::window::create_vulkan_surface(const state& s, const vk::Instance instance) -> vk::SurfaceKHR {
	const auto surface = glfw::create_window_surface(instance, s.handle);
	assert(surface, "Failed to create window surface for Vulkan!");
	return surface;
}

auto gse::window::raw_handle(const state& s) -> GLFWwindow* {
	return s.handle;
}

auto gse::window::set_ui_focus(state& s, const bool focus) -> void {
	const bool was = s.ui_focus;
	s.ui_focus = focus;

	if (!was && focus && s.handle) {
		const auto dims = viewport(s);
		glfwSetCursorPos(s.handle, dims.x() / 2.0, dims.y() / 2.0);
	}
}

auto gse::window::ui_focus(const state& s) -> bool {
	return s.ui_focus;
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
