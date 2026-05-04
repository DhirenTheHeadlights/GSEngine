export module gse.os:input;

import std;
import gse.glfw;

import gse.math;
import gse.meta;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

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
	struct system {
		struct state {
			std::vector<event> queue;
			std::mutex mutex;
			double_buffer<input::state> states;
		};

		static auto update(
			update_context& ctx,
			state& s
		) -> async::task<>;

		static auto key_callback(
			state& s,
			int key,
			int action
		) -> void;

		static auto mouse_button_callback(
			state& s,
			int button,
			int action,
			double x_pos,
			double y_pos
		) -> void;

		static auto mouse_pos_callback(
			state& s,
			double x_pos,
			double y_pos
		) -> void;

		static auto mouse_scroll_callback(
			state& s,
			double x_offset,
			double y_offset
		) -> void;

		static auto text_callback(
			state& s,
			unsigned int codepoint
		) -> void;

		static auto clear_events(
			state& s
		) -> void;

		static auto current_state(
			const state& s
		) -> const input::state&;

	private:
		static auto push_event(
			state& s,
			event&& e
		) -> void;

		static auto to_key(
			int glfw_key
		) -> std::optional<key>;

		static auto to_mouse_button(
			int glfw_button
		) -> std::optional<mouse_button>;
	};
}

auto gse::input::system::push_event(state& s, event&& e) -> void {
	std::scoped_lock lock(s.mutex);
	s.queue.emplace_back(std::move(e));
}

auto gse::input::system::key_callback(state& s, const int key, const int action) -> void {
	if (const auto gse_key = to_key(key)) {
		if (action == glfw::press) {
			push_event(s, key_pressed{ .key_code = *gse_key });
		}
		else if (action == glfw::release) {
			push_event(s, key_released{ .key_code = *gse_key });
		}
	}
}

auto gse::input::system::mouse_button_callback(state& s, const int button, const int action, const double x_pos, const double y_pos) -> void {
	if (const auto gse_button = to_mouse_button(button)) {
		if (action == glfw::press) {
			push_event(s, mouse_button_pressed{ *gse_button, x_pos, y_pos });
		}
		else if (action == glfw::release) {
			push_event(s, mouse_button_released{ *gse_button, x_pos, y_pos });
		}
	}
}

auto gse::input::system::mouse_pos_callback(state& s, const double x_pos, const double y_pos) -> void {
	push_event(s, mouse_moved{ x_pos, y_pos });
}

auto gse::input::system::mouse_scroll_callback(state& s, const double x_offset, const double y_offset) -> void {
	push_event(s, mouse_scrolled{ x_offset, y_offset });
}

auto gse::input::system::text_callback(state& s, const unsigned int codepoint) -> void {
	push_event(s, text_entered{ codepoint });
}

auto gse::input::system::clear_events(state& s) -> void {
	std::scoped_lock lock(s.mutex);
	s.queue.clear();
}

auto gse::input::system::current_state(const state& s) -> const input::state& {
	return s.states.read();
}

auto gse::input::system::to_key(const int glfw_key) -> std::optional<key> {
	if (glfw_key >= glfw::key_space && glfw_key <= glfw::key_last) {
		return static_cast<key>(glfw_key);
	}
	return std::nullopt;
}

auto gse::input::system::to_mouse_button(const int glfw_button) -> std::optional<mouse_button> {
	if (glfw_button >= glfw::mouse_button_1 && glfw_button <= glfw::mouse_button_last) {
		return static_cast<mouse_button>(glfw_button);
	}
	return std::nullopt;
}

auto gse::input::system::update(update_context& ctx, state& s) -> async::task<> {
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
		ctx.channels.push(std::move(e));
	}

	s.states.flip();

	co_return;
}
