module;

#include "glad/glad.h"
#include "GLFW/glfw3.h"

module gse.platform.glfw.window;


import std;
import glm;

import gse.platform.glfw.input;
import gse.platform.perma_assert;

#undef max
#undef min

namespace {
	GLFWwindow* g_window = nullptr;

	std::optional<GLuint> g_fbo;
	glm::ivec2 g_fbo_size = { 1, 1 };

	bool g_current_full_screen = false;
	bool g_full_screen = false;
	bool g_window_focused = true;
	bool g_mouse_visible = true;
	int g_mouse_moved = 0;

	std::vector<std::shared_ptr<gse::window::rendering_interface>> g_rendering_interfaces;

	/// Callbacks

	void key_callback(GLFWwindow* window, const int key, int scancode, const int action, int mods) {
		if (gse::input::get_keyboard().keys.contains(key)) {
			if (action == GLFW_PRESS) {
				gse::input::internal::process_event_button(gse::input::get_keyboard().keys[key], true);
			}
			else if (action == GLFW_RELEASE) {
				gse::input::internal::process_event_button(gse::input::get_keyboard().keys[key], false);
			}
		}
	}

	void mouse_callback(GLFWwindow* window, const int button, const int action, int mods) {
		if (gse::input::get_mouse().buttons.contains(button)) {
			if (action == GLFW_PRESS) {
				gse::input::internal::process_event_button(gse::input::get_mouse().buttons[button], true);
			}
			else if (action == GLFW_RELEASE) {
				gse::input::internal::process_event_button(gse::input::get_mouse().buttons[button], false);
			}
		}
	}

	void window_focus_callback(GLFWwindow* window, const int focused) {
		if (focused) {
			g_window_focused = true;
		}
		else {
			g_window_focused = false;
			gse::input::internal::reset_inputs_to_zero(); // To reset buttons
		}
	}

	void window_size_callback(GLFWwindow* window, int x, int y) {
		gse::input::internal::reset_inputs_to_zero();
	}

	void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
		g_mouse_moved = 1;
	}

	void character_callback(GLFWwindow* window, const unsigned int codepoint) {
		if (codepoint < 127) {
			gse::input::internal::add_to_typed_input(codepoint);
		}
	}
}

void gse::window::add_rendering_interface(const std::shared_ptr<rendering_interface>& rendering_interface) {
	g_rendering_interfaces.push_back(rendering_interface);
}

void gse::window::remove_rendering_interface(const std::shared_ptr<rendering_interface>& rendering_interface) {
	if (const auto it = std::ranges::find(g_rendering_interfaces, rendering_interface); it != g_rendering_interfaces.end()) {
		g_rendering_interfaces.erase(it);
	}
}

void gse::window::initialize() {
	assert_comment(glfwInit(), "Error initializing GLFW");
	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
	glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	const int w = mode->width;
	const int h = mode->height;

	// Create window in full screen mode
	g_window = glfwCreateWindow(w, h, "SavantShooter", nullptr, nullptr);
	glfwSetWindowPos(g_window, 0.f, 0.f);
	glfwMakeContextCurrent(g_window);
	glfwSwapInterval(1);

	glfwSetKeyCallback(g_window, key_callback);
	glfwSetMouseButtonCallback(g_window, mouse_callback);
	glfwSetWindowFocusCallback(g_window, window_focus_callback);
	glfwSetWindowSizeCallback(g_window, window_size_callback);
	glfwSetCursorPosCallback(g_window, cursor_position_callback);
	glfwSetCharCallback(g_window, character_callback);

	assert_comment(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)), "Error Initializing GLAD");
}

void gse::window::begin_frame() {
	glViewport(0, 0, get_frame_buffer_size().x, get_frame_buffer_size().y);
	glClear(GL_COLOR_BUFFER_BIT);

	for (const auto& rendering_interface : g_rendering_interfaces) {
		rendering_interface->on_pre_render();
	}
}

