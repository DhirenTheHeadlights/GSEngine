export module gse.graphics:input_widget;

import std;

import :types;
import :text_renderer;
import :ids;

export namespace gse::gui::draw {
	auto input(
		const widget_context& context,
		const std::string& name,
		std::string& buffer,
		id& hot_widget_id,
		id& active_widget_id
	) -> void;
}

namespace gse::gui::draw {
	struct input_state {
		int caret = 0;
		int anchor = 0;
		float scroll_x = 0.f;
		double blink_ms = 0.0;
		bool blink_on = true;
		bool rpt_active = false;
		double rpt_next_ms = 0.0;
	};

	std::unordered_map<uint64_t, input_state> global_input_state;
}

auto gse::gui::draw::input(const widget_context& context, const std::string& name, std::string& buffer, id& hot_widget_id, id& active_widget_id) -> void {
	if (!context.current_menu) return;

	const std::string id_label = name + "##Input";
	const auto widget_id = ids::make(id_label);

	auto clamp = [](const int v, const int lo, const int hi) -> int {
		return v < lo ? lo : v > hi ? hi : v;
	};

	auto& state = global_input_state[widget_id.number()];
	state.caret = clamp(state.caret, 0, static_cast<int>(buffer.size()));
	state.anchor = clamp(state.anchor, 0, static_cast<int>(buffer.size()));

	const float widget_height = context.font->line_height(context.style.font_size) + context.style.padding * 0.5f;
	const auto content_rect = context.current_menu->rect.inset({ context.style.padding, context.style.padding });

	const ui_rect row_rect = ui_rect::from_position_size(
		{ content_rect.left(), context.layout_cursor.y() },
		{ content_rect.width(), widget_height }
	);

	const float label_width = content_rect.width() * 0.4f;
	const ui_rect box_rect = ui_rect::from_position_size(
		{ row_rect.left() + label_width, row_rect.top() },
		{ content_rect.width() - label_width, widget_height }
	);

	const bool hover = box_rect.contains(mouse::position());
	if (hover) {
		hot_widget_id = widget_id;
	}

	const bool focused = active_widget_id == widget_id;

	auto pick_index_from_x = [&](const float x_local) -> int {
		const int n = static_cast<int>(buffer.size());
		float best_dx = std::numeric_limits<float>::max();
		int best_k = 0;
		for (int k = 0; k <= n; ++k) {
			const float w = context.font->width(buffer.substr(0, k), context.style.font_size);
			if (const float dx = std::abs(w - state.scroll_x - x_local); dx < best_dx) {
				best_dx = dx; best_k = k;
			}
		}
		return best_k;
	};

	auto now_ms = [] -> double {
		using namespace std::chrono;
		return duration_cast<std::chrono::milliseconds>(steady_clock::now().time_since_epoch()).count();
	};

	if (hover && mouse::pressed(mouse_button::button_1)) {
		const bool shift = keyboard::pressed(key::left_shift) || keyboard::pressed(key::right_shift);
		const float x_local = mouse::position().x() - box_rect.left();
		int i = pick_index_from_x(x_local);

		if (shift) {
			state.caret = clamp(i, 0, static_cast<int>(buffer.size()));
		}
		else {
			state.caret = state.anchor = clamp(i, 0, static_cast<int>(buffer.size()));
		}

		state.blink_ms = now_ms();
		state.blink_on = true;
	}

	auto has_sel = [](const input_state& s) -> bool {
		return s.caret != s.anchor;
	};

	auto sel_range = [](const input_state& s) -> std::pair<int, int> {
		return s.caret < s.anchor ? std::pair{ s.caret, s.anchor } : std::pair{ s.anchor, s.caret };
	};

	if (focused) {
		if (const std::string entered = text::entered(); !entered.empty()) {
			if (has_sel(state)) {
				auto [a, b] = sel_range(state);
				buffer.erase(a, b - a);
				state.caret = state.anchor = a;
			}

			buffer.insert(state.caret, entered);

			state.caret += static_cast<int>(entered.size());
			state.anchor = state.caret;
			state.blink_ms = now_ms();
			state.blink_on = true;
		}

		const bool shift = keyboard::pressed(key::left_shift) || keyboard::pressed(key::right_shift);
		const bool ctrl = keyboard::pressed(key::left_control) || keyboard::pressed(key::right_control);

		auto move_caret = [&](int new_i) {
			new_i = clamp(new_i, 0, static_cast<int>(buffer.size()));
			if (shift) state.caret = new_i; else state.caret = state.anchor = new_i;
			state.blink_ms = now_ms(); state.blink_on = true;
		};

		auto word_left = [](const std::string_view t, int i) -> int {
			i = std::max(i, 0);
			while (i > 0 && !std::isalnum(static_cast<unsigned char>(t[i - 1]))) --i;
			while (i > 0 && std::isalnum(static_cast<unsigned char>(t[i - 1]))) --i;
			return i;
		};

		auto word_right = [](const std::string_view t, int i) -> int {
			const int n = static_cast<int>(t.size());
			while (i < n && !std::isalnum(static_cast<unsigned char>(t[i]))) ++i;
			while (i < n && std::isalnum(static_cast<unsigned char>(t[i]))) ++i;
			return i;
		};

		if (keyboard::pressed(key::home)) {
			move_caret(0);
		}
		if (keyboard::pressed(key::end)) {
			move_caret(static_cast<int>(buffer.size()));
		}
		if (keyboard::pressed(key::left)) {
			move_caret(ctrl ? word_left(buffer, state.caret) : state.caret - 1);
		}
		if (keyboard::pressed(key::right)) {
			move_caret(ctrl ? word_right(buffer, state.caret) : state.caret + 1);
		}

		auto do_backspace = [&] {
			if (has_sel(state)) {
				auto [a, b] = sel_range(state);
				buffer.erase(a, b - a);
				state.caret = state.anchor = a;
			}
			else if (state.caret > 0) {
				buffer.erase(state.caret - 1, 1);
				state.caret--; state.anchor = state.caret;
			}
			state.blink_ms = now_ms(); state.blink_on = true;
		};

		auto do_delete = [&] {
			if (has_sel(state)) {
				auto [a, b] = sel_range(state);
				buffer.erase(a, b - a);
				state.caret = state.anchor = a;
			}
			else if (state.caret < static_cast<int>(buffer.size())) {
				buffer.erase(state.caret, 1);
			}
			state.blink_ms = now_ms(); state.blink_on = true;
		};

		if (keyboard::pressed(key::backspace)) {
			do_backspace(); state.rpt_active = true; state.rpt_next_ms = now_ms() + 400;
		}

		if (keyboard::pressed(key::del)) {
			do_delete();   state.rpt_active = true; state.rpt_next_ms = now_ms() + 400;
		}

		if (state.rpt_active && (keyboard::held(key::backspace) || keyboard::held(key::del))) {
			if (double t = now_ms(); t >= state.rpt_next_ms) {
				if (keyboard::held(key::backspace)) do_backspace();
				if (keyboard::held(key::del))   do_delete();
				state.rpt_next_ms = t + 33;
			}
		}
		else {
			state.rpt_active = false;
		}

		if (hover && mouse::held(mouse_button::button_1)) {
			const float x_local = mouse::position().x() - box_rect.left();
			int i = pick_index_from_x(x_local);
			state.caret = clamp(i, 0, static_cast<int>(buffer.size()));
		}

		float caret_x = context.font->width(buffer.substr(0, state.caret), context.style.font_size);
		if (const float inner_r = box_rect.width() - 5.f; caret_x - state.scroll_x > inner_r) {
			state.scroll_x = caret_x - inner_r;
		}
		if (constexpr float inner_l = 5.f; caret_x - state.scroll_x < inner_l) {
			state.scroll_x = caret_x - inner_l;
		}
		if (state.scroll_x < 0.f) {
			state.scroll_x = 0.f;
		}

		if (double t = now_ms(); t - state.blink_ms > 500.0) {
			state.blink_ms = t; state.blink_on = !state.blink_on;
		}

		if (keyboard::pressed(key::enter) || keyboard::pressed(key::escape)) {
			active_widget_id = {};
		}
	}

	const unitless::vec2 label_pos = {
		row_rect.left(),
		row_rect.center().y() + context.style.font_size / 2.f
	};

	context.text_renderer.draw_text({
		.font = context.font,
		.text = name,
		.position = label_pos,
		.scale = context.style.font_size,
		.clip_rect = row_rect
	});

	context.sprite_renderer.queue({
		.rect = box_rect,
		.color = context.style.color_widget_background,
		.texture = context.blank_texture
	});

	static constexpr float text_padding = 5.f;
	const ui_rect clip_rect = box_rect.inset({ text_padding, 0.f });
	const unitless::vec2 text_pos = {
		box_rect.left() + text_padding,
		box_rect.center().y() + context.style.font_size / 2.f
	};

	if (focused && has_sel(state)) {
		auto [a, b] = sel_range(state);

		float ax = context.font->width(buffer.substr(0, a), context.style.font_size) - state.scroll_x;
		float bx = context.font->width(buffer.substr(0, b), context.style.font_size) - state.scroll_x;

		ui_rect sel_rect = ui_rect::from_position_size(
			{ text_pos.x() + ax, box_rect.top() + (box_rect.height() - context.style.font_size) / 2.f },
			{ std::max(1.f, bx - ax), context.style.font_size }
		);

		context.sprite_renderer.queue({
			.rect = sel_rect,
			.color = context.style.color_dock_widget,
			.texture = context.blank_texture
		});
	}

	context.text_renderer.draw_text({
		.font = context.font,
		.text = buffer,
		.position = { text_pos.x() - state.scroll_x, text_pos.y() },
		.scale = context.style.font_size,
		.clip_rect = clip_rect
	});

	if (focused && state.blink_on) {
		float cx = context.font->width(buffer.substr(0, state.caret), context.style.font_size) - state.scroll_x;

		const ui_rect cursor_rect = ui_rect::from_position_size(
			{ text_pos.x() + cx, box_rect.top() - (box_rect.height() - context.style.font_size) / 2.f },
			{ 2.f, context.style.font_size }
		);

		context.sprite_renderer.queue({
			.rect = cursor_rect,
			.color = context.style.color_text,
			.texture = context.blank_texture
		});
	}

	context.layout_cursor.y() -= widget_height + context.style.padding;
}
