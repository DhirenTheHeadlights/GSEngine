module;

#include <glad/glad.h>
#include <GLFW/glfw3.h>

export module gse.platform.glfw.window;

import std;

import gse.platform.glfw.input;
import gse.platform.perma_assert;
import gse.physics.math;

#undef max
#undef min

export namespace gse::window {
	struct rendering_interface {
		virtual ~rendering_interface() = default;
		virtual auto on_pre_render() -> void;
		virtual auto on_post_render() -> void = 0;
	};

	auto add_rendering_interface(const std::shared_ptr<rendering_interface>& rendering_interface) -> void;
	auto remove_rendering_interface(const std::shared_ptr<rendering_interface>& rendering_interface) -> void;

	auto initialize() -> void;
	auto begin_frame() -> void;
	auto update() -> void;
	auto end_frame() -> void;
	auto shutdown() -> void;

	auto get_window() -> GLFWwindow*;

	auto is_window_closed() -> bool;
	auto is_full_screen() -> bool;
	auto is_focused() -> bool;
	auto is_minimized() -> bool;
	auto is_mouse_visible() -> bool;
	auto has_mouse_moved() -> int;

	auto get_current_monitor() -> GLFWmonitor*;
	auto get_mouse_delta_rel_top_left() -> unitless::vec2;
	auto get_mouse_delta_rel_bottom_left() -> unitless::vec2;

	auto get_fbo() -> std::optional<GLuint>;
	auto get_frame_buffer_size() -> unitless::vec2i;
	auto get_mouse_position_rel_top_left() -> unitless::vec2;
	auto get_mouse_position_rel_bottom_left() -> unitless::vec2;
	auto get_window_size() -> unitless::vec2i;
	auto get_viewport_size() -> unitless::vec2i;

	auto set_fbo(GLuint fbo_in, const unitless::vec2i& size) -> void;
	auto set_mouse_pos_relative_to_window(const unitless::vec2i& position) -> void;
	auto set_full_screen(bool fs) -> void;
	auto set_window_focused(bool focused) -> void;
	auto set_mouse_visible(bool show) -> void;
}

GLFWwindow* g_window = nullptr;

std::optional<GLuint> g_fbo;
gse::unitless::vec2i g_fbo_size = { 1, 1 };

bool g_current_full_screen = false;
bool g_full_screen = false;
bool g_window_focused = true;
bool g_mouse_visible = true;
int g_mouse_moved = 0;

std::vector<std::shared_ptr<gse::window::rendering_interface>> g_rendering_interfaces;

/// Callbacks

auto key_callback(GLFWwindow* window, const int key, int scancode, const int action, int mods) -> void {
	if (gse::input::get_keyboard().keys.contains(key)) {
		if (action == GLFW_PRESS) {
			gse::input::internal::process_event_button(gse::input::get_keyboard().keys[key], true);
		}
		else if (action == GLFW_RELEASE) {
			gse::input::internal::process_event_button(gse::input::get_keyboard().keys[key], false);
		}
	}
}

auto mouse_callback(GLFWwindow* window, const int button, const int action, int mods) -> void {
	if (gse::input::get_mouse().buttons.contains(button)) {
		if (action == GLFW_PRESS) {
			gse::input::internal::process_event_button(gse::input::get_mouse().buttons[button], true);
		}
		else if (action == GLFW_RELEASE) {
			gse::input::internal::process_event_button(gse::input::get_mouse().buttons[button], false);
		}
	}
}

auto window_focus_callback(GLFWwindow* window, const int focused) -> void {
	if (focused) {
		g_window_focused = true;
	}
	else {
		g_window_focused = false;
		gse::input::internal::reset_inputs_to_zero(); // To reset buttons
	}
}

auto window_size_callback(GLFWwindow* window, int x, int y) -> void {
	gse::input::internal::reset_inputs_to_zero();
}

auto cursor_position_callback(GLFWwindow* window, double xpos, double ypos) -> void {
	g_mouse_moved = 1;
}

auto character_callback(GLFWwindow* window, const unsigned int codepoint) -> void {
	if (codepoint < 127) {
		gse::input::internal::add_to_typed_input(codepoint);
	}
}

