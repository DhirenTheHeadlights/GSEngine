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
	struct [[= gse::system_state<"Input">{}]] data {
		[[= gse::shared]] double_buffer<input::state> states;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		channel_read<synthetic_input_request> synthetic_in,
		std::optional<shared_view<window::data>> win
	) -> async::task<>;

	auto current_state(
		shared_view<data> d
	) -> const input::state&;
}

auto gse::input::current_state(const shared_view<data> d) -> const input::state& {
	return d.states.read();
}

auto gse::input::run(gse::context& ctx, data& d, const channel_read<synthetic_input_request> synthetic_in, const std::optional<shared_view<window::data>> win) -> async::task<> {
	const auto& tok = detail::token();
	auto& persistent_state = d.states.write();

	persistent_state.copy_persistent_from(d.states.read());
	persistent_state.begin_frame(tok);

	std::vector<event> drained;
	if (win) {
		drained = win->primary.input_events.drain();
	}

	for (const auto& request : synthetic_in.of<synthetic_input_request>()) {
		drained.push_back(request.value);
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

	return {};
}
