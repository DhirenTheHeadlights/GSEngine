#include "PlatformInput.h"

Platform::Button keyBoard[Platform::Button::BUTTONS_COUNT];
Platform::Button leftMouse;
Platform::Button rightMouse;

Platform::ControllerButtons controllerButtons;
std::string typedInput;
glm::ivec2 mouseDelta;
glm::ivec2 lastMousePos;

int Platform::isButtonHeld(int key) {
	if (key < Button::A || key >= Button::BUTTONS_COUNT) { return 0; }

	return keyBoard[key].held;
}

int Platform::isButtonPressedOn(int key) {
	if (key < Button::A || key >= Button::BUTTONS_COUNT) { return 0; }

	return keyBoard[key].pressed;
}

int Platform::isButtonReleased(int key) {
	if (key < Button::A || key >= Button::BUTTONS_COUNT) { return 0; }

	return keyBoard[key].released;
}

int Platform::isButtonTyped(int key) {
	if (key < Button::A || key >= Button::BUTTONS_COUNT) { return 0; }

	return keyBoard[key].typed;
}

int Platform::isLMousePressed() {
	return leftMouse.pressed;
}

int Platform::isRMousePressed() {
	return rightMouse.pressed;
}

int Platform::isLMouseReleased() {
	return leftMouse.released;
}

int Platform::isRMouseReleased() {
	return rightMouse.released;
}

int Platform::isLMouseHeld() {
	return leftMouse.held;
}

int Platform::isRMouseHeld() {
	return rightMouse.held;
}

Platform::ControllerButtons Platform::getControllerButtons() {
	return Platform::windowFocused ? controllerButtons : Platform::ControllerButtons{};
}

std::string Platform::getTypedInput() {
	return typedInput;
}

glm::vec2 Platform::getMouseDelta() {
	return mouseDelta;
}

void Platform::internal::setButtonState(int button, int newState) {
	processEventButton(keyBoard[button], newState);
}

void Platform::internal::setLeftMouseState(int newState) {
	processEventButton(leftMouse, newState);
}

void Platform::internal::setRightMouseState(int newState) {
	processEventButton(rightMouse, newState);
}


void Platform::internal::updateAllButtons(float deltaTime) {
	for (int i = 0; i < Platform::Button::BUTTONS_COUNT; i++) {
		updateButton(keyBoard[i], deltaTime);
	}

	updateButton(leftMouse, deltaTime);
	updateButton(rightMouse, deltaTime);
	
	for(int i=0; i<=GLFW_JOYSTICK_LAST; i++) {
		if(glfwJoystickPresent(i) && glfwJoystickIsGamepad(i)) {
			GLFWgamepadstate state;

			if (glfwGetGamepadState(i, &state)) {
				for (int b = 0; b <= GLFW_GAMEPAD_BUTTON_LAST; b++) {
					if(state.buttons[b] == GLFW_PRESS) {
						processEventButton(controllerButtons.buttons[b], 1);
					}
					else if (state.buttons[b] == GLFW_RELEASE) {
						processEventButton(controllerButtons.buttons[b], 0);
					}
					updateButton(controllerButtons.buttons[b], deltaTime);
				}
				
				controllerButtons.LT = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
				controllerButtons.RT = state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];

				controllerButtons.LStick.x = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
				controllerButtons.LStick.y = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];

				controllerButtons.RStick.x = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
				controllerButtons.RStick.y = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
			
				break;
			}
		}
	}

	// Update Mouse Delta
	mouseDelta = Platform::getRelMousePosition() - lastMousePos;
	lastMousePos = Platform::getRelMousePosition();
}

void Platform::internal::resetInputsToZero() {
	resetTypedInput();

	for (int i = 0; i < Platform::Button::BUTTONS_COUNT; i++) {
		resetButtonToZero(keyBoard[i]);
	}

	resetButtonToZero(leftMouse);
	resetButtonToZero(rightMouse);
	
	controllerButtons.setAllToZero();
}

void Platform::internal::addToTypedInput(char c) {
	typedInput += c;
}

void Platform::internal::resetTypedInput() {
	typedInput.clear();
}
