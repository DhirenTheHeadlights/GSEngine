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
		explicit window(const std::string& title);
		~window() override;

		auto update(bool ui_focus) -> void;
		static auto poll_events() -> void;
		auto shutdown() -> void ;

		auto is_open() const -> bool;
		auto minimized() const -> bool;
		auto mouse_visible() const -> bool;
		auto viewport() const -> unitless::vec2i;
		auto frame_buffer_resized() -> bool;
		auto create_vulkan_surface(VkInstance instance) const -> VkSurfaceKHR;

		auto set_mouse_visible(bool visible) -> void;

		auto raw_handle() const -> GLFWwindow* { return m_window; }
	private:
		GLFWwindow* m_window = nullptr;
		bool m_fullscreen = true;
		bool m_current_fullscreen = true;
		bool m_mouse_visible = false;
		bool m_focused = true;
		bool m_frame_buffer_resized = false;
		auto set_fullscreen(bool fullscreen) -> void;
		bool m_ui_focus = false;
	};
}

gse::window::window(const std::string& title) {
	assert(glfwInit(), std::source_location::current(), "Error initializing GLFW");
	assert(glfwVulkanSupported(), std::source_location::current(), "Vulkan not supported");

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_window = glfwCreateWindow(1920, 1080, title.c_str(), nullptr, nullptr);
	assert(m_window, std::source_location::current(), "Failed to create GLFW window!");

	glfwSetWindowUserPointer(m_window, this);

	glfwSetKeyCallback(
		m_window,
		[](GLFWwindow*, const int key, int, const int action, int) {
			input::key_callback(key, action);
		}
	);

	glfwSetMouseButtonCallback(
		m_window,
		[](GLFWwindow* window, const int button, const int action, int) {
			double x, y;
			glfwGetCursorPos(window, &x, &y);
			input::mouse_button_callback(button, action, x, y);
		}
	);

	glfwSetCursorPosCallback(
		m_window,
		[](GLFWwindow* window, double xpos, double ypos) {
			const auto* self = static_cast<gse::window*>(glfwGetWindowUserPointer(window));

			if (!self) {
				return;
			}

			if (self->m_ui_focus) {
				const auto dims = self->viewport();

				xpos = std::clamp(xpos, 0.0, static_cast<double>(dims.x()));
				ypos = std::clamp(ypos, 0.0, static_cast<double>(dims.y()));

				const double inverted_ypos = static_cast<double>(dims.y()) - ypos;
				input::mouse_pos_callback(xpos, inverted_ypos);
			}
			else {
				input::mouse_pos_callback(xpos, ypos);
			}
		}
	);

	glfwSetScrollCallback(
		m_window,
		[](GLFWwindow*, const double xoffset, const double yoffset) {
			input::mouse_scroll_callback(xoffset, yoffset);
		}
	);

	glfwSetCharCallback(
		m_window,
		[](GLFWwindow*, const unsigned int codepoint) {
			input::text_callback(codepoint);
		}
	);

	glfwSetWindowFocusCallback(
		m_window,
		[](GLFWwindow* window, const int focused) {
			auto* self = static_cast<gse::window*>(glfwGetWindowUserPointer(window));
			self->m_focused = (focused == GLFW_TRUE);
			if (self->m_focused) {
				const int cursor_mode = self->mouse_visible() ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;
				glfwSetInputMode(window, GLFW_CURSOR, cursor_mode);
			}
			else {
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				input::clear_events();
			}
		}
	);

	glfwSetFramebufferSizeCallback(
		m_window,
		[](GLFWwindow* window, const int width, const int height) {
			auto* self = static_cast<gse::window*>(glfwGetWindowUserPointer(window));
			self->m_frame_buffer_resized = true;
		}
	);
}

gse::window::~window() {
	assert(
		!m_window,
		std::source_location::current(),
		"Shutdown not called before destructing window!"
	);
}

auto gse::window::update(const bool ui_focus) -> void {
	m_ui_focus = ui_focus;

	if (m_focused && m_current_fullscreen != m_fullscreen) {
		static int last_pos_x = 0, last_pos_y = 0;
		static int last_w = 0, last_h = 0;

		if (m_fullscreen) {
			glfwGetWindowPos(m_window, &last_pos_x, &last_pos_y);
			glfwGetWindowSize(m_window, &last_w, &last_h);

			int monitor_count = 0;
			GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);

			assert(
				monitor_count > 0,
				std::source_location::current(),
				"Failed to get monitors! At least one monitor is required for fullscreen mode."
			);

			int wx = 0, wy = 0;
			glfwGetWindowPos(m_window, &wx, &wy);
			int ww = 0, wh = 0;
			glfwGetWindowSize(m_window, &ww, &wh);

			GLFWmonitor* best_monitor = glfwGetPrimaryMonitor();
			int best_overlap = 0;

			for (int i = 0; i < monitor_count; ++i) {
				GLFWmonitor* monitor = monitors[i];
				const GLFWvidmode* mode = glfwGetVideoMode(monitor);
				int mx = 0, my = 0;
				glfwGetMonitorPos(monitor, &mx, &my);
				const int mw = mode->width;
				const int mh = mode->height;

				const int overlap =
					std::max(0, std::min(wx + ww, mx + mw) - std::max(wx, mx)) *
					std::max(0, std::min(wy + wh, my + mh) - std::max(wy, my));

				if (overlap > best_overlap) {
					best_overlap = overlap;
					best_monitor = monitor;
				}
			}

			const GLFWvidmode* mode = glfwGetVideoMode(best_monitor);
			glfwSetWindowMonitor(m_window, best_monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
		}
		else {
			glfwSetWindowMonitor(m_window, nullptr, last_pos_x, last_pos_y, last_w, last_h, 0);
		}
		m_current_fullscreen = m_fullscreen;
	}

	const int cursor_mode = m_mouse_visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;
	glfwSetInputMode(m_window, GLFW_CURSOR, cursor_mode);
}

auto gse::window::poll_events() -> void {
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

auto gse::window::set_fullscreen(const bool fullscreen) -> void {
	m_fullscreen = fullscreen;
}

auto gse::window::set_mouse_visible(const bool visible) -> void {
	m_mouse_visible = visible;
}