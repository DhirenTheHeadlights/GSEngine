#include "CallBacks.h"

void Platform::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if ((action == GLFW_REPEAT || action == GLFW_PRESS) && key == GLFW_KEY_BACKSPACE) {
        Platform::internal::addToTypedInput(8);
    }

    bool state = 0;

    if (action == GLFW_PRESS) {
        state = 1;
    }
    else if (action == GLFW_RELEASE) {
        state = 0;
    }
    else {
        return;
    }

    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
        int index = key - GLFW_KEY_A;
        Platform::internal::setButtonState(Platform::Button::A + index, state);
    }
    else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
        int index = key - GLFW_KEY_0;
        Platform::internal::setButtonState(Platform::Button::NR0 + index, state);
    }
    else {
        // Special Keys
        if (key == GLFW_KEY_SPACE) {
            Platform::internal::setButtonState(Platform::Button::Space, state);
        }
        else if (key == GLFW_KEY_ENTER) {
            Platform::internal::setButtonState(Platform::Button::Enter, state);
        }
        else if (key == GLFW_KEY_ESCAPE) {
            Platform::internal::setButtonState(Platform::Button::Escape, state);
        }
        else if (key == GLFW_KEY_UP) {
            Platform::internal::setButtonState(Platform::Button::Up, state);
        }
        else if (key == GLFW_KEY_DOWN) {
            Platform::internal::setButtonState(Platform::Button::Down, state);
        }
        else if (key == GLFW_KEY_LEFT) {
            Platform::internal::setButtonState(Platform::Button::Left, state);
        }
        else if (key == GLFW_KEY_RIGHT) {
            Platform::internal::setButtonState(Platform::Button::Right, state);
        }
        else if (key == GLFW_KEY_LEFT_CONTROL) {
            Platform::internal::setButtonState(Platform::Button::LeftCtrl, state);
        }
        else if (key == GLFW_KEY_TAB) {
            Platform::internal::setButtonState(Platform::Button::Tab, state);
        }
        else if (key == GLFW_KEY_LEFT_SHIFT) {
            Platform::internal::setButtonState(Platform::Button::LeftShift, state);
        }
        else if (key == GLFW_KEY_LEFT_ALT) {
            Platform::internal::setButtonState(Platform::Button::LeftAlt, state);
        }
    }
}

void Platform::mouseCallback(GLFWwindow* window, int key, int action, int mods) {
	bool state = 0;

    if (action == GLFW_PRESS) {
		state = 1;
	}
    else if (action == GLFW_RELEASE) {
		state = 0;
	}
    else {
		return;
	}

    if (key == GLFW_MOUSE_BUTTON_LEFT) {
		Platform::internal::setLeftMouseState(state);
	}
    else if (key == GLFW_MOUSE_BUTTON_RIGHT) {
		Platform::internal::setRightMouseState(state);
	}
}

void Platform::windowFocusCallback(GLFWwindow* window, int focused) {
    if (focused) {
		Platform::windowFocused = 1;
	}
    else {
		Platform::windowFocused = 0;
		Platform::internal::resetInputsToZero(); // To reset buttons
	}
}

void Platform::windowSizeCallback(GLFWwindow* window, int x, int y) {
	Platform::internal::resetInputsToZero();
}

void Platform::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
	Platform::mouseMoved = 1;
}

void Platform::characterCallback(GLFWwindow* window, unsigned int codepoint) {
    if (codepoint < 127) {
		Platform::internal::addToTypedInput(codepoint);
	}
}
