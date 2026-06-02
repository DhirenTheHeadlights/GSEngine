export module gse.graphics:text_area_widget;

import std;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;

import :types;
import :ids;
import :styles;
import :builder;
import :text_buffer;
import :font;

export namespace gse::gui {
	struct text_area_state {
		buffer_position caret;
		buffer_position anchor;
		vec2f scroll{ 0.f, 0.f };
		time last_blink{};
		bool blink_on = true;
		bool rpt_active = false;
		time rpt_next{};
	};

	struct text_area {
		using result = void;

		struct params {
			text_buffer& buffer;
			text_area_state& state;
			std::span<const text_span> spans{};
			std::optional<ui_rect> rect{};
			bool read_only = false;
			bool show_line_numbers = false;
			time blink_interval = milliseconds(500);
		};

		static auto draw(
			const draw_context& ctx,
			const params& p,
			id& hot,
			id& active,
			id& focus
		) -> void;
	};
}

export namespace gse::gui::draw {
	auto text_area_in_rect(
		const draw_context& ctx,
		id widget_id,
		text_buffer& buffer,
		text_area_state& state,
		std::span<const text_span> spans,
		const ui_rect& rect,
		bool read_only,
		bool show_line_numbers,
		time blink_interval,
		id& hot_widget_id,
		id& focus_widget_id
	) -> bool;
}

auto gse::gui::text_area::draw(const draw_context& ctx, const params& p, id& hot, id& active, id& focus) -> void {
	(void)active;
	const ui_rect rect = p.rect.value_or(ctx.next_row(8.f));
	draw::text_area_in_rect(
		ctx,
		ids::make_from_key(stable_id("##TextArea")),
		p.buffer,
		p.state,
		p.spans,
		rect,
		p.read_only,
		p.show_line_numbers,
		p.blink_interval,
		hot,
		focus
	);
}

