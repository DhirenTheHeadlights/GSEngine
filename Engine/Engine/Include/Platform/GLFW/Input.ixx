module;

#include <GLFW/glfw3.h>

export module gse.platform:input;

import std;

import gse.physics.math;

import :keys;

namespace gse::input {
	struct key_pressed {
		key key_code;
	};

	struct key_released {
		key key_code;
	};

	struct mouse_button_pressed {
		mouse_button button;
		double x_pos;
		double y_pos;
	};

	struct mouse_button_released {
		mouse_button button;
		double x_pos;
		double y_pos;
	};

	struct mouse_moved {
		double x_pos;
		double y_pos;
	};

	struct mouse_scrolled {
		double x_offset;
		double y_offset;
	};

	struct text_entered {
		std::uint32_t codepoint;
	};

	using event = std::variant<
		key_pressed,
		key_released,
		mouse_button_pressed,
		mouse_button_released,
		mouse_moved,
		mouse_scrolled,
		text_entered
	>;

	class input_state {
	public:
		auto key_pressed(const key key) const -> bool {
			return m_keys_pressed_this_frame.contains(key);
		}

		auto key_held(const key key) const -> bool {
			return m_keys_held.contains(key);
		}

		auto key_released(const key key) const -> bool {
			return m_keys_released_this_frame.contains(key);
		}

		auto mouse_button_pressed(const mouse_button button) const -> bool {
			return m_mouse_buttons_pressed_this_frame.contains(button);
		}

		auto mouse_button_held(const mouse_button button) const -> bool {
			return m_mouse_buttons_held.contains(button);
		}

		auto mouse_button_released(const mouse_button button) const -> bool {
			return m_mouse_buttons_released_this_frame.contains(button);
		}

		auto mouse_position() const -> unitless::vec2 {
			return m_mouse_position;
		}

		auto mouse_delta() const -> unitless::vec2 {
			return m_mouse_delta;
		}

		auto text_entered() const -> const std::string& {
			return m_text_entered_this_frame;
		}

	private:
		friend auto update(const std::function<void()>& in_frame) -> void;

		std::unordered_set<key> m_keys_held;
		std::unordered_set<key> m_keys_pressed_this_frame;
		std::unordered_set<key> m_keys_released_this_frame;

		std::unordered_set<mouse_button> m_mouse_buttons_held;
		std::unordered_set<mouse_button> m_mouse_buttons_pressed_this_frame;
		std::unordered_set<mouse_button> m_mouse_buttons_released_this_frame;

		unitless::vec2 m_mouse_position;
		unitless::vec2 m_mouse_delta;
		std::string m_text_entered_this_frame;
	};

	export auto update(const std::function<void()>& in_frame) -> void;

	auto key_callback(int key, int action) -> void;
	auto mouse_button_callback(int button, int action, double x_pos, double y_pos) -> void;
	auto mouse_pos_callback(double x_pos, double y_pos) -> void;
	auto mouse_scroll_callback(double x_offset, double y_offset) -> void;
	auto text_callback(unsigned int codepoint) -> void;
	auto clear_events() -> void;

	thread_local const input_state* global_input_state = nullptr;
}

namespace gse::input {
	std::vector<event> queue;
	std::mutex mutex;

	auto to_key(int glfw_key) -> std::optional<key> {
		if (glfw_key >= GLFW_KEY_SPACE && glfw_key <= GLFW_KEY_LAST) {
			return static_cast<key>(glfw_key);
		}
		return std::nullopt;
	}

	auto to_mouse_button(int glfw_button) -> std::optional<mouse_button> {
		if (glfw_button >= GLFW_MOUSE_BUTTON_1 && glfw_button <= GLFW_MOUSE_BUTTON_LAST) {
			return static_cast<mouse_button>(glfw_button);
		}
		return std::nullopt;
	}

	auto codepoint_to_utf8(std::string& s, const std::uint32_t c) -> void {
		if (c < 0x80) {
			s += static_cast<char>(c);
		}
		else if (c < 0x800) {
			s += static_cast<char>(0xC0 | (c >> 6));
			s += static_cast<char>(0x80 | (c & 0x3F));
		}
		else if (c < 0x10000) {
			s += static_cast<char>(0xE0 | (c >> 12));
			s += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
			s += static_cast<char>(0x80 | (c & 0x3F));
		}
		else {
			s += static_cast<char>(0xF0 | (c >> 18));
			s += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
			s += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
			s += static_cast<char>(0x80 | (c & 0x3F));
		}
	}
}

auto gse::input::key_callback(const int key, const int action) -> void {
	if (const auto gse_key = to_key(key)) {
		std::scoped_lock lock(mutex);
		if (action == GLFW_PRESS) {
			queue.emplace_back(key_pressed{ .key_code = *gse_key });
		}
		else if (action == GLFW_RELEASE) {
			queue.emplace_back(key_released{ .key_code = *gse_key });
		}
	}
}

