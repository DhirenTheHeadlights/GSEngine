module;

#include <glad/glad.h>
#include <GLFW/glfw3.h>

export module gse.platform.glfw.input;

import std;
import gse.platform.glfw.input.key_defs;
import gse.platform.perma_assert;
import gse.core.main_clock;
import gse.physics.math;

struct callback;

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
	};

	struct keyboard {
		std::unordered_map<control, button> keys;

		std::string typed_input;

		auto reset() -> void {
			for (auto& snd : keys | std::views::values) {
				snd.reset();
			}
		}
	};

	struct mouse {
		std::unordered_map<control, button> buttons;

		unitless::vec2 position;

		auto reset() -> void {
			for (auto& snd : buttons | std::views::values) {
				snd.reset();
			}
		}
	};

	auto update() -> void;
	auto set_up_key_maps(uint32_t id = 0) -> void;

	template <typename... Keys> auto add_callback(std::function<void(uint32_t, control)> func, gse::time cooldown, const int64_t& id, Keys... keys) -> void;
	auto get_callback(const int64_t& id, gse::input::control key) -> callback&;
	auto get_all_keyboards() -> std::unordered_map<uint32_t, keyboard>&;
	auto get_keyboard(uint32_t id = 0) -> keyboard&;
	auto get_controller(uint32_t id = 0) -> controller&;
	auto get_mouse(uint32_t id = 0) -> mouse&;

	auto set_inputs_blocked(bool blocked) -> void;

	namespace internal {
		auto process_event_button(button& button, bool new_state) -> void;
		auto update_button(button& button) -> void;

		auto update_all_buttons(uint32_t id = 0) -> void;
		auto reset_inputs_to_zero(uint32_t id = 0) -> void;

		auto add_to_typed_input(char input, uint32_t id = 0) -> void;
		auto reset_typed_input(uint32_t id = 0) -> void;
	};
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
};

template <typename T>
struct input_hasher {
	std::size_t operator()(T t) const noexcept {
		using key_type = std::underlying_type_t<T>;
		return std::hash<key_type>{}(static_cast<key_type>(t));
	}
};

template <typename T>
struct callback_key_hasher {
	std::size_t operator()(const std::pair<int64_t, T>& p) const noexcept {
		return std::hash<int64_t>{}(p.first) ^ input_hasher<T>{}(p.second);
	}
};

std::unordered_map<std::pair<uint32_t, gse::input::control>, callback, callback_key_hasher<gse::input::control>> g_key_callbacks;
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
	for (int i = static_cast<int>(control::a); i <= static_cast<int>(control::z); i++) {
		g_keyboards[id].keys.insert(std::make_pair(static_cast<control>(i), button()));
	}
	
	for (int i = static_cast<int>(control::num_0); i <= static_cast<int>(control::num_9); i++) {
		g_keyboards[id].keys.insert(std::make_pair(static_cast<control>(i), button()));
	}


	g_keyboards[id].keys.insert(std::make_pair(control::space, button()));
	g_keyboards[id].keys.insert(std::make_pair(control::enter, button()));
	g_keyboards[id].keys.insert(std::make_pair(control::escape, button()));
	g_keyboards[id].keys.insert(std::make_pair(control::up, button()));
	g_keyboards[id].keys.insert(std::make_pair(control::down, button()));
	g_keyboards[id].keys.insert(std::make_pair(control::left, button()));
	g_keyboards[id].keys.insert(std::make_pair(control::right, button()));
	g_keyboards[id].keys.insert(std::make_pair(control::left_control, button()));
	g_keyboards[id].keys.insert(std::make_pair(control::tab, button()));
	g_keyboards[id].keys.insert(std::make_pair(control::left_shift, button()));
	g_keyboards[id].keys.insert(std::make_pair(control::left_alt, button()));


	if (g_controllers.find(id) == g_controllers.end()) g_controllers.insert({ id, controller() });
	for (int i = 0; i <= static_cast<int>(control::gamepad_last); i++) {
		g_controllers[id].buttons.insert(std::make_pair(static_cast<control>(i), button()));
	}

	if (g_mice.find(id) == g_mice.end()) g_mice.insert({ id, mouse() });
	for (int i = 0; i <= static_cast<int>(control::mouse_button_last); i++) {
		g_mice[id].buttons.insert(std::make_pair(static_cast<control>(i), button()));
	}
}


template <typename... Keys>
auto gse::input::add_callback(std::function<void(uint32_t, control)> func, gse::time cooldown, const int64_t& id, Keys... keys) -> void {
	(g_key_callbacks.insert({ std::make_pair(id, keys), { id, keys,  func, cooldown } }), ...);
}

auto gse::input::get_callback(const int64_t& id, gse::input::control key) -> callback& {
	auto it = g_key_callbacks.find(std::make_pair( id, key ));
	perma_assert(it != g_key_callbacks.end(), "Callback does not exist.\n");
	return it->second;
}

auto gse::input::get_all_keyboards() -> std::unordered_map<uint32_t, keyboard>& {
	return g_keyboards;
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
				if (state.buttons[static_cast<int>(b)] == static_cast<int>(control::press)) {
					process_event_button(button, true);
				}
				else if (state.buttons[static_cast<int>(b)] == static_cast<int>(control::release)) {
					process_event_button(button, false);
				}
				update_button(button);
			}

			g_controllers[id].rt = state.axes[static_cast<int>(control::gamepad_axis_right_trigger)];
			g_controllers[id].rt = state.axes[static_cast<int>(control::gamepad_axis_left_trigger)];

			g_controllers[id].l_stick.x = state.axes[static_cast<int>(control::gamepad_axis_left_x)];
			g_controllers[id].l_stick.y = state.axes[static_cast<int>(control::gamepad_axis_left_y)];

			g_controllers[id].r_stick.x = state.axes[static_cast<int>(control::gamepad_axis_right_x)];
			g_controllers[id].r_stick.y = state.axes[static_cast<int>(control::gamepad_axis_right_y)];

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