#include "Input.h"

#include <GLFW/glfw3.h>

Engine::Input::Keyboard keyboard;
Engine::Input::Controller controller;
Engine::Input::Mouse mouse;

void setUpKeyMaps() {
	for (int i = GLFW_KEY_A; i <= GLFW_KEY_Z; i++) {
		keyboard.keys.insert(std::make_pair(i, Engine::Input::Button()));
	}

	for (int i = GLFW_KEY_0; i <= GLFW_KEY_9; i++) {
		keyboard.keys.insert(std::make_pair(i, Engine::Input::Button()));
	}

	keyboard.keys.insert(std::make_pair(GLFW_KEY_SPACE, Engine::Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_ENTER, Engine::Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_ESCAPE, Engine::Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_UP, Engine::Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_DOWN, Engine::Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT, Engine::Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_RIGHT, Engine::Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT_CONTROL, Engine::Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_TAB, Engine::Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT_SHIFT, Engine::Input::Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT_ALT, Engine::Input::Button()));

	for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; i++) {
		controller.buttons.insert(std::make_pair(i, Engine::Input::Button()));
	}

	for (int i = 0; i <= GLFW_MOUSE_BUTTON_LAST; i++) {
		mouse.buttons.insert(std::make_pair(i, Engine::Input::Button()));
	}
}

Engine::Input::Keyboard& Engine::Input::getKeyboard() {
	return keyboard;
}

Engine::Input::Controller& Engine::Input::getController() {
	return controller;
}

Engine::Input::Mouse& Engine::Input::getMouse() {
	return mouse;
}

void Engine::Input::internal::updateAllButtons(const float deltaTime) {
	for (auto& [fst, snd] : keyboard.keys) {
		internal::updateButton(snd, deltaTime);
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
	mouse.delta = Platform::getRelMousePosition() - mouse.lastPosition;
	mouse.lastPosition = Platform::getRelMousePosition();
}

void Engine::Input::internal::resetInputsToZero() {
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

void Engine::Input::internal::addToTypedInput(const char c) {
	keyboard.typedInput += c;
}

void Engine::Input::internal::resetTypedInput() {
	keyboard.typedInput.clear();
}