void gse::window::update() {
	int w = 0, h = 0;
	glfwGetWindowSize(g_window, &w, &h);
	if (g_window_focused && g_current_full_screen != g_full_screen) {
		static int last_w = w;
		static int last_h = h;
		static int last_pos_x = 0;
		static int last_pos_y = 0;

		if (g_full_screen) {
			last_w = w;
			last_h = h;

			glfwGetWindowPos(g_window, &last_pos_x, &last_pos_y);

			const auto monitor = get_current_monitor();

			const GLFWvidmode* mode = glfwGetVideoMode(monitor);

			// Switch to full screen
			glfwSetWindowMonitor(g_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

			g_current_full_screen = true;

		}
		else {
			glfwSetWindowMonitor(g_window, nullptr, last_pos_x, last_pos_y, last_w, last_h, 0);

			g_current_full_screen = false;
		}

		g_mouse_moved = 0;
	}

	if (g_mouse_visible) {
		glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	else {
		glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
}

void gse::window::end_frame() {
	for (const auto& rendering_interface : g_rendering_interfaces) {
		rendering_interface->on_post_render();
	}

	glfwSwapBuffers(g_window);
	glfwPollEvents();
}

void gse::window::shutdown() {
	glfwTerminate();
}

/// Getters and Setters

GLFWwindow* gse::window::get_window() {
	return g_window;
}

bool gse::window::is_window_closed() {
	return glfwWindowShouldClose(g_window);
}

bool gse::window::is_full_screen() {
	return g_full_screen;
}

bool gse::window::is_focused() {
	return g_window_focused;
}

bool gse::window::is_minimized() {
	int width, height;
	glfwGetWindowSize(g_window, &width, &height);
	return width == 0 || height == 0;
}

bool gse::window::is_mouse_visible() {
	return g_mouse_visible;
}

int gse::window::has_mouse_moved() {
	return g_mouse_moved;
}

GLFWmonitor* gse::window::get_current_monitor() {
	int number_of_monitors;
	int wx, wy, ww, wh;
	int mx, my;

	int best_overlap = 0;
	GLFWmonitor* best_monitor = nullptr;

	glfwGetWindowPos(g_window, &wx, &wy);
	glfwGetWindowSize(g_window, &ww, &wh);
	GLFWmonitor** monitors = glfwGetMonitors(&number_of_monitors);

	for (int i = 0; i < number_of_monitors; i++) {
		const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
		glfwGetMonitorPos(monitors[i], &mx, &my);
		const int mw = mode->width;
		const int mh = mode->height;

		const int overlap = std::max(0, std::min(wx + ww, mx + mw) - std::max(wx, mx)) *
			std::max(0, std::min(wy + wh, my + mh) - std::max(wy, my));

		if (best_overlap < overlap) {
			best_overlap = overlap;
			best_monitor = monitors[i];
		}
	}

	return best_monitor;
}

std::optional<GLuint> gse::window::get_fbo() {
	return g_fbo;
}

glm::ivec2 gse::window::get_frame_buffer_size() {
	if (g_fbo.has_value()) {
		if (g_fbo_size == glm::ivec2(0, 0)) {
			return { 1, 1 };
		}
		return g_fbo_size;
	}

	int x = 0; int y = 0;
	glfwGetFramebufferSize(g_window, &x, &y);
	if (x == 0 || y == 0) {
		return { 1, 1 };
	}
	return { x, y };
}

glm::ivec2 gse::window::get_rel_mouse_position() {
	double x = 0, y = 0;
	glfwGetCursorPos(g_window, &x, &y);
	return { x, y };
}

glm::ivec2 gse::window::get_window_size() {
	int x = 0; int y = 0;
	glfwGetWindowSize(g_window, &x, &y);
	return { x, y };
}

glm::ivec2 gse::window::get_viewport_size() {
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	return { viewport[2], viewport[3] };
}

void gse::window::set_fbo(const GLuint fbo_in, const glm::ivec2& size) {
	g_fbo = fbo_in;
	g_fbo_size = size;
}

void gse::window::set_mouse_pos_relative_to_window(const glm::ivec2& position) {
	glfwSetCursorPos(g_window, position.x, position.y);
}

void gse::window::set_full_screen(const bool fs) {
	g_full_screen = fs;
}

void gse::window::set_window_focused(const bool focused) {
	g_window_focused = focused;
}

void gse::window::set_mouse_visible(const bool show) {
	g_mouse_visible = show;
}