auto gse::gui::draw::text_area_in_rect(const draw_context& ctx, id widget_id, text_buffer& buffer, text_area_state& state, std::span<const text_span> spans, const ui_rect& rect, bool read_only, bool show_line_numbers, time blink_interval, id& hot_widget_id, id& focus_widget_id) -> bool {
	(void)spans;
	(void)show_line_numbers;

	bool modified = false;

	state.caret = buffer.clamp(state.caret);
	state.anchor = buffer.clamp(state.anchor);

	const float scale = ctx.style.font_size;
	const float pad = ctx.style.padding;
	const float line_h = ctx.font->line_height(scale);

	const float text_x = rect.left() + pad - state.scroll.x();
	const float top_y = rect.top() - pad + state.scroll.y();

	const bool hovered = rect.contains(ctx.input.mouse_position()) && ctx.input_available();
	if (hovered) {
		hot_widget_id = widget_id;
	}

	if (ctx.input.mouse_button_pressed(mouse_button::button_1)) {
		if (hovered) {
			focus_widget_id = widget_id;

			const auto mouse = ctx.input.mouse_position();
			const int line_count = static_cast<int>(buffer.line_count());
			const auto picked_line = static_cast<std::uint32_t>(
				std::clamp(static_cast<int>((top_y - mouse.y()) / line_h), 0, std::max(0, line_count - 1)));
			const std::string_view line = buffer.line(picked_line);

			int picked_col = 0;
			float best_dx = std::numeric_limits<float>::max();
			for (int k = 0; k <= static_cast<int>(line.size()); ++k) {
				if (const float dx = std::abs(text_x + ctx.font->width(line.substr(0, k), scale) - mouse.x()); dx < best_dx) {
					best_dx = dx;
					picked_col = k;
				}
			}

			state.caret = buffer.clamp({ picked_line, static_cast<std::uint32_t>(picked_col) });
			state.anchor = state.caret;
			state.last_blink = system_clock::now<time>();
			state.blink_on = true;
		}
		else if (focus_widget_id == widget_id && ctx.input_available()) {
			focus_widget_id = {};
		}
	}

	const bool focused = (focus_widget_id == widget_id);

	if (focused) {
		auto line_len = [&](const std::uint32_t l) -> std::uint32_t {
			return static_cast<std::uint32_t>(buffer.line(l).size());
		};
		auto pos_left = [&](const buffer_position p) -> buffer_position {
			if (p.column > 0) {
				return { p.line, p.column - 1 };
			}
			if (p.line > 0) {
				return { p.line - 1, line_len(p.line - 1) };
			}
			return p;
		};
		auto pos_right = [&](const buffer_position p) -> buffer_position {
			if (p.column < line_len(p.line)) {
				return { p.line, p.column + 1 };
			}
			if (p.line + 1 < buffer.line_count()) {
				return { p.line + 1, 0 };
			}
			return p;
		};

		bool changed = false;
		auto move_to = [&](const buffer_position p) {
			state.caret = buffer.clamp(p);
			state.anchor = state.caret;
			changed = true;
		};

		if (ctx.key_pressed_for(key::left)) {
			move_to(pos_left(state.caret));
		}
		if (ctx.key_pressed_for(key::right)) {
			move_to(pos_right(state.caret));
		}
		if (ctx.key_pressed_for(key::up) && state.caret.line > 0) {
			move_to({ state.caret.line - 1, state.caret.column });
		}
		if (ctx.key_pressed_for(key::down) && state.caret.line + 1 < buffer.line_count()) {
			move_to({ state.caret.line + 1, state.caret.column });
		}
		if (ctx.key_pressed_for(key::home)) {
			move_to({ state.caret.line, 0 });
		}
		if (ctx.key_pressed_for(key::end)) {
			move_to({ state.caret.line, line_len(state.caret.line) });
		}

		if (!read_only) {
			if (const std::string_view entered = ctx.input.text_entered(); !entered.empty()) {
				state.caret = buffer.insert(state.caret, entered);
				state.anchor = state.caret;
				changed = true;
				modified = true;
			}
			if (ctx.key_pressed_for(key::enter)) {
				state.caret = buffer.insert(state.caret, "\n");
				state.anchor = state.caret;
				changed = true;
				modified = true;
			}
			if (ctx.key_pressed_for(key::backspace)) {
				if (const buffer_position from = pos_left(state.caret); from != state.caret) {
					buffer.erase(from, state.caret);
					state.caret = from;
					state.anchor = from;
					changed = true;
					modified = true;
				}
			}
			if (ctx.key_pressed_for(key::del)) {
				if (const buffer_position to = pos_right(state.caret); to != state.caret) {
					buffer.erase(state.caret, to);
					changed = true;
					modified = true;
				}
			}
		}

		if (changed) {
			state.caret = buffer.clamp(state.caret);
			state.anchor = buffer.clamp(state.anchor);
			state.last_blink = system_clock::now<time>();
			state.blink_on = true;
		}
	}

	if (blink_interval <= time{}) {
		state.blink_on = true;
	}
	else if (focused) {
		if (const auto now = system_clock::now<time>(); now - state.last_blink > blink_interval) {
			state.last_blink = now;
			state.blink_on = !state.blink_on;
		}
	}

	ctx.queue_sprite({
		.rect = rect,
		.color = ctx.style.color_input_background,
		.texture = ctx.blank_texture,
		.corner_radius = ctx.style.corner_radius,
	});

	for (std::size_t i = 0; i < buffer.line_count(); ++i) {
		const std::string_view line = buffer.line(i);
		if (!line.empty()) {
			const float line_center = top_y - static_cast<float>(i) * line_h - line_h * 0.5f;
			ctx.queue_text({
				.font = ctx.font,
				.text = std::string(line),
				.position = { text_x, line_center + ctx.font->vertical_center_offset(scale) },
				.scale = scale,
				.color = ctx.style.color_text,
				.clip_rect = rect,
			});
		}
	}

	if (focused && state.blink_on) {
		const std::string_view caret_line = buffer.line(state.caret.line);
		const float caret_x = text_x + ctx.font->width(caret_line.substr(0, state.caret.column), scale);
		const float caret_top = top_y - static_cast<float>(state.caret.line) * line_h - line_h * 0.5f + ctx.font->vertical_center_offset(scale);
		ctx.queue_sprite({
			.rect = ui_rect::from_position_size({ caret_x, caret_top }, { 2.f, line_h }),
			.color = ctx.style.color_caret,
			.texture = ctx.blank_texture,
		});
	}

	return modified;
}
