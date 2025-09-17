module;

#include <GLFW/glfw3.h>

export module gse.platform:input;

import std;

import gse.physics.math;
import gse.utility;

import :keys;
import :input_state;

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

	export auto update(
		const std::function<void()>& in_frame
	) -> void;

	auto key_callback(
		int key,
		int action
	) -> void;

	auto mouse_button_callback(
		int button,
		int action,
		double x_pos,
		double y_pos
	) -> void;

	auto mouse_pos_callback(
		double x_pos,
		double y_pos
	) -> void;

	auto mouse_scroll_callback(
		double x_offset,
		double y_offset
	) -> void;

	auto text_callback(
		unsigned int codepoint
	) -> void;

	auto clear_events(
	) -> void;

	thread_local const input_state* global_input_state = nullptr;
}

namespace gse::input {
	std::vector<event> queue;
	std::mutex mutex;

	auto to_key(
		int glfw_key
	) -> std::optional<key>;

	auto to_mouse_button(
		int glfw_button
	) -> std::optional<mouse_button>;
}

namespace gse::detail {
	struct input_state_token {
		input_state_token() = default;
	};

	auto token(
	) -> const input_state_token& {
		static constexpr input_state_token t{};
		return t;
	}
}

auto gse::input::update(const std::function<void()>& in_frame) -> void {
	static input_state persistent_state;

	std::vector<event> events_to_process;
	scope([&] {
		std::scoped_lock lock(mutex);
		events_to_process.swap(queue);
	});

	const auto& tok = detail::token();
	persistent_state.begin_frame(tok);

	for (const auto& evt : events_to_process) {
		match(evt)
			.if_is<key_pressed>([&](const auto& arg) {
				persistent_state.on_key_pressed(arg.key_code, tok);
			})
			.else_if_is<key_released>([&](const auto& arg) {
				persistent_state.on_key_released(arg.key_code, tok);
			})
			.else_if_is<mouse_button_pressed>([&](const auto& arg) {
				persistent_state.on_mouse_button_pressed(arg.button, tok);
			})
			.else_if_is<mouse_button_released>([&](const auto& arg) {
				persistent_state.on_mouse_button_released(arg.button, tok);
			})
			.else_if_is<mouse_moved>([&](const auto& arg) {
				persistent_state.on_mouse_moved(static_cast<float>(arg.x_pos), static_cast<float>(arg.y_pos), tok);
			})
			.else_if_is<mouse_scrolled>([&](const auto&) {
				// no state tracked for scroll yet
			})
			.else_if_is<text_entered>([&](const auto& arg) {
				persistent_state.append_codepoint(arg.codepoint, tok);
			});
	}

	persistent_state.end_frame(tok);

	global_input_state = &persistent_state;
	in_frame();
	global_input_state = nullptr;
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

export namespace gse::keyboard {
	auto pressed(
		key key
	) -> bool;

	auto released(
		key key
	) -> bool;

	auto held(
		key key
	) -> bool;
}

export namespace gse::mouse {
	auto pressed(
		mouse_button button
	) -> bool;

	auto released(
		mouse_button button
	) -> bool;

	auto held(
		mouse_button button
	) -> bool;

	auto position(
	) -> unitless::vec2;

	auto delta(
	) -> unitless::vec2;
}

export namespace gse::text {
	auto entered(
	) -> const std::string&;
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

auto gse::text::entered() -> const std::string& {
	if (input::global_input_state) {
		return input::global_input_state->text_entered();
	}
	static const std::string empty_string;
	return empty_string;
}

auto gse::input::to_key(const int glfw_key) -> std::optional<key> {
	if (glfw_key >= GLFW_KEY_SPACE && glfw_key <= GLFW_KEY_LAST) {
		return static_cast<key>(glfw_key);
	}
	return std::nullopt;
}

auto gse::input::to_mouse_button(const int glfw_button) -> std::optional<mouse_button> {
	if (glfw_button >= GLFW_MOUSE_BUTTON_1 && glfw_button <= GLFW_MOUSE_BUTTON_LAST) {
		return static_cast<mouse_button>(glfw_button);
	}
	return std::nullopt;
}
