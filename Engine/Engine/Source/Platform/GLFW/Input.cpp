#include "Platform/GLFW/Input.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Core/Clock.h"
#include "Platform/GLFW/Window.h"

gse::Input::Keyboard keyboard;
gse::Input::Controller controller;
gse::Input::Mouse mouse;

void gse::Input::update() {
	Internal::updateAllButtons();
	Internal::resetTypedInput();
}

namespace {
	bool blockInputs = false;
}

void gse::Input::setUpKeyMaps() {
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

gse::Input::Keyboard& gse::Input::getKeyboard() {
	return keyboard;
}

gse::Input::Controller& gse::Input::getController() {
	return controller;
}

gse::Input::Mouse& gse::Input::getMouse() {
	return mouse;
}

void gse::Input::setInputsBlocked(const bool blocked) {
	blockInputs = blocked;
}

void gse::Input::Internal::processEventButton(Button& button, const bool newState) {
	button.newState = newState;
}

void gse::Input::Internal::updateButton(Button& button) {
	if (button.newState == 1) {
		if (button.held) {
			button.pressed = false;
		}
		else {
			button.pressed = true;
			button.toggled = !button.toggled;
		}

		button.held = true;
		button.released = false;
	}
	else if (button.newState == 0) {
		button.held = false;
		button.pressed = false;
		button.released = true;
	}
	else {
		button.pressed = false;
		button.released = false;
	}

	if (button.pressed)
	{
		button.typed = true;
		button.typedTime = 0.48f;
	}
	else if (button.held) {
		button.typedTime -= MainClock::getDeltaTime().as<Seconds>();

		if (button.typedTime < 0.f)
		{
			button.typedTime += 0.07f;
			button.typed = true;
		}
		else {
			button.typed = false;
		}

	}
	else {
		button.typedTime = 0;
		button.typed = false;
	}
	button.newState = -1;
}

void gse::Input::Internal::updateAllButtons() {
	if (blockInputs) return;

	for (auto& button : keyboard.keys | std::views::values) {
		updateButton(button);
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
				updateButton(button);
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

	for (auto& button : mouse.buttons | std::views::values) {
		updateButton(button);
	}

	mouse.delta = Window::getRelMousePosition() - mouse.lastPosition;
	mouse.lastPosition = Window::getRelMousePosition();
}

void gse::Input::Internal::resetInputsToZero() {
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

void gse::Input::Internal::addToTypedInput(const char input) {
	keyboard.typedInput += input;
}

void gse::Input::Internal::resetTypedInput() {
	keyboard.typedInput.clear();
}
