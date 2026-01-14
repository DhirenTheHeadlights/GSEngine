export module gse.graphics:input_widget;

import std;

import gse.platform;
import gse.utility;

import :types;
import :ids;
import :styles;

export namespace gse::gui::draw {
	auto input(
		const draw_context& ctx,
		const std::string& name,
		std::string& buffer,
		id& hot_widget_id,
		id& focus_widget_id
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

auto gse::gui::draw::input(const draw_context& ctx, const std::string& name, std::string& buffer, id& hot_widget_id, id& focus_widget_id) -> void {
    if (!ctx.current_menu) {
        return;
    }

    const std::string id_label = name + "##Input";
    const id widget_id = ids::make(id_label);

    input_state& state = global_input_state[widget_id.number()];
    state.caret = std::clamp(state.caret, 0, static_cast<int>(buffer.size()));
    state.anchor = std::clamp(state.anchor, 0, static_cast<int>(buffer.size()));

    const float widget_height = ctx.font->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
    const ui_rect content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });

    const ui_rect row_rect = ui_rect::from_position_size(
        { content_rect.left(), ctx.layout_cursor.y() },
        { content_rect.width(), widget_height }
    );

    const float label_width = content_rect.width() * 0.4f;
    const ui_rect box_rect = ui_rect::from_position_size(
        { row_rect.left() + label_width, row_rect.top() },
        { content_rect.width() - label_width, widget_height }
    );

    const bool hovered = box_rect.contains(ctx.input.mouse_position());

    if (hovered) {
        hot_widget_id = widget_id;
    }

    if (ctx.input.mouse_button_pressed(mouse_button::button_1)) {
        if (hovered) {
            focus_widget_id = widget_id;
        } else if (focus_widget_id == widget_id) {
            focus_widget_id = {};
        }
    }

    const bool focused = (focus_widget_id == widget_id);

    auto pick_index_from_x = [&](const float x_local) -> int {
        const int n = static_cast<int>(buffer.size());
        float best_dx = std::numeric_limits<float>::max();
        int best_k = 0;

        for (int k = 0; k <= n; ++k) {
            const float w = ctx.font->width(buffer.substr(0, k), ctx.style.font_size);
            if (const float dx = std::abs(w - state.scroll_x - x_local); dx < best_dx) {
                best_dx = dx;
                best_k = k;
            }
        }

        return best_k;
    };

    if (hovered && ctx.input.mouse_button_pressed(mouse_button::button_1)) {
        const bool shift = ctx.input.key_held(key::left_shift) || ctx.input.key_held(key::right_shift);
        const float x_local = ctx.input.mouse_position().x() - box_rect.left();
        const int i = pick_index_from_x(x_local);

        if (shift) {
            state.caret = std::clamp(i, 0, static_cast<int>(buffer.size()));
        } else {
            state.caret = state.anchor = std::clamp(i, 0, static_cast<int>(buffer.size()));
        }

        state.blink_ms = system_clock::now<time>().as<milliseconds>();
        state.blink_on = true;
    }

    auto has_sel = [](const input_state& s) -> bool {
        return s.caret != s.anchor;
    };

    auto sel_range = [](const input_state& s) -> std::pair<int, int> {
        return s.caret < s.anchor ? std::pair{ s.caret, s.anchor } : std::pair{ s.anchor, s.caret };
    };

