module;

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

export module gse.platform:window;

import std;

import :input;

import gse.assert;
import gse.physics.math;
import gse.utility;

export namespace gse {
	class window final : non_copyable {
	public:
		explicit window(
			const std::string& title,
			input::system& input_system,
			save::system& save_system
		);
		
		~window() override;

		auto update(
			bool ui_focus
		) -> void;

		static auto poll_events(
		) -> void;

		auto shutdown(
		) -> void;

		auto is_open(
		) const -> bool;

		auto minimized(
		) const -> bool;

		auto mouse_visible(
		) const -> bool;

		auto viewport(
		) const -> unitless::vec2i;

		auto frame_buffer_resized(
		) -> bool;

		auto create_vulkan_surface(
			VkInstance instance
		) const -> VkSurfaceKHR;

		auto set_mouse_visible(
			bool visible
		) -> void;

		auto raw_handle(
		) const -> GLFWwindow* { return m_window; }
	private:
		GLFWwindow* m_window = nullptr;
		input::system& m_input;
		
		bool m_fullscreen = true;
		bool m_current_fullscreen = false;
		bool m_mouse_visible = false;
		bool m_focused = true;
		bool m_frame_buffer_resized = false;
		bool m_ui_focus = false;

		struct pending_state {
			std::optional<bool> fullscreen_request;
			std::optional<int> cursor_mode_request;
			int windowed_pos_x = 100, windowed_pos_y = 100;
			int windowed_w = 1920, windowed_h = 1080;
		} m_pending;
		std::mutex m_pending_mutex;

		auto request_fullscreen(
			bool fullscreen
		) -> void;

		auto process_pending_operations(
		) -> void;

		static inline std::vector<window*> s_windows;
		static inline std::mutex s_windows_mutex;

		auto set_fullscreen(
			bool fullscreen
		) -> void;
	};
}

gse::window::window(const std::string& title, input::system& input_system, save::system& save_system) : m_input(input_system) {
	assert(glfwInit(), std::source_location::current(), "Error initializing GLFW");
	assert(glfwVulkanSupported(), std::source_location::current(), "Vulkan not supported");

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);

	m_window = glfwCreateWindow(1920, 1080, title.c_str(), nullptr, nullptr);
	assert(m_window, std::source_location::current(), "Failed to create GLFW window!");

	glfwSetWindowUserPointer(m_window, this);

	glfwSetKeyCallback(
		m_window,
		[](GLFWwindow* w, const int key, int, const int action, int) {
			if (const auto* self = static_cast<window*>(glfwGetWindowUserPointer(w))) {
				self->m_input.key_callback(key, action);
			}
		}
	);

	glfwSetMouseButtonCallback(
		m_window,
		[](GLFWwindow* w, const int button, const int action, int) {
			if (const auto* self = static_cast<window*>(glfwGetWindowUserPointer(w))) {
				double x, y;
				glfwGetCursorPos(w, &x, &y);
				self->m_input.mouse_button_callback(button, action, x, y);
			}
		}
	);

	glfwSetCursorPosCallback(
		m_window,
		[](GLFWwindow* w, double xpos, double ypos) {
			auto* self = static_cast<window*>(glfwGetWindowUserPointer(w));
			if (!self) return;

			if (self->m_ui_focus) {
				const auto dims = self->viewport();
				const double clamped_x = std::clamp(xpos, 0.0, static_cast<double>(dims.x()));
				const double clamped_y = std::clamp(ypos, 0.0, static_cast<double>(dims.y()));

				if (clamped_x != xpos || clamped_y != ypos) {
					glfwSetCursorPos(w, clamped_x, clamped_y);
				}

				const double inverted_y = static_cast<double>(dims.y()) - clamped_y;
				self->m_input.mouse_pos_callback(clamped_x, inverted_y);
			}
			else {
				self->m_input.mouse_pos_callback(xpos, ypos);
			}
		}
	);

	glfwSetScrollCallback(
		m_window,
		[](GLFWwindow* w, const double xoffset, const double yoffset) {
			if (const auto* self = static_cast<window*>(glfwGetWindowUserPointer(w))) {
				self->m_input.mouse_scroll_callback(xoffset, yoffset);
			}
		}
	);

	glfwSetCharCallback(
		m_window,
		[](GLFWwindow* w, const unsigned int codepoint) {
			if (const auto* self = static_cast<window*>(glfwGetWindowUserPointer(w))) {
				self->m_input.text_callback(codepoint);
			}
		}
	);

	glfwSetWindowFocusCallback(
		m_window,
		[](GLFWwindow* w, const int focused) {
			auto* self = static_cast<window*>(glfwGetWindowUserPointer(w));
			if (!self) return;

			self->m_focused = (focused == GLFW_TRUE);
			if (self->m_focused) {
				const int cursor_mode = self->mouse_visible() ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;
				glfwSetInputMode(w, GLFW_CURSOR, cursor_mode);
			}
			else {
				glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				self->m_input.clear_events();
			}
		}
	);

	glfwSetFramebufferSizeCallback(
		m_window,
		[](GLFWwindow* w, const int, const int) {
			if (auto* self = static_cast<window*>(glfwGetWindowUserPointer(w))) {
				self->m_frame_buffer_resized = true;
			}
		}
	);

	const int cursor_mode = m_mouse_visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;
	glfwSetInputMode(m_window, GLFW_CURSOR, cursor_mode);

	save_system.bind("Window", "Fullscreen", m_fullscreen)
		.description("Run in fullscreen mode")
		.default_value(true)
		.commit();

	save_system.bind("Window", "Mouse Visible", m_mouse_visible)
		.description("Show mouse cursor")
		.default_value(false)
		.commit();

	glfwFocusWindow(m_window);

	{
		std::lock_guard lock(s_windows_mutex);
		s_windows.push_back(this);
	}
}

