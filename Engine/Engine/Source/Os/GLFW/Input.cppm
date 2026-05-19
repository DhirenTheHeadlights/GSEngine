export module gse.os:input;

import std;

import gse.math;
import gse.meta;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.log;

import :keys;
import :input_state;
import :input_events;
import :window;

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
		struct data {
			double_buffer<input::state> states;
		};

		static auto run(run_context& ctx, data& d, const window::data* win) -> async::task<>;

		static auto current_state(const data& d) -> const input::state&;
	};
}

auto gse::input::system::current_state(const data& d) -> const input::state& {
	return d.states.read();
}

auto gse::input::system::run(run_context& ctx, data& d, const window::data* win) -> async::task<> {
	while (true) {
		const auto& tok = detail::token();
		auto& persistent_state = d.states.write();

		persistent_state.copy_persistent_from(d.states.read());
		persistent_state.begin_frame(tok);

		std::vector<event> drained;
		if (win) {
			drained = win->input_events.drain();
		}

		for (const auto& evt : drained) {
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

		d.states.flip();

		co_await ctx.next_tick();
	}
}
