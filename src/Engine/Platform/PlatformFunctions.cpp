#include "PlatformFunctions.h"

#undef max
#undef min

GLFWwindow* Platform::window = nullptr;

bool Platform::currentFullScreen = false;
bool Platform::fullScreen = false;
bool Platform::windowFocused = true;
int Platform::mouseMoved = 0;

void Platform::setMousePosRelativeToWindow(int x, int y) {
	glfwSetCursorPos(window, x, y);
}

glm::ivec2 Platform::getFrameBufferSize() {
	int x = 0; int y = 0;
	glfwGetFramebufferSize(window, &x, &y);
	return { x, y };
}

glm::ivec2 Platform::getRelMousePosition() {
	double x = 0, y = 0;
	glfwGetCursorPos(window, &x, &y);
	return { x, y };
}

glm::ivec2 Platform::getWindowSize() {
	int x = 0; int y = 0;
	glfwGetWindowSize(window, &x, &y);
	return { x, y };
}

void Platform::showMouse(bool show) {
	if (show) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	else {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
	}
}

bool Platform::writeEntireFile(const char* name, void* buffer, size_t size) {
	std::ofstream f(name, std::ios::binary);

	if (!f.is_open()) {
		return 0;
	}

	f.write((char*)buffer, size);

	f.close();

	return 1;
}

bool Platform::readEntireFile(const char* name, void* buffer, size_t size) {
	std::ifstream f(name, std::ios::binary);

	if (!f.is_open()) {
		return 0;
	}

	f.read(static_cast<char*>(buffer), size);

	f.close();

	return 1;
}

//https://stackoverflow.com/questions/21421074/how-to-create-a-full-screen-window-on-the-current-monitor-with-glfw
GLFWmonitor* Platform::getCurrentMonitor() {
	int nmonitors, i;
	int wx, wy, ww, wh;
	int mx, my, mw, mh;
	int overlap, bestoverlap;
	GLFWmonitor* bestmonitor;
	GLFWmonitor** monitors;
	const GLFWvidmode* mode;

	bestoverlap = 0;
	bestmonitor = NULL;

	glfwGetWindowPos(window, &wx, &wy);
	glfwGetWindowSize(window, &ww, &wh);
	monitors = glfwGetMonitors(&nmonitors);

	for (i = 0; i < nmonitors; i++) {
		mode = glfwGetVideoMode(monitors[i]);
		glfwGetMonitorPos(monitors[i], &mx, &my);
		mw = mode->width;
		mh = mode->height;

		overlap =
			std::max(0, std::min(wx + ww, mx + mw) - std::max(wx, mx)) *
			std::max(0, std::min(wy + wh, my + mh) - std::max(wy, my));

		if (bestoverlap < overlap) {
			bestoverlap = overlap;
			bestmonitor = monitors[i];
		}
	}

	return bestmonitor;
}