gse::window::~window() {
	{
		std::lock_guard lock(s_windows_mutex);
		std::erase(s_windows, this);
	}

	assert(
		!m_window,
		std::source_location::current(),
		"Shutdown not called before destructing window!"
	);
}

auto gse::window::update(const bool ui_focus) -> void {
	const bool was_ui_focus = m_ui_focus;
	m_ui_focus = ui_focus;

	if (!was_ui_focus && ui_focus) {
		const auto dims = viewport();
		glfwSetCursorPos(m_window, dims.x() / 2.0, dims.y() / 2.0);
	}

	if (m_current_fullscreen != m_fullscreen) {
		request_fullscreen(m_fullscreen);
	}

	{
		const int cursor_mode = m_mouse_visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;
		std::lock_guard lock(m_pending_mutex);
		m_pending.cursor_mode_request = cursor_mode;
	}
}

auto gse::window::poll_events() -> void {\
	{
		std::lock_guard lock(s_windows_mutex);
		for (auto* w : s_windows) {
			w->process_pending_operations();
		}
	}

	glfwPollEvents();
}

auto gse::window::shutdown() -> void {
	assert(m_window, std::source_location::current(), "Window already shutdown!");
	glfwDestroyWindow(m_window);
	m_window = nullptr;
}

auto gse::window::is_open() const -> bool {
	return !glfwWindowShouldClose(m_window);
}

auto gse::window::minimized() const -> bool {
	int width = 0, height = 0;
	glfwGetFramebufferSize(m_window, &width, &height);
	return width == 0 || height == 0;
}

auto gse::window::mouse_visible() const -> bool {
	return m_mouse_visible;
}

auto gse::window::viewport() const -> unitless::vec2i {
	int width = 0, height = 0;
	glfwGetFramebufferSize(m_window, &width, &height);
	return { width, height };
}

auto gse::window::frame_buffer_resized() -> bool {
	if (m_frame_buffer_resized) {
		m_frame_buffer_resized = false;
		return true;
	}
	return false;
}

