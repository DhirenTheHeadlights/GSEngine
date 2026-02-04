module;

#include <GLFW/glfw3.h>

export module gse.platform:input;

import std;

import gse.physics.math;
import gse.utility;

import :keys;
import :input_state;

export namespace gse::input {
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
}

namespace gse::detail {
	struct input_state_token {
		input_state_token() = default;
	};

	auto token() -> const input_state_token& {
		static constexpr input_state_token t{};
		return t;
	}
}

export namespace gse::input {
	class system_state {
	public:
		std::vector<event> queue;
		std::mutex mutex;
		double_buffer<state> states;

		auto key_callback(int key, int action) -> void;
		auto mouse_button_callback(int button, int action, double x_pos, double y_pos) -> void;
		auto mouse_pos_callback(double x_pos, double y_pos) -> void;
		auto mouse_scroll_callback(double x_offset, double y_offset) -> void;
		auto text_callback(unsigned int codepoint) -> void;
		auto clear_events() -> void;
		auto current_state() const -> const state&;

	private:
		auto push_event(event&& e) -> void;

		static auto to_key(int glfw_key) -> std::optional<key>;
		static auto to_mouse_button(int glfw_button) -> std::optional<mouse_button>;
	};

	struct system {
		static auto update(update_phase& phase, system_state& s) -> void;
		static auto end_frame(end_frame_phase& phase, system_state& s) -> void;
	};
}

auto gse::input::system_state::push_event(event&& e) -> void {
	std::scoped_lock lock(mutex);
	queue.emplace_back(std::move(e));
}

auto gse::input::system_state::key_callback(const int key, const int action) -> void {
	if (const auto gse_key = to_key(key)) {
		if (action == GLFW_PRESS) {
			push_event(key_pressed{ .key_code = *gse_key });
		}
		else if (action == GLFW_RELEASE) {
			push_event(key_released{ .key_code = *gse_key });
		}
	}
}

auto gse::input::system_state::mouse_button_callback(const int button, const int action, const double x_pos, const double y_pos) -> void {
	if (const auto gse_button = to_mouse_button(button)) {
		if (action == GLFW_PRESS) {
			push_event(mouse_button_pressed{ *gse_button, x_pos, y_pos });
		}
		else if (action == GLFW_RELEASE) {
			push_event(mouse_button_released{ *gse_button, x_pos, y_pos });
		}
	}
}

auto gse::input::system_state::mouse_pos_callback(const double x_pos, const double y_pos) -> void {
	push_event(mouse_moved{ x_pos, y_pos });
}

auto gse::input::system_state::mouse_scroll_callback(const double x_offset, const double y_offset) -> void {
	push_event(mouse_scrolled{ x_offset, y_offset });
}

auto gse::input::system_state::text_callback(const unsigned int codepoint) -> void {
	push_event(text_entered{ codepoint });
}

auto gse::input::system_state::clear_events() -> void {
	std::scoped_lock lock(mutex);
	queue.clear();
}

auto gse::input::system_state::current_state() const -> const state& {
	return states.read();
}

auto gse::input::system_state::to_key(const int glfw_key) -> std::optional<key> {
	if (glfw_key >= GLFW_KEY_SPACE && glfw_key <= GLFW_KEY_LAST) {
		return static_cast<key>(glfw_key);
	}
	return std::nullopt;
}

auto gse::input::system_state::to_mouse_button(const int glfw_button) -> std::optional<mouse_button> {
	if (glfw_button >= GLFW_MOUSE_BUTTON_1 && glfw_button <= GLFW_MOUSE_BUTTON_LAST) {
		return static_cast<mouse_button>(glfw_button);
	}
	return std::nullopt;
}

auto gse::input::system::update(update_phase& phase, system_state& s) -> void {
	std::vector<event> events_to_process;

	{
		std::scoped_lock lock(s.mutex);
		events_to_process.swap(s.queue);
	}

	const auto& tok = detail::token();
	auto& persistent_state = s.states.write();

	persistent_state.copy_persistent_from(s.states.read());
	persistent_state.begin_frame(tok);

	for (const auto& evt : events_to_process) {
		match(evt)
			.if_is([&](const key_pressed& arg) {
				persistent_state.on_key_pressed(arg.key_code, tok);
			})
			.else_if_is([&](const key_released& arg) {
				persistent_state.on_key_released(arg.key_code, tok);
			})
			.else_if_is([&](const mouse_button_pressed& arg) {
				persistent_state.on_mouse_button_pressed(arg.button, tok);
			})
			.else_if_is([&](const mouse_button_released& arg) {
				persistent_state.on_mouse_button_released(arg.button, tok);
			})
			.else_if_is([&](const mouse_moved& arg) {
				persistent_state.on_mouse_moved(static_cast<float>(arg.x_pos), static_cast<float>(arg.y_pos), tok);
			})
			.else_if_is([&](const mouse_scrolled& arg) {
				persistent_state.on_scroll(static_cast<float>(arg.x_offset), static_cast<float>(arg.y_offset), tok);
			})
			.else_if_is([&](const text_entered& arg) {
				persistent_state.append_codepoint(arg.codepoint, tok);
			});
	}

	persistent_state.end_frame(tok);

	for (auto& e : events_to_process) {
		phase.channels.push(std::move(e));
	}
}

auto gse::input::system::end_frame(end_frame_phase&, system_state& s) -> void {
	s.states.flip();
}