auto gse::window::add_rendering_interface(const std::shared_ptr<rendering_interface>& rendering_interface) -> void {
	g_rendering_interfaces.push_back(rendering_interface);
}

auto gse::window::remove_rendering_interface(const std::shared_ptr<rendering_interface>& rendering_interface) -> void {
	if (const auto it = std::ranges::find(g_rendering_interfaces, rendering_interface); it != g_rendering_interfaces.end()) {
		g_rendering_interfaces.erase(it);
	}
}

auto gse::window::initialize() -> void {
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

auto gse::window::begin_frame() -> void {
	glViewport(0, 0, get_frame_buffer_size().x, get_frame_buffer_size().y);
	glClear(GL_COLOR_BUFFER_BIT);

	for (const auto& rendering_interface : g_rendering_interfaces) {
		rendering_interface->on_pre_render();
	}
}

auto gse::window::update() -> void {
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

auto gse::window::end_frame() -> void {
	for (const auto& rendering_interface : g_rendering_interfaces) {
		rendering_interface->on_post_render();
	}

	glfwSwapBuffers(g_window);
	glfwPollEvents();
}

auto gse::window::shutdown() -> void {
	glfwTerminate();
}

/// Getters and Setters

auto gse::window::get_window() -> GLFWwindow* {
	return g_window;
}

auto gse::window::is_window_closed() -> bool {
	return glfwWindowShouldClose(g_window);
}

auto gse::window::is_full_screen() -> bool {
	return g_full_screen;
}

auto gse::window::is_focused() -> bool {
	return g_window_focused;
}

auto gse::window::is_minimized() -> bool {
	int width, height;
	glfwGetWindowSize(g_window, &width, &height);
	return width == 0 || height == 0;
}

auto gse::window::is_mouse_visible() -> bool {
	return g_mouse_visible;
}

auto gse::window::has_mouse_moved() -> int {
	return g_mouse_moved;
}

auto gse::window::get_current_monitor() -> GLFWmonitor* {
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

gse::unitless::vec2 g_last_mouse_position = { 0, 0 };

auto gse::window::get_mouse_delta_rel_top_left() -> unitless::vec2 {
	const auto current_mouse_position = unitless::vec2(get_mouse_position_rel_top_left());
	const auto delta = current_mouse_position - g_last_mouse_position;
	g_last_mouse_position = current_mouse_position;
	return delta;
}

auto gse::window::get_mouse_delta_rel_bottom_left() -> unitless::vec2 {
	auto delta_top_left = get_mouse_delta_rel_top_left();
	delta_top_left.y *= -1;
	return delta_top_left;
}

auto gse::window::get_fbo() -> std::optional<GLuint> {
	return g_fbo;
}

auto gse::window::get_frame_buffer_size() -> unitless::vec2i {
	if (g_fbo.has_value()) {
		if (g_fbo_size == unitless::vec2i(0, 0)) {
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

auto gse::window::get_mouse_position_rel_top_left() -> unitless::vec2 {
	double x = 0, y = 0;
	glfwGetCursorPos(g_window, &x, &y);
	return { static_cast<float>(x), static_cast<float>(y) };
}

auto gse::window::get_mouse_position_rel_bottom_left() -> unitless::vec2 {
	const auto top_left = get_mouse_position_rel_top_left();
	const auto window_size = get_window_size();
	return { top_left.x, window_size.y - top_left.y - 1 };
}

auto gse::window::get_window_size() -> unitless::vec2i {
	int x = 0; int y = 0;
	glfwGetWindowSize(g_window, &x, &y);
	return { x, y };
}

auto gse::window::get_viewport_size() -> unitless::vec2i {
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	return { viewport[2], viewport[3] };
}

auto gse::window::set_fbo(const GLuint fbo_in, const unitless::vec2i& size) -> void {
	g_fbo = fbo_in;
	g_fbo_size = size;
}

auto gse::window::set_mouse_pos_relative_to_window(const unitless::vec2i& position) -> void {
	glfwSetCursorPos(g_window, position.x, position.y);
}

auto gse::window::set_full_screen(const bool fs) -> void {
	g_full_screen = fs;
}

auto gse::window::set_window_focused(const bool focused) -> void {
	g_window_focused = focused;
}

auto gse::window::set_mouse_visible(const bool show) -> void {
	g_mouse_visible = show;
}