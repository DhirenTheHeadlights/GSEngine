#include "Platform/GLFW/Window.h"
#include <fstream>

#include "Core/Clock.h"
#include "Platform/GLFW/Input.h"
#include "Platform/PermaAssert.h"

#undef max
#undef min

namespace {
	GLFWwindow* window = nullptr;

	std::optional<GLuint> fbo;
	glm::ivec2 fboSize = { 0, 0 };

	bool currentFullScreen = false;
	bool fullScreen = false;
	bool windowFocused = true;
	bool mouseVisible = true;
	int mouseMoved = 0;

	std::vector<std::shared_ptr<Engine::Window::RenderingInterface>> renderingInterfaces;
}

void Engine::Window::addRenderingInterface(const std::shared_ptr<RenderingInterface>& renderingInterface) {
	renderingInterfaces.push_back(renderingInterface);
}

void Engine::Window::removeRenderingInterface(const std::shared_ptr<RenderingInterface>& renderingInterface) {
	if (const auto it = std::ranges::find(renderingInterfaces, renderingInterface); it != renderingInterfaces.end()) {
		renderingInterfaces.erase(it);
	}
}

void Engine::Window::initialize() {
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

	glfwSetKeyCallback(window, keyCallback);
	glfwSetMouseButtonCallback(window, mouseCallback);
	glfwSetWindowFocusCallback(window, windowFocusCallback);
	glfwSetWindowSizeCallback(window, windowSizeCallback);
	glfwSetCursorPosCallback(window, cursorPositionCallback);
	glfwSetCharCallback(window, characterCallback);

	permaAssertComment(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)), "Error Initializing GLAD");
}

void Engine::Window::beginFrame() {
	glViewport(0, 0, getFrameBufferSize().x, getFrameBufferSize().y);
	glClear(GL_COLOR_BUFFER_BIT);

	for (const auto& renderingInterface : renderingInterfaces) {
		renderingInterface->onPreRender();
	}
}

void Engine::Window::update() {
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

			const auto monitor = getCurrentMonitor();

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

void Engine::Window::endFrame() {
	for (const auto& renderingInterface : renderingInterfaces) {
		renderingInterface->onPostRender();
	}

	glfwSwapBuffers(window);
	glfwPollEvents();
}

void Engine::Window::shutdown() {
	glfwTerminate();
}

/// Getters and Setters

GLFWwindow* Engine::Window::getWindow() {
	return window;
}

bool Engine::Window::isWindowClosed() {
	return glfwWindowShouldClose(window);
}

bool Engine::Window::isFullScreen() {
	return fullScreen;
}

bool Engine::Window::isFocused() {
	return windowFocused;
}

bool Engine::Window::isMinimized() {
	int width, height;
	glfwGetWindowSize(window, &width, &height);
	return width == 0 || height == 0;
}

bool Engine::Window::isMouseVisible() {
	return mouseVisible;
}

int Engine::Window::hasMouseMoved() {
	return mouseMoved;
}

GLFWmonitor* Engine::Window::getCurrentMonitor() {
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

std::optional<GLuint> Engine::Window::getFbo() {
	return fbo;
}

glm::ivec2 Engine::Window::getFrameBufferSize() {
	if (fbo.has_value()) {
		return fboSize;
	}

	int x = 0; int y = 0;
	glfwGetFramebufferSize(window, &x, &y);
	return { x, y };
}

glm::ivec2 Engine::Window::getRelMousePosition() {
	double x = 0, y = 0;
	glfwGetCursorPos(window, &x, &y);
	return { x, y };
}

glm::ivec2 Engine::Window::getWindowSize() {
	int x = 0; int y = 0;
	glfwGetWindowSize(window, &x, &y);
	return { x, y };
}

glm::ivec2 Engine::Window::getViewportSize() {
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	return { viewport[2], viewport[3] };
}

void Engine::Window::setFbo(const GLuint fboIn, const glm::ivec2& size) {
	fbo = fboIn;
	fboSize = size;
}

void Engine::Window::setMousePosRelativeToWindow(const glm::ivec2& position) {
	glfwSetCursorPos(window, position.x, position.y);
}

void Engine::Window::setFullScreen(const bool fs) {
	fullScreen = fs;
}

void Engine::Window::setWindowFocused(const bool focused) {
	windowFocused = focused;
}

void Engine::Window::setMouseVisible(const bool show) {
	mouseVisible = show;
}

/// Callbacks

void Engine::Window::keyCallback(GLFWwindow* window, const int key, int scancode, const int action, int mods) {
	if (Input::getKeyboard().keys.contains(key)) {
		if (action == GLFW_PRESS) {
			Input::Internal::processEventButton(Input::getKeyboard().keys[key], true);
		}
		else if (action == GLFW_RELEASE) {
			Input::Internal::processEventButton(Input::getKeyboard().keys[key], false);
		}
	}
}

void Engine::Window::mouseCallback(GLFWwindow* window, const int button, const int action, int mods) {
	if (Input::getMouse().buttons.contains(button)) {
		// Handle mouse press and release events
		if (action == GLFW_PRESS) {
			Input::Internal::processEventButton(Input::getMouse().buttons[button], true);
		}
		else if (action == GLFW_RELEASE) {
			Input::Internal::processEventButton(Input::getMouse().buttons[button], false);
		}
	}
}

void Engine::Window::windowFocusCallback(GLFWwindow* window, const int focused) {
	if (focused) {
		windowFocused = true;
	}
	else {
		windowFocused = false;
		Input::Internal::resetInputsToZero(); // To reset buttons
	}
}

void Engine::Window::windowSizeCallback(GLFWwindow* window, int x, int y) {
	Input::Internal::resetInputsToZero();
}

void Engine::Window::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
	mouseMoved = 1;
}

void Engine::Window::characterCallback(GLFWwindow* window, const unsigned int codepoint) {
	if (codepoint < 127) {
		Input::Internal::addToTypedInput(codepoint);
	}
}