    if (focused) {
        if (const std::string_view entered = ctx.input.text_entered(); !entered.empty()) {
            if (has_sel(state)) {
                auto [a, b] = sel_range(state);
                buffer.erase(a, b - a);
                state.caret = state.anchor = a;
            }

            buffer.insert(state.caret, entered);
            state.caret += static_cast<int>(entered.size());
            state.anchor = state.caret;
            state.blink_ms = system_clock::now<time>().as<milliseconds>();
            state.blink_on = true;
        }

        const bool shift = ctx.input.key_held(key::left_shift) || ctx.input.key_held(key::right_shift);
        const bool ctrl = ctx.input.key_held(key::left_control) || ctx.input.key_held(key::right_control);

        auto move_caret = [&](int new_i) {
            new_i = std::clamp(new_i, 0, static_cast<int>(buffer.size()));
            if (shift) {
                state.caret = new_i;
            } else {
                state.caret = state.anchor = new_i;
            }
            state.blink_ms = system_clock::now<time>().as<milliseconds>();
            state.blink_on = true;
        };

        auto word_left = [](const std::string_view t, int i) -> int {
            i = std::max(i, 0);
            while (i > 0 && !std::isalnum(static_cast<unsigned char>(t[i - 1]))) {
                --i;
            }
            while (i > 0 && std::isalnum(static_cast<unsigned char>(t[i - 1]))) {
                --i;
            }
            return i;
        };

        auto word_right = [](const std::string_view t, int i) -> int {
            const int n = static_cast<int>(t.size());
            while (i < n && !std::isalnum(static_cast<unsigned char>(t[i]))) {
                ++i;
            }
            while (i < n && std::isalnum(static_cast<unsigned char>(t[i]))) {
                ++i;
            }
            return i;
        };

        if (ctx.input.key_pressed(key::home)) {
            move_caret(0);
        }

        if (ctx.input.key_pressed(key::end)) {
            move_caret(static_cast<int>(buffer.size()));
        }

        if (ctx.input.key_pressed(key::left)) {
            move_caret(ctrl ? word_left(buffer, state.caret) : state.caret - 1);
        }

        if (ctx.input.key_pressed(key::right)) {
            move_caret(ctrl ? word_right(buffer, state.caret) : state.caret + 1);
        }

        auto do_backspace = [&] {
            if (has_sel(state)) {
                auto [a, b] = sel_range(state);
                buffer.erase(a, b - a);
                state.caret = state.anchor = a;
            } else if (state.caret > 0) {
                buffer.erase(state.caret - 1, 1);
                --state.caret;
                state.anchor = state.caret;
            }
            state.blink_ms = system_clock::now<time>().as<milliseconds>();
            state.blink_on = true;
        };

        auto do_delete = [&] {
            if (has_sel(state)) {
                auto [a, b] = sel_range(state);
                buffer.erase(a, b - a);
                state.caret = state.anchor = a;
            } else if (state.caret < static_cast<int>(buffer.size())) {
                buffer.erase(state.caret, 1);
            }
            state.blink_ms = system_clock::now<time>().as<milliseconds>();
            state.blink_on = true;
        };

        if (ctx.input.key_pressed(key::backspace)) {
            do_backspace();
            state.rpt_active = true;
            state.rpt_next_ms = system_clock::now<time>().as<milliseconds>() + 400;
        }

        if (ctx.input.key_pressed(key::del)) {
            do_delete();
            state.rpt_active = true;
            state.rpt_next_ms = system_clock::now<time>().as<milliseconds>() + 400;
        }

        if (state.rpt_active && (ctx.input.key_held(key::backspace) || ctx.input.key_held(key::del))) {
            if (const double t = system_clock::now<time>().as<milliseconds>(); t >= state.rpt_next_ms) {
                if (ctx.input.key_held(key::backspace)) {
                    do_backspace();
                }
                if (ctx.input.key_held(key::del)) {
                    do_delete();
                }
                state.rpt_next_ms = t + 33;
            }
        } else {
            state.rpt_active = false;
        }

        if (hovered && ctx.input.mouse_button_held(mouse_button::button_1)) {
            const float x_local = ctx.input.mouse_position().x() - box_rect.left();
            const int i = pick_index_from_x(x_local);
            state.caret = std::clamp(i, 0, static_cast<int>(buffer.size()));
        }

        const float caret_x = ctx.font->width(buffer.substr(0, state.caret), ctx.style.font_size);

        if (const float inner_r = box_rect.width() - 5.f; caret_x - state.scroll_x > inner_r) {
            state.scroll_x = caret_x - inner_r;
        }

        if (constexpr float inner_l = 5.f; caret_x - state.scroll_x < inner_l) {
            state.scroll_x = caret_x - inner_l;
        }

        if (state.scroll_x < 0.f) {
            state.scroll_x = 0.f;
        }

        if (const double t = system_clock::now<time>().as<milliseconds>(); t - state.blink_ms > 500.0) {
            state.blink_ms = t;
            state.blink_on = !state.blink_on;
        }

        if (ctx.input.key_pressed(key::enter) || ctx.input.key_pressed(key::escape)) {
            focus_widget_id = {};
        }
    }

    const unitless::vec2 label_pos = {
        row_rect.left(),
        row_rect.center().y() + ctx.style.font_size / 2.f
    };

    ctx.queue_text({
        .font = ctx.font,
        .text = name,
        .position = label_pos,
        .scale = ctx.style.font_size,
        .color = ctx.style.color_text,
        .clip_rect = row_rect
    });

    ctx.queue_sprite({
        .rect = box_rect,
        .color = ctx.style.color_input_background,
        .texture = ctx.blank_texture
    });

    constexpr float text_padding = 5.f;
    const ui_rect clip_rect = box_rect.inset({ text_padding, 0.f });
    const unitless::vec2 text_pos = {
        box_rect.left() + text_padding,
        box_rect.center().y() + ctx.style.font_size / 2.f
    };

    if (focused && has_sel(state)) {
        auto [a, b] = sel_range(state);
        const float ax = ctx.font->width(buffer.substr(0, a), ctx.style.font_size) - state.scroll_x;
        const float bx = ctx.font->width(buffer.substr(0, b), ctx.style.font_size) - state.scroll_x;

        const ui_rect sel_rect = ui_rect::from_position_size(
            { text_pos.x() + ax, box_rect.top() - (box_rect.height() - ctx.style.font_size) / 2.f },
            { std::max(1.f, bx - ax), ctx.style.font_size }
        );

        ctx.queue_sprite({
            .rect = sel_rect,
            .color = ctx.style.color_selection,
            .texture = ctx.blank_texture
        });
    }

    ctx.queue_text({
        .font = ctx.font,
        .text = buffer,
        .position = { text_pos.x() - state.scroll_x, text_pos.y() },
        .scale = ctx.style.font_size,
        .color = ctx.style.color_text,
        .clip_rect = clip_rect
    });

    if (focused && state.blink_on) {
        const float cx = ctx.font->width(buffer.substr(0, state.caret), ctx.style.font_size) - state.scroll_x;
        const ui_rect cursor_rect = ui_rect::from_position_size(
            { text_pos.x() + cx, box_rect.top() - (box_rect.height() - ctx.style.font_size) / 2.f },
            { 2.f, ctx.style.font_size }
        );

        ctx.queue_sprite({
            .rect = cursor_rect,
            .color = ctx.style.color_caret,
            .texture = ctx.blank_texture
        });
    }

    ctx.layout_cursor.y() -= widget_height + ctx.style.padding;
}