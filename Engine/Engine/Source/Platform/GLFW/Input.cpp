#include "Platform/GLFW/Input.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Core/Clock.h"
#include "Platform/GLFW/Window.h"

gse::input::keyboard keyboard;
gse::input::controller controller;
gse::input::mouse mouse;

void gse::input::update() {
	internal::update_all_buttons();
	internal::reset_typed_input();
}

namespace {
	bool blockInputs = false;
}

void gse::input::set_up_key_maps() {
	for (int i = GLFW_KEY_A; i <= GLFW_KEY_Z; i++) {
		keyboard.keys.insert(std::make_pair(i, button()));
	}

	for (int i = GLFW_KEY_0; i <= GLFW_KEY_9; i++) {
		keyboard.keys.insert(std::make_pair(i, button()));
	}

	keyboard.keys.insert(std::make_pair(GLFW_KEY_SPACE, button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_ENTER, button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_ESCAPE, button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_UP, button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_DOWN, button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT, button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_RIGHT, button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT_CONTROL, button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_TAB, button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT_SHIFT, button()));
	keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT_ALT, button()));

	for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; i++) {
		controller.buttons.insert(std::make_pair(i, button()));
	}

	for (int i = 0; i <= GLFW_MOUSE_BUTTON_LAST; i++) {
		mouse.buttons.insert(std::make_pair(i, button()));
	}
}

gse::input::keyboard& gse::input::get_keyboard() {
	return keyboard;
}

gse::input::controller& gse::input::get_controller() {
	return controller;
}

gse::input::mouse& gse::input::get_mouse() {
	return mouse;
}

void gse::input::set_inputs_blocked(const bool blocked) {
	blockInputs = blocked;
}

void gse::input::internal::process_event_button(button& button, const bool new_state) {
	button.new_state = new_state;
}

void gse::input::internal::update_button(button& button) {
	if (button.new_state == 1) {
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
	else if (button.new_state == 0) {
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
		button.typed_time = 0.48f;
	}
	else if (button.held) {
		button.typed_time -= main_clock::get_delta_time().as<Seconds>();

		if (button.typed_time < 0.f)
		{
			button.typed_time += 0.07f;
			button.typed = true;
		}
		else {
			button.typed = false;
		}

	}
	else {
		button.typed_time = 0;
		button.typed = false;
	}
	button.new_state = -1;
}

void gse::input::internal::update_all_buttons() {
	if (blockInputs) return;

	for (auto& button : keyboard.keys | std::views::values) {
		update_button(button);
	}
	
	for(int i = 0; i <= static_cast<int>(controller.buttons.size()); i++) {
		if (!(glfwJoystickPresent(i) && glfwJoystickIsGamepad(i))) continue;

		GLFWgamepadstate state;

		if (glfwGetGamepadState(i, &state)) {
			for (auto& [b, button] : controller.buttons) {
				if (state.buttons[b] == GLFW_PRESS) {
					process_event_button(button, true);
				}
				else if (state.buttons[b] == GLFW_RELEASE) {
					process_event_button(button, false);
				}
				update_button(button);
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
		update_button(button);
	}

	mouse.delta = window::get_rel_mouse_position() - mouse.lastPosition;
	mouse.lastPosition = window::get_rel_mouse_position();
}

void gse::input::internal::reset_inputs_to_zero() {
	reset_typed_input();

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

void gse::input::internal::add_to_typed_input(const char input) {
	keyboard.typedInput += input;
}

void gse::input::internal::reset_typed_input() {
	keyboard.typedInput.clear();
}
