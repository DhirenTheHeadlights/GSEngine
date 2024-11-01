#include "Platform/GLFW/Input.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Core/Clock.h"
#include "Platform/GLFW/Window.h"

Engine::Input::Keyboard keyboard;
Engine::Input::Controller controller;
Engine::Input::Mouse mouse;

void Engine::Input::update() {
	Internal::updateAllButtons(MainClock::getDeltaTime().as<Units::Seconds>());
	Internal::resetTypedInput();
}

void Engine::Input::setUpKeyMaps() {
	for (int i = GLFW_KEY_A; i <= GLFW_KEY_Z; i++) {
		keyboard.keys.insert(std::make_pair(i, Button()));
	}

	for (int i = GLFW_KEY_0; i <= GLFW_KEY_9; i++) {
		keyboard.keys.insert(std::make_pair(i, Button()));
	}

	keyboard.keys.insert(std::make_pair(GLFW_KEY_SPACE, Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_ENTER, Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_ESCAPE, Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_UP, Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_DOWN, Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT, Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_RIGHT, Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT_CONTROL, Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_TAB, Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT_SHIFT, Button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT_ALT, Button()));

	for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; i++) {
		controller.buttons.insert(std::make_pair(i, Button()));
	}

	for (int i = 0; i <= GLFW_MOUSE_BUTTON_LAST; i++) {
		mouse.buttons.insert(std::make_pair(i, Button()));
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

void Engine::Input::Internal::updateAllButtons(const float deltaTime) {
	for (auto& snd : keyboard.keys | std::views::values) {
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
			
			controller.rt = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
			controller.rt = state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];

			controller.lStick.x = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
			controller.lStick.y = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];

			controller.rStick.x = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
			controller.rStick.y = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
		
			break;
		}
	}

	for (auto& snd : mouse.buttons | std::views::values) {
		updateButton(snd, deltaTime);
	}

	// Update Mouse Delta
	mouse.delta = Window::getRelMousePosition() - mouse.lastPosition;
	mouse.lastPosition = Window::getRelMousePosition();
}

void Engine::Input::Internal::resetInputsToZero() {
	resetTypedInput();

	for (auto& snd : keyboard.keys | std::views::values) {
		snd.reset();
	}

	for (auto& snd : controller.buttons | std::views::values) {
		snd.reset();
	}

	for (auto& snd : mouse.buttons | std::views::values) {
		snd.reset();
	}
}

void Engine::Input::Internal::addToTypedInput(const char c) {
	keyboard.typedInput += c;
}

void Engine::Input::Internal::resetTypedInput() {
	keyboard.typedInput.clear();
}
