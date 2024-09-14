#include "PlatformFunctions.h"
#include <fstream>

#undef max
#undef min

GLFWwindow* Engine::Platform::window = nullptr;

bool Engine::Platform::currentFullScreen = false;
bool Engine::Platform::fullScreen = false;
bool Engine::Platform::windowFocused = true;
int Engine::Platform::mouseMoved = 0;

void Engine::Platform::setMousePosRelativeToWindow(const int x, const int y) {
	glfwSetCursorPos(window, x, y);
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

void Engine::Platform::showMouse(const bool show) {
	if (show) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	else {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
	}
}

bool Engine::Platform::writeEntireFile(const char* name, void* buffer, const size_t size) {
	std::ofstream f(name, std::ios::binary);

	if (!f.is_open()) {
		return false;
	}

	f.write(static_cast<char*>(buffer), size);

	f.close();

	return true;
}

bool Engine::Platform::readEntireFile(const char* name, void* buffer, const size_t size) {
	std::ifstream f(name, std::ios::binary);

	if (!f.is_open()) {
		return false;
	}

	f.read(static_cast<char*>(buffer), size);

	f.close();

	return true;
}

//https://stackoverflow.com/questions/21421074/how-to-create-a-full-screen-window-on-the-current-monitor-with-glfw
GLFWmonitor* Engine::Platform::getCurrentMonitor() {
	int numberOfMonitors;
	int wx, wy, ww, wh;
	int mx, my;

	int bestOverlap = 0;
	GLFWmonitor* bestMonitor = nullptr;

	glfwGetWindowPos(Engine::Platform::window, &wx, &wy);
	glfwGetWindowSize(Engine::Platform::window, &ww, &wh);
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