auto gse::window::create_vulkan_surface(const VkInstance instance) const -> VkSurfaceKHR {
	VkSurfaceKHR surface = nullptr;
	const VkResult result = glfwCreateWindowSurface(instance, m_window, nullptr, &surface);
	assert(result == VK_SUCCESS, std::source_location::current(), "Failed to create window surface for Vulkan!");
	return surface;
}

auto gse::window::request_fullscreen(const bool fullscreen) -> void {
	std::lock_guard lock(m_pending_mutex);
	
	if (fullscreen && !m_current_fullscreen) {
		glfwGetWindowPos(m_window, &m_pending.windowed_pos_x, &m_pending.windowed_pos_y);
		glfwGetWindowSize(m_window, &m_pending.windowed_w, &m_pending.windowed_h);
	}

	m_pending.fullscreen_request = fullscreen;
}

auto gse::window::process_pending_operations() -> void {
	std::optional<bool> fullscreen_req;
	std::optional<int> cursor_mode_req;
	int pos_x, pos_y, w, h;

	{
		std::lock_guard lock(m_pending_mutex);
		fullscreen_req = std::exchange(m_pending.fullscreen_request, std::nullopt);
		cursor_mode_req = std::exchange(m_pending.cursor_mode_request, std::nullopt);
		pos_x = m_pending.windowed_pos_x;
		pos_y = m_pending.windowed_pos_y;
		w = m_pending.windowed_w;
		h = m_pending.windowed_h;
	}

	if (cursor_mode_req.has_value()) {
		const int current_mode = glfwGetInputMode(m_window, GLFW_CURSOR);
		const int new_mode = *cursor_mode_req;

		if (current_mode == GLFW_CURSOR_DISABLED && new_mode == GLFW_CURSOR_NORMAL) {
			const auto dims = viewport();
			glfwSetCursorPos(m_window, dims.x() / 2.0, dims.y() / 2.0);
		}

		glfwSetInputMode(m_window, GLFW_CURSOR, new_mode);
	}

	if (!fullscreen_req.has_value() || !m_focused) {
		return;
	}

	const bool fullscreen = *fullscreen_req;
	if (m_current_fullscreen == fullscreen) {
		return;
	}

	m_current_fullscreen = fullscreen;

	if (fullscreen) {
		int monitor_count = 0;
		GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);

		assert(
			monitor_count > 0,
			std::source_location::current(),
			"Failed to get monitors!"
		);

		int wx = 0, wy = 0, ww = 0, wh = 0;
		glfwGetWindowPos(m_window, &wx, &wy);
		glfwGetWindowSize(m_window, &ww, &wh);

		GLFWmonitor* best_monitor = glfwGetPrimaryMonitor();
		int best_overlap = 0;

		for (int i = 0; i < monitor_count; ++i) {
			GLFWmonitor* monitor = monitors[i];
			const GLFWvidmode* mode = glfwGetVideoMode(monitor);
			int mx = 0, my = 0;
			glfwGetMonitorPos(monitor, &mx, &my);

			const int overlap =
				std::max(0, std::min(wx + ww, mx + mode->width) - std::max(wx, mx)) *
				std::max(0, std::min(wy + wh, my + mode->height) - std::max(wy, my));

			if (overlap > best_overlap) {
				best_overlap = overlap;
				best_monitor = monitor;
			}
		}

		const GLFWvidmode* mode = glfwGetVideoMode(best_monitor);
		glfwSetWindowMonitor(m_window, best_monitor, 0, 0,
			mode->width, mode->height, mode->refreshRate);
	}
	else {
		glfwSetWindowMonitor(m_window, nullptr, pos_x, pos_y, w, h, 0);
	}
}

auto gse::window::set_fullscreen(const bool fullscreen) -> void {
	m_fullscreen = fullscreen;
}

auto gse::window::set_mouse_visible(const bool visible) -> void {
	m_mouse_visible = visible;
}