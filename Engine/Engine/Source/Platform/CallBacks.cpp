#include "Engine/Include/Platform/CallBacks.h"
#include "Engine/Include/Input/Input.h"
#include "Engine/Include/Platform/Platform.h"

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
		Engine::Input::Internal::addToTypedInput(codepoint);
	}
}
