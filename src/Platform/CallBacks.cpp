#include "CallBacks.h"

void Platform::keyCallback(GLFWwindow* window, const int key, int scancode, const int action, int mods) {
	// Check if the key exists in the map
	if (Platform::getKeyboard().keys.find(key) != Platform::getKeyboard().keys.end()) {
		// Handle key press and release events
		if (action == GLFW_PRESS) {
			Platform::internal::processEventButton(Platform::getKeyboard().keys[key], true);
		}
		else if (action == GLFW_RELEASE) {
			Platform::internal::processEventButton(Platform::getKeyboard().keys[key], false);
		}
	}
}

void Platform::mouseCallback(GLFWwindow* window, const int button, const int action, int mods) {
	if (Platform::getMouse().buttons.find(button) != Platform::getMouse().buttons.end()) {
		// Handle mouse press and release events
		if (action == GLFW_PRESS) {
			Platform::internal::processEventButton(Platform::getMouse().buttons[button], true);
		}
		else if (action == GLFW_RELEASE) {
			Platform::internal::processEventButton(Platform::getMouse().buttons[button], false);
		}
	}
}

void Platform::windowFocusCallback(GLFWwindow* window, const int focused) {
    if (focused) {
		Platform::windowFocused = true;
	}
    else {
		Platform::windowFocused = false;
		Platform::internal::resetInputsToZero(); // To reset buttons
	}
}

void Platform::windowSizeCallback(GLFWwindow* window, int x, int y) {
	Platform::internal::resetInputsToZero();
}

void Platform::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
	Platform::mouseMoved = 1;
}

void Platform::characterCallback(GLFWwindow* window, const unsigned int codepoint) {
    if (codepoint < 127) {
		Platform::internal::addToTypedInput(codepoint);
	}
}
