module;

#include <GLFW/glfw3.h>

export module gse.platform.glfw.input;

import std;
import gse.platform.glfw.input.key_defs;
import gse.platform.perma_assert;
import gse.core.main_clock;
import gse.physics.math;

export namespace gse::input {
	using client_id = uint32_t;
	bool client_mode = false;
}

namespace gse::input {
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
}

namespace gse::input::keyboard {
	std::unordered_map<control, button> keys;
	std::string typed_input;

	auto reset() -> void {
		for (auto& snd : keys | std::views::values) {
			snd.reset();
		}
	}

	auto key(const control key) -> button& {
		perma_assert(keys.contains(key), "Keyboard key not found in keyboard keys map");
		return keys[key];
	}
}

namespace gse::input::mouse {
	std::unordered_map<control, button> buttons;
	unitless::vec2 position;

	auto reset() -> void {
		for (auto& snd : buttons | std::views::values) {
			snd.reset();
		}
	}

	auto button(const control key) -> button& {
		perma_assert(buttons.contains(key), "Mouse button not found in mouse buttons map");
		return buttons[key];
	}
}

namespace gse::input::controller {
	std::unordered_map<control, button> buttons;

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

	auto button(const control key) -> button& {
		perma_assert(buttons.contains(key), "Controller button not found in controller buttons map");
		return buttons[key];
	}
}

struct callback {
	int64_t id;
	gse::input::control trigger;
	std::function<void(std::int64_t, gse::input::control)> func;
	gse::time cooldown = 0;
	gse::time last_triggered = gse::main_clock::get_current_time();

	auto use() -> void {
		if (gse::main_clock::get_current_time() - last_triggered < cooldown) return;
		last_triggered = gse::main_clock::get_current_time();
		func(id, trigger);
	}

	struct handle {
		int64_t id;
		gse::input::control trigger;
		gse::time cooldown = 0;
		gse::time last_triggered = gse::main_clock::get_current_time();
	};

	explicit operator handle() const {
		return handle{ id, trigger, cooldown, last_triggered };
	}
};

export namespace gse::input {
	auto initialize(uint32_t id = 0) -> void;
	auto update() -> void;

	template <auto... Keys>
	auto add_callback(const std::function<void(uint32_t, control)>& func, time cooldown, const int64_t& id) -> void;

	auto validate_callback(const callback::handle& cb, const callback::handle& validator) -> bool {
		if (cb.id != validator.id || cb.trigger != validator.trigger) {
			return false;
		}

		const auto now = main_clock::get_current_time();
		if (now - validator.last_triggered < validator.cooldown) {
			return false;
		}

		return true;
	}

	namespace internal {
		auto process_event_button(button& button, bool new_state) -> void;
		auto update_button(button& button) -> void;

		auto update_all_buttons(uint32_t id = 0) -> void;
		auto reset_inputs_to_zero(uint32_t id = 0) -> void;

		auto add_to_typed_input(char input, uint32_t id = 0) -> void;
		auto reset_typed_input(uint32_t id = 0) -> void;
	};
}

std::vector<callback> g_key_callbacks;

auto gse::input::update() -> void {
	internal::update_all_buttons();
	internal::reset_typed_input();

	for (auto& cb : g_key_callbacks) {
		if (cb.cooldown > seconds(0)) {
			cb.use();
		}
	}
}

bool g_block_inputs = false;

auto gse::input::initialize(uint32_t id) -> void {
	auto fill_map = [](std::unordered_map<control, button>& map, const control first, const control last) {
		for (auto i = static_cast<int>(first); i <= static_cast<int>(last); ++i) {
			auto key = static_cast<control>(i);
			map.insert(std::make_pair(key, button()));
		}
		};

	fill_map(keyboard::keys, control::space, control::last);
	fill_map(mouse::buttons, control::mouse_button_1, control::mouse_button_last);
	fill_map(controller::buttons, control::gamepad_a, control::gamepad_last);
}

template <auto... Keys>
auto gse::input::add_callback(const std::function<void(uint32_t, control)>& func, const time cooldown, const int64_t& id) -> void {
	(g_key_callbacks.emplace_back(callback{ id, Keys, func, cooldown }), ...);
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

auto gse::input::internal::update_all_buttons(const uint32_t id) -> void {
	if (g_block_inputs) return;


	for (auto& button : keyboard::keys | std::views::values) {
		update_button(button);
	}

	for (int i = 0; i <= static_cast<int>(control::gamepad_last); ++i) {
		if (!(glfwJoystickPresent(i) && glfwJoystickIsGamepad(i))) continue;

		GLFWgamepadstate state;

		if (glfwGetGamepadState(i, &state)) {
			for (auto& [b, button] : controller::buttons) {
				if (state.buttons[static_cast<int>(b)] == static_cast<int>(control::press)) {
					process_event_button(button, true);
				}
				else if (state.buttons[static_cast<int>(b)] == static_cast<int>(control::release)) {
					process_event_button(button, false);
				}
				update_button(button);
			}

			controller::rt = state.axes[static_cast<int>(control::gamepad_axis_right_trigger)];
			controller::rt = state.axes[static_cast<int>(control::gamepad_axis_left_trigger)];

			controller::l_stick.x = state.axes[static_cast<int>(control::gamepad_axis_left_x)];
			controller::l_stick.y = state.axes[static_cast<int>(control::gamepad_axis_left_y)];

			controller::r_stick.x = state.axes[static_cast<int>(control::gamepad_axis_right_x)];
			controller::r_stick.y = state.axes[static_cast<int>(control::gamepad_axis_right_y)];

			break;
		}
	}

	for (auto& button : mouse::buttons | std::views::values) {
		update_button(button);
	}
}

auto gse::input::internal::reset_inputs_to_zero(const uint32_t id) -> void {
	reset_typed_input(id);

	for (auto& snd : keyboard::keys | std::views::values) {
		snd.reset();
	}

	for (auto& snd : controller::buttons | std::views::values) {
		snd.reset();
	}

	for (auto& snd : mouse::buttons | std::views::values) {
		snd.reset();
	}
}

auto gse::input::internal::add_to_typed_input(const char input, const uint32_t id) -> void {
	keyboard::typed_input += input;
}

auto gse::input::internal::reset_typed_input(const uint32_t id) -> void {
	keyboard::typed_input.clear();
}