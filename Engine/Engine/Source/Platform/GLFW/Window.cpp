#include "Platform/GLFW/Window.h"
#include <fstream>

#include "Core/Clock.h"
#include "Platform/PermaAssert.h"
#include "Platform/GLFW/Input.h"

#undef max
#undef min

namespace {
	GLFWwindow* window = nullptr;

	std::optional<GLuint> fbo;
	glm::ivec2 fboSize = { 1, 1 };

	bool currentFullScreen = false;
	bool fullScreen = false;
	bool windowFocused = true;
	bool mouseVisible = true;
	int mouseMoved = 0;

	std::vector<std::shared_ptr<gse::window::rendering_interface>> renderingInterfaces;
}

void gse::window::add_rendering_interface(const std::shared_ptr<rendering_interface>& rendering_interface) {
	renderingInterfaces.push_back(rendering_interface);
}

void gse::window::remove_rendering_interface(const std::shared_ptr<rendering_interface>& rendering_interface) {
	if (const auto it = std::ranges::find(renderingInterfaces, rendering_interface); it != renderingInterfaces.end()) {
		renderingInterfaces.erase(it);
	}
}

void gse::window::initialize() {
	permaAssertComment(glfwInit(), "Error initializing GLFW");
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
	window = glfwCreateWindow(w, h, "SavantShooter", nullptr, nullptr);
	glfwSetWindowPos(window, 0.f, 0.f);
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	glfwSetKeyCallback(window, key_callback);
	glfwSetMouseButtonCallback(window, mouse_callback);
	glfwSetWindowFocusCallback(window, window_focus_callback);
	glfwSetWindowSizeCallback(window, window_size_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetCharCallback(window, character_callback);

	permaAssertComment(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)), "Error Initializing GLAD");
}

void gse::window::begin_frame() {
	glViewport(0, 0, get_frame_buffer_size().x, get_frame_buffer_size().y);
	glClear(GL_COLOR_BUFFER_BIT);

	for (const auto& renderingInterface : renderingInterfaces) {
		renderingInterface->on_pre_render();
	}
}

