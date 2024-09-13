#include "Input.h"

Input::Keyboard keyboard;
Input::Controller controller;
Input::Mouse mouse;

void setUpKeyMaps() {
	for (int i = GLFW_KEY_A; i <= GLFW_KEY_Z; i++) {
		keyboard.keys.insert(std::make_pair(i, Input::Button()));
	}

	for (int i = GLFW_KEY_0; i <= GLFW_KEY_9; i++) {
		keyboard.keys.insert(std::make_pair(i, Input::Button()));
	}

	keyboard.keys.insert(std::make_pair(GLFW_KEY_SPACE, Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_ENTER, Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_ESCAPE, Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_UP, Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_DOWN, Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT, Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_RIGHT, Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT_CONTROL, Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_TAB, Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT_SHIFT, Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT_ALT, Input::Button()));

	for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; i++) {
		controller.buttons.insert(std::make_pair(i, Input::Button()));
	}

	for (int i = 0; i <= GLFW_MOUSE_BUTTON_LAST; i++) {
		mouse.buttons.insert(std::make_pair(i, Input::Button()));
	}
}

Input::Keyboard& Input::getKeyboard() {
	return keyboard;
}

Input::Controller& Input::getController() {
	return controller;
}

Input::Mouse& Input::getMouse() {
	return mouse;
}

void Input::internal::updateAllButtons(const float deltaTime) {
	for (auto& [fst, snd] : keyboard.keys) {
		updateButton(snd, deltaTime);
	}
	
	for(int i = 0; i <= static_cast<int>(controller.buttons.size()); i++) {
		if (!(glfwJoystickPresent(i) && glfwJoystickIsGamepad(i))) continue;

		GLFWgamepadstate state;

		if (glfwGetGamepadState(i, &state)) {
			for (auto& [b, button] : controller.buttons) {
				if (state.buttons[b] == GLFW_PRESS) {
					processEventButton(button, true);
				}
				else if (state.buttons[b] == GLFW_RELEASE) {
					processEventButton(button, false);
				}
				updateButton(button, deltaTime);
			}
			
			controller.RT = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
			controller.RT = state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];

			controller.LStick.x = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
			controller.LStick.y = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];

			controller.RStick.x = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
			controller.RStick.y = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
		
			break;
		}
	}

	for (auto& i : mouse.buttons) {
		updateButton(i.second, deltaTime);
	}

	// Update Mouse Delta
	mouse.delta = Input::getRelMousePosition() - mouse.lastPosition;
	mouse.lastPosition = Input::getRelMousePosition();
}

void Input::internal::resetInputsToZero() {
	resetTypedInput();

	for (auto& [fst, snd] : keyboard.keys) {
		snd.reset();
	}

	for (auto& [fst, snd] : controller.buttons) {
		snd.reset();
	}

	for (auto& [fst, snd] : mouse.buttons) {
		snd.reset();
	}
}

void Input::internal::addToTypedInput(const char c) {
	keyboard.typedInput += c;
}

void Input::internal::resetTypedInput() {
	keyboard.typedInput.clear();
}
