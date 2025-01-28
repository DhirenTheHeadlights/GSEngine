module;

#include "GLFW/glfw3.h"

module gse.platform.glfw.input;

import std;
import glm;

import gse.core.clock;
import gse.platform.glfw.window;

gse::input::keyboard g_keyboard;
gse::input::controller g_controller;
gse::input::mouse g_mouse;

void gse::input::update() {
	internal::update_all_buttons();
	internal::reset_typed_input();
}

namespace {
	bool g_block_inputs = false;
}

void gse::input::set_up_key_maps() {
	for (int i = GLFW_KEY_A; i <= GLFW_KEY_Z; i++) {
		g_keyboard.keys.insert(std::make_pair(i, button()));
	}

	for (int i = GLFW_KEY_0; i <= GLFW_KEY_9; i++) {
		g_keyboard.keys.insert(std::make_pair(i, button()));
	}

	g_keyboard.keys.insert(std::make_pair(GLFW_KEY_SPACE, button()));
	g_keyboard.keys.insert(std::make_pair(GLFW_KEY_ENTER, button()));
	g_keyboard.keys.insert(std::make_pair(GLFW_KEY_ESCAPE, button()));
	g_keyboard.keys.insert(std::make_pair(GLFW_KEY_UP, button()));
	g_keyboard.keys.insert(std::make_pair(GLFW_KEY_DOWN, button()));
	g_keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT, button()));
	g_keyboard.keys.insert(std::make_pair(GLFW_KEY_RIGHT, button()));
	g_keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT_CONTROL, button()));
	g_keyboard.keys.insert(std::make_pair(GLFW_KEY_TAB, button()));
	g_keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT_SHIFT, button()));
	g_keyboard.keys.insert(std::make_pair(GLFW_KEY_LEFT_ALT, button()));

	for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; i++) {
		g_controller.buttons.insert(std::make_pair(i, button()));
	}

	for (int i = 0; i <= GLFW_MOUSE_BUTTON_LAST; i++) {
		g_mouse.buttons.insert(std::make_pair(i, button()));
	}
}

gse::input::keyboard& gse::input::get_keyboard() {
	return g_keyboard;
}

gse::input::controller& gse::input::get_controller() {
	return g_controller;
}

gse::input::mouse& gse::input::get_mouse() {
	return g_mouse;
}

void gse::input::set_inputs_blocked(const bool blocked) {
	g_block_inputs = blocked;
}

void gse::input::internal::process_event_button(button& button, const bool new_state) {
	button.new_state = static_cast<int8_t>(new_state);
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
		button.typed_time -= main_clock::get_raw_delta_time().as<units::seconds>();

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
	if (g_block_inputs) return;

	for (auto& button : g_keyboard.keys | std::views::values) {
		update_button(button);
	}
	
	for(int i = 0; i <= static_cast<int>(g_controller.buttons.size()); i++) {
		if (!(glfwJoystickPresent(i) && glfwJoystickIsGamepad(i))) continue;

		GLFWgamepadstate state;

		if (glfwGetGamepadState(i, &state)) {
			for (auto& [b, button] : g_controller.buttons) {
				if (state.buttons[b] == GLFW_PRESS) {
					process_event_button(button, true);
				}
				else if (state.buttons[b] == GLFW_RELEASE) {
					process_event_button(button, false);
				}
				update_button(button);
			}
			
			g_controller.rt = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
			g_controller.rt = state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];

			g_controller.l_stick.x = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
			g_controller.l_stick.y = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];

			g_controller.r_stick.x = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
			g_controller.r_stick.y = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
		
			break;
		}
	}

	for (auto& button : g_mouse.buttons | std::views::values) {
		update_button(button);
	}

	g_mouse.delta = window::get_rel_mouse_position() - g_mouse.last_position;
	g_mouse.last_position = window::get_rel_mouse_position();
}

void gse::input::internal::reset_inputs_to_zero() {
	reset_typed_input();

	for (auto& snd : g_keyboard.keys | std::views::values) {
		snd.reset();
	}

	for (auto& snd : g_controller.buttons | std::views::values) {
		snd.reset();
	}

	for (auto& snd : g_mouse.buttons | std::views::values) {
		snd.reset();
	}
}

void gse::input::internal::add_to_typed_input(const char input) {
	g_keyboard.typed_input += input;
}

void gse::input::internal::reset_typed_input() {
	g_keyboard.typed_input.clear();
}
