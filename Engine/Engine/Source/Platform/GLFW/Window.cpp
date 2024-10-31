#include "Platform/GLFW/Window.h"
#include <fstream>


#include "Core/Clock.h"
#include "Platform/GLFW/Input.h"
#include "Platform/PermaAssert.h"

#undef max
#undef min

GLFWwindow* Engine::Platform::window = nullptr;

bool Engine::Platform::currentFullScreen = false;
bool Engine::Platform::fullScreen = false;
bool Engine::Platform::windowFocused = true;
bool Engine::Platform::mouseVisible = true;
int Engine::Platform::mouseMoved = 0;

void Engine::Platform::initialize() {
	permaAssertComment(glfwInit(), "Error initializing GLFW");
	glfwWindowHint(GLFW_SAMPLES, 4);

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
	window = glfwCreateWindow(w, h, "SavantShooter", monitor, nullptr);
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

void Engine::Platform::update() {
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

void Engine::Platform::shutdown() {
	glfwTerminate();
}

void Engine::Platform::setMousePosRelativeToWindow(const glm::ivec2& position) {
	glfwSetCursorPos(window, position.x, position.y);
}

glm::ivec2 Engine::Platform::getFrameBufferSize() {
	int x = 0; int y = 0;
	glfwGetFramebufferSize(window, &x, &y);
	return { x, y };
}

glm::ivec2 Engine::Platform::getRelMousePosition() {
	double x = 0, y = 0;
	glfwGetCursorPos(window, &x, &y);
	return { x, y };
}

glm::ivec2 Engine::Platform::getWindowSize() {
	int x = 0; int y = 0;
	glfwGetWindowSize(window, &x, &y);
	return { x, y };
}

//https://stackoverflow.com/questions/21421074/how-to-create-a-full-screen-window-on-the-current-monitor-with-glfw
GLFWmonitor* Engine::Platform::getCurrentMonitor() {
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

/// Callbacks

void Engine::Platform::keyCallback(GLFWwindow* window, const int key, int scancode, const int action, int mods) {
	// Check if the key exists in the map
	if (Input::getKeyboard().keys.contains(key)) {
		// Handle key press and release events
		if (action == GLFW_PRESS) {
			Input::Internal::processEventButton(Input::getKeyboard().keys[key], true);
		}
		else if (action == GLFW_RELEASE) {
			Input::Internal::processEventButton(Input::getKeyboard().keys[key], false);
		}
	}
}

void Engine::Platform::mouseCallback(GLFWwindow* window, const int button, const int action, int mods) {
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

void Engine::Platform::windowFocusCallback(GLFWwindow* window, const int focused) {
	if (focused) {
		windowFocused = true;
	}
	else {
		windowFocused = false;
		Input::Internal::resetInputsToZero(); // To reset buttons
	}
}

void Engine::Platform::windowSizeCallback(GLFWwindow* window, int x, int y) {
	Input::Internal::resetInputsToZero();
}

void Engine::Platform::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
	mouseMoved = 1;
}

void Engine::Platform::characterCallback(GLFWwindow* window, const unsigned int codepoint) {
	if (codepoint < 127) {
		Input::Internal::addToTypedInput(codepoint);
	}
}