void gse::window::update() {
	int w = 0, h = 0;
	glfwGetWindowSize(window, &w, &h);
	if (windowFocused && currentFullScreen != fullScreen) {
		static int lastW = w;
		static int lastH = h;
		static int lastPosX = 0;
		static int lastPosY = 0;

		if (fullScreen) {
			lastW = w;
			lastH = h;

			glfwGetWindowPos(window, &lastPosX, &lastPosY);

			const auto monitor = get_current_monitor();

			const GLFWvidmode* mode = glfwGetVideoMode(monitor);

			// Switch to full screen
			glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

			currentFullScreen = true;

		}
		else {
			glfwSetWindowMonitor(window, nullptr, lastPosX, lastPosY, lastW, lastH, 0);

			currentFullScreen = false;
		}

		mouseMoved = 0;
	}

	if (mouseVisible) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	else {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
}

void gse::window::end_frame() {
	for (const auto& renderingInterface : renderingInterfaces) {
		renderingInterface->on_post_render();
	}

	glfwSwapBuffers(window);
	glfwPollEvents();
}

void gse::window::shutdown() {
	glfwTerminate();
}

/// Getters and Setters

GLFWwindow* gse::window::get_window() {
	return window;
}

bool gse::window::is_window_closed() {
	return glfwWindowShouldClose(window);
}

bool gse::window::is_full_screen() {
	return fullScreen;
}

bool gse::window::is_focused() {
	return windowFocused;
}

bool gse::window::is_minimized() {
	int width, height;
	glfwGetWindowSize(window, &width, &height);
	return width == 0 || height == 0;
}

bool gse::window::is_mouse_visible() {
	return mouseVisible;
}

int gse::window::has_mouse_moved() {
	return mouseMoved;
}

GLFWmonitor* gse::window::get_current_monitor() {
	int numberOfMonitors;
	int wx, wy, ww, wh;
	int mx, my;

	int bestOverlap = 0;
	GLFWmonitor* bestMonitor = nullptr;

	glfwGetWindowPos(window, &wx, &wy);
	glfwGetWindowSize(window, &ww, &wh);
	GLFWmonitor** monitors = glfwGetMonitors(&numberOfMonitors);

	for (int i = 0; i < numberOfMonitors; i++) {
		const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
		glfwGetMonitorPos(monitors[i], &mx, &my);
		const int mw = mode->width;
		const int mh = mode->height;

		const int overlap = std::max(0, std::min(wx + ww, mx + mw) - std::max(wx, mx)) *
			std::max(0, std::min(wy + wh, my + mh) - std::max(wy, my));

		if (bestOverlap < overlap) {
			bestOverlap = overlap;
			bestMonitor = monitors[i];
		}
	}

	return bestMonitor;
}

std::optional<GLuint> gse::window::get_fbo() {
	return fbo;
}

glm::ivec2 gse::window::get_frame_buffer_size() {
	if (fbo.has_value()) {
		if (fboSize == glm::ivec2(0, 0)) {
			return { 1, 1 };
		}
		return fboSize;
	}

	int x = 0; int y = 0;
	glfwGetFramebufferSize(window, &x, &y);
	if (x == 0 || y == 0) {
		return { 1, 1 };
	}
	return { x, y };
}

glm::ivec2 gse::window::get_rel_mouse_position() {
	double x = 0, y = 0;
	glfwGetCursorPos(window, &x, &y);
	return { x, y };
}

glm::ivec2 gse::window::get_window_size() {
	int x = 0; int y = 0;
	glfwGetWindowSize(window, &x, &y);
	return { x, y };
}

glm::ivec2 gse::window::get_viewport_size() {
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	return { viewport[2], viewport[3] };
}

void gse::window::set_fbo(const GLuint fbo_in, const glm::ivec2& size) {
	fbo = fbo_in;
	fboSize = size;
}

void gse::window::set_mouse_pos_relative_to_window(const glm::ivec2& position) {
	glfwSetCursorPos(window, position.x, position.y);
}

void gse::window::set_full_screen(const bool fs) {
	fullScreen = fs;
}

void gse::window::set_window_focused(const bool focused) {
	windowFocused = focused;
}

void gse::window::set_mouse_visible(const bool show) {
	mouseVisible = show;
}

/// Callbacks

void gse::window::key_callback(GLFWwindow* window, const int key, int scancode, const int action, int mods) {
	if (input::get_keyboard().keys.contains(key)) {
		if (action == GLFW_PRESS) {
			input::internal::process_event_button(input::get_keyboard().keys[key], true);
		}
		else if (action == GLFW_RELEASE) {
			input::internal::process_event_button(input::get_keyboard().keys[key], false);
		}
	}
}

void gse::window::mouse_callback(GLFWwindow* window, const int button, const int action, int mods) {
	if (input::get_mouse().buttons.contains(button)) {
		if (action == GLFW_PRESS) {
			input::internal::process_event_button(input::get_mouse().buttons[button], true);
		}
		else if (action == GLFW_RELEASE) {
			input::internal::process_event_button(input::get_mouse().buttons[button], false);
		}
	}
}

void gse::window::window_focus_callback(GLFWwindow* window, const int focused) {
	if (focused) {
		windowFocused = true;
	}
	else {
		windowFocused = false;
		input::internal::reset_inputs_to_zero(); // To reset buttons
	}
}

void gse::window::window_size_callback(GLFWwindow* window, int x, int y) {
	input::internal::reset_inputs_to_zero();
}

void gse::window::cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
	mouseMoved = 1;
}

void gse::window::character_callback(GLFWwindow* window, const unsigned int codepoint) {
	if (codepoint < 127) {
		input::internal::add_to_typed_input(codepoint);
	}
}