auto gse::input::mouse_button_callback(const int button, const int action, const double x_pos, const double y_pos) -> void {
	if (const auto gse_button = to_mouse_button(button)) {
		std::scoped_lock lock(mutex);
		if (action == GLFW_PRESS) {
			queue.emplace_back(mouse_button_pressed{ *gse_button, x_pos, y_pos });
		}
		else if (action == GLFW_RELEASE) {
			queue.emplace_back(mouse_button_released{ *gse_button, x_pos, y_pos });
		}
	}
}

auto gse::input::mouse_pos_callback(const double x_pos, const double y_pos) -> void {
	std::scoped_lock lock(mutex);
	queue.emplace_back(mouse_moved{ x_pos, y_pos });
}

auto gse::input::mouse_scroll_callback(const double x_offset, const double y_offset) -> void {
	std::scoped_lock lock(mutex);
	queue.emplace_back(mouse_scrolled{ x_offset, y_offset });
}

auto gse::input::text_callback(const unsigned int codepoint) -> void {
	std::scoped_lock lock(mutex);
	queue.emplace_back(text_entered{ codepoint });
}

auto gse::input::clear_events() -> void {
	std::scoped_lock lock(mutex);
	queue.clear();
}

auto gse::input::update(const std::function<void()>& in_frame) -> void {
	static input_state persistent_state;

	std::vector<event> events_to_process;
	{
		std::scoped_lock lock(mutex);
		events_to_process.swap(queue);
	}

	persistent_state.m_keys_pressed_this_frame.clear();
	persistent_state.m_keys_released_this_frame.clear();
	persistent_state.m_mouse_buttons_pressed_this_frame.clear();
	persistent_state.m_mouse_buttons_released_this_frame.clear();
	persistent_state.m_text_entered_this_frame.clear();

	const unitless::vec2 last_mouse_pos = persistent_state.m_mouse_position;

	for (const auto& evt : events_to_process) {
		std::visit(
			[&]<typename T0>(T0 && arg) {
			using t = std::decay_t<T0>;
			if constexpr (std::is_same_v<t, key_pressed>) {
				persistent_state.m_keys_pressed_this_frame.insert(arg.key_code);
				persistent_state.m_keys_held.insert(arg.key_code);
			}
			else if constexpr (std::is_same_v<t, key_released>) {
				persistent_state.m_keys_released_this_frame.insert(arg.key_code);
				persistent_state.m_keys_held.erase(arg.key_code);
			}
			else if constexpr (std::is_same_v<t, mouse_button_pressed>) {
				persistent_state.m_mouse_buttons_pressed_this_frame.insert(arg.button);
				persistent_state.m_mouse_buttons_held.insert(arg.button);
			}
			else if constexpr (std::is_same_v<t, mouse_button_released>) {
				persistent_state.m_mouse_buttons_released_this_frame.insert(arg.button);
				persistent_state.m_mouse_buttons_held.erase(arg.button);
			}
			else if constexpr (std::is_same_v<t, mouse_moved>) {
				persistent_state.m_mouse_position = { static_cast<float>(arg.x_pos), static_cast<float>(arg.y_pos) };
			}
			else if constexpr (std::is_same_v<t, text_entered>) {
				codepoint_to_utf8(persistent_state.m_text_entered_this_frame, arg.codepoint);
			}
		},
			evt
		);
	}

	persistent_state.m_mouse_delta = persistent_state.m_mouse_position - last_mouse_pos;

	global_input_state = &persistent_state;
	in_frame();
	global_input_state = nullptr;
}

export namespace gse::keyboard {
	auto pressed(key key) -> bool;
	auto released(key key) -> bool;
	auto held(key key) -> bool;
}

export namespace gse::mouse {
	auto pressed(mouse_button button) -> bool;
	auto released(mouse_button button) -> bool;
	auto held(mouse_button button) -> bool;
	auto position() -> unitless::vec2;
	auto delta() -> unitless::vec2;
}

auto gse::keyboard::pressed(const key key) -> bool {
	return input::global_input_state && input::global_input_state->key_pressed(key);
}

auto gse::keyboard::released(const key key) -> bool {
	return input::global_input_state && input::global_input_state->key_released(key);
}

auto gse::keyboard::held(const key key) -> bool {
	return input::global_input_state && input::global_input_state->key_held(key);
}

auto gse::mouse::pressed(const mouse_button button) -> bool {
	return input::global_input_state && input::global_input_state->mouse_button_pressed(button);
}

auto gse::mouse::released(const mouse_button button) -> bool {
	return input::global_input_state && input::global_input_state->mouse_button_released(button);
}

auto gse::mouse::held(const mouse_button button) -> bool {
	return input::global_input_state && input::global_input_state->mouse_button_held(button);
}

auto gse::mouse::position() -> unitless::vec2 {
	if (input::global_input_state) {
		return input::global_input_state->mouse_position();
	}
	return {};
}

auto gse::mouse::delta() -> unitless::vec2 {
	if (input::global_input_state) {
		return input::global_input_state->mouse_delta();
	}
	return {};
}