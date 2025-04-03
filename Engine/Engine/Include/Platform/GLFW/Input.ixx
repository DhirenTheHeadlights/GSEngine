module;

#include <glad/glad.h>
#include <GLFW/glfw3.h>

export module gse.platform.glfw.input;

import std;

import gse.core.main_clock;
import gse.physics.math;

export namespace gse::input {
	struct button {
		std::uint8_t pressed = 0;
		std::uint8_t held = 0;
		std::uint8_t released = 0;
		std::int8_t new_state = -1;
		std::uint8_t typed = 0;
		float typed_time = 0.0f;
		bool toggled = false;

		auto merge(const button& b) -> void {
			this->pressed |= b.pressed;
			this->released |= b.released;
			this->held |= b.held;
		}

		auto reset() -> void {
			pressed = 0;
			held = 0;
			released = 0;
		}
	};

	struct controller {
		std::unordered_map<int, button> buttons;

		float lt = 0.f;
		float rt = 0.f;

		struct {
			float x = 0.f, y = 0.f;
		} l_stick, r_stick;

		auto reset() -> void {
			for (auto& snd : buttons | std::views::values) {
				snd.reset();
			}

			lt = 0.f;
			rt = 0.f;
			l_stick.x = 0.f;
			l_stick.y = 0.f;
		}
	};

	struct keyboard {
		std::unordered_map<int, button> keys;

		std::string typed_input;

		auto reset() -> void {
			for (auto& snd : keys | std::views::values) {
				snd.reset();
			}
		}
	};

	struct mouse {
		std::unordered_map<int, button> buttons;

		unitless::vec2 position;

		auto reset() -> void {
			for (auto& snd : buttons | std::views::values) {
				snd.reset();
			}
		}
	};

	auto update() -> void;
	auto set_up_key_maps(uint32_t id) -> void;

	auto get_keyboard(uint32_t id) -> keyboard&;
	auto get_controller(uint32_t id) -> controller&;
	auto get_mouse(uint32_t id) -> mouse&;

	auto set_inputs_blocked(bool blocked) -> void;

	namespace internal {
		auto process_event_button(button& button, bool new_state) -> void;
		auto update_button(button& button) -> void;

		auto update_all_buttons(uint32_t id) -> void;
		auto reset_inputs_to_zero(uint32_t id) -> void;

		auto add_to_typed_input(char input, uint32_t id) -> void;
		auto reset_typed_input(uint32_t id) -> void;
	};
}

std::unordered_map<uint32_t, gse::input::keyboard> g_keyboards = { { 0,  gse::input::keyboard() } };
std::unordered_map<uint32_t, gse::input::controller> g_controllers = { { 0, gse::input::controller() } };
std::unordered_map<uint32_t, gse::input::mouse> g_mice = { { 0, gse::input::mouse() } };

auto gse::input::update() -> void {
	for (auto& [id, input] : g_keyboards) {;
		internal::update_all_buttons(id);
		internal::reset_typed_input(id);
	}
}

bool g_block_inputs = false;

auto gse::input::set_up_key_maps(uint32_t id) -> void {

	if (g_keyboards.find(id) == g_keyboards.end()) g_keyboards.insert({ id, keyboard() });
	for (int i = GLFW_KEY_A; i <= GLFW_KEY_Z; i++) {
		g_keyboards[id].keys.insert(std::make_pair(i, button()));
	}

	for (int i = GLFW_KEY_0; i <= GLFW_KEY_9; i++) {
		g_keyboards[id].keys.insert(std::make_pair(i, button()));
	}

	g_keyboards[id].keys.insert(std::make_pair(GLFW_KEY_SPACE, button()));
	g_keyboards[id].keys.insert(std::make_pair(GLFW_KEY_ENTER, button()));
	g_keyboards[id].keys.insert(std::make_pair(GLFW_KEY_ESCAPE, button()));
	g_keyboards[id].keys.insert(std::make_pair(GLFW_KEY_UP, button()));
	g_keyboards[id].keys.insert(std::make_pair(GLFW_KEY_DOWN, button()));
	g_keyboards[id].keys.insert(std::make_pair(GLFW_KEY_LEFT, button()));
	g_keyboards[id].keys.insert(std::make_pair(GLFW_KEY_RIGHT, button()));
	g_keyboards[id].keys.insert(std::make_pair(GLFW_KEY_LEFT_CONTROL, button()));
	g_keyboards[id].keys.insert(std::make_pair(GLFW_KEY_TAB, button()));
	g_keyboards[id].keys.insert(std::make_pair(GLFW_KEY_LEFT_SHIFT, button()));
	g_keyboards[id].keys.insert(std::make_pair(GLFW_KEY_LEFT_ALT, button()));


	if (g_controllers.find(id) == g_controllers.end()) g_controllers.insert({ id, controller() });
	for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; i++) {
		g_controllers[id].buttons.insert(std::make_pair(i, button()));
	}

	if (g_mice.find(id) == g_mice.end()) g_mice.insert({ id, mouse() });
	for (int i = 0; i <= GLFW_MOUSE_BUTTON_LAST; i++) {
		g_mice[id].buttons.insert(std::make_pair(i, button()));
	}
}

auto gse::input::get_keyboard(uint32_t id) -> keyboard& {
	return g_keyboards[id];
}

auto gse::input::get_controller(uint32_t id) -> controller& {
	return g_controllers[id];
}

auto gse::input::get_mouse(uint32_t id) -> mouse& {
	return g_mice[id];
}

auto gse::input::set_inputs_blocked(const bool blocked) -> void {
	g_block_inputs = blocked;
}

auto gse::input::internal::process_event_button(button& button, const bool new_state) -> void {
	button.new_state = static_cast<int8_t>(new_state);
}

auto gse::input::internal::update_button(button& button) -> void {
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

auto gse::input::internal::update_all_buttons(uint32_t id) -> void {
	if (g_block_inputs) return;


	for (auto& button : g_keyboards[id].keys | std::views::values) {
		update_button(button);
	}

	for (int i = 0; i <= static_cast<int>(g_controllers[id].buttons.size()); i++) {
		if (!(glfwJoystickPresent(i) && glfwJoystickIsGamepad(i))) continue;

		GLFWgamepadstate state;

		if (glfwGetGamepadState(i, &state)) {
			for (auto& [b, button] : g_controllers[id].buttons) {
				if (state.buttons[b] == GLFW_PRESS) {
					process_event_button(button, true);
				}
				else if (state.buttons[b] == GLFW_RELEASE) {
					process_event_button(button, false);
				}
				update_button(button);
			}

			g_controllers[id].rt = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
			g_controllers[id].rt = state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];

			g_controllers[id].l_stick.x = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
			g_controllers[id].l_stick.y = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];

			g_controllers[id].r_stick.x = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
			g_controllers[id].r_stick.y = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];

			break;
		}
	}

	for (auto& button : g_mice[id].buttons | std::views::values) {
		update_button(button);
	}
}

auto gse::input::internal::reset_inputs_to_zero(uint32_t id) -> void {
	reset_typed_input(id);

	for (auto& snd : g_keyboards[id].keys | std::views::values) {
		snd.reset();
	}

	for (auto& snd : g_controllers[id].buttons | std::views::values) {
		snd.reset();
	}

	for (auto& snd : g_mice[id].buttons | std::views::values) {
		snd.reset();
	}
}

auto gse::input::internal::add_to_typed_input(const char input, uint32_t id) -> void {
	g_keyboards[id].typed_input += input;
}

auto gse::input::internal::reset_typed_input(uint32_t id) -> void {
	g_keyboards[id].typed_input.clear();
}