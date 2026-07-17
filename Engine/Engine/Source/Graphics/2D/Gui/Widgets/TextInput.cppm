export module gse.graphics:text_input_widget;

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
import :interaction;

export namespace gse::gui {
	struct text_input_state {
		int caret = 0;
		int anchor = 0;
		float scroll_x = 0.f;
		time last_blink{};
		bool blink_on = true;
		bool rpt_active = false;
		time rpt_next{};
		interaction::click_state click;
		int select_granularity = 0;
		int select_origin = 0;
	};
}

export namespace gse::gui::draw {
	auto text_input_in_rect(
		const draw_context& ctx,
		id widget_id,
		std::string& buffer,
		text_input_state& state,
		const rectf& box_rect,
		id& hot_widget_id,
		id& focus_widget_id
	) -> void;

	auto text_input(
		const draw_context& ctx,
		const std::string& name,
		std::string& buffer,
		text_input_state& state,
		id& hot_widget_id,
		id& focus_widget_id
	) -> void;
}

export namespace gse::gui {
	struct text_input {
		using result = void;
		struct params {
			std::string_view name;
			std::string& buffer;
			text_input_state& state;
		};
		static auto draw(const draw_context& ctx, const params& p, id& hot, id&, id& focus) -> void {
			draw::text_input(ctx, std::string(p.name), p.buffer, p.state, hot, focus);
		}
	};
}

auto gse::gui::draw::text_input(const draw_context& ctx, const std::string& name, std::string& buffer, text_input_state& state, id& hot_widget_id, id& focus_widget_id) -> void {
	if (!ctx.current_menu) {
		return;
	}

	constexpr std::uint64_t input_suffix_hash = stable_id("##Input");
	const id widget_id = ids::make_from_key(hash_combine(stable_id(name), input_suffix_hash));

	const float widget_height = ctx.font->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
	const rectf content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });

	const rectf row_rect = rectf::from_position_size(
		{ content_rect.left(), ctx.layout_cursor.y() },
		{ content_rect.width(), widget_height }
	);

	const float label_width = content_rect.width() * 0.4f;

	const rectf label_rect = rectf::from_position_size(
		row_rect.top_left(),
		{ label_width, widget_height }
	);

	const rectf box_rect = rectf::from_position_size(
		{ row_rect.left() + label_width, row_rect.top() },
		{ content_rect.width() - label_width, widget_height }
	);

	ctx.queue_text({
		.font = ctx.font,
		.text = name,
		.position = { label_rect.left(), label_rect.center().y() + ctx.font->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text,
		.clip_rect = label_rect
	});

	text_input_in_rect(ctx, widget_id, buffer, state, box_rect, hot_widget_id, focus_widget_id);

	ctx.layout_cursor.y() -= widget_height + ctx.style.padding;
}

auto gse::gui::draw::text_input_in_rect(const draw_context& ctx, const id widget_id, std::string& buffer, text_input_state& state, const rectf& box_rect, id& hot_widget_id, id& focus_widget_id) -> void {
	state.caret = std::clamp(state.caret, 0, static_cast<int>(buffer.size()));
	state.anchor = std::clamp(state.anchor, 0, static_cast<int>(buffer.size()));

	const bool hovered = box_rect.contains(ctx.input.mouse_position()) && ctx.input_available();

	if (hovered) {
		hot_widget_id = widget_id;
	}

	if (ctx.input.mouse_button_pressed(mouse_button::button_1)) {
		if (hovered) {
			focus_widget_id = widget_id;
		}
		else if (focus_widget_id == widget_id && ctx.input_available()) {
			focus_widget_id = {};
		}
	}

	const bool focused = (focus_widget_id == widget_id);

	auto pick_index_from_x = [&](const float x_local) -> int {
		const int n = static_cast<int>(buffer.size());
		const std::vector<float> offsets = ctx.font->caret_offsets(buffer, ctx.style.font_size);
		float best_dx = std::numeric_limits<float>::max();
		int best_k = 0;

		for (int k = 0; k <= n; ++k) {
			if (const float dx = std::abs(offsets[static_cast<std::size_t>(k)] - state.scroll_x - x_local); dx < best_dx) {
				best_dx = dx;
				best_k = k;
			}
		}

		return best_k;
	};

	auto classify_char = [](const char c) -> int {
		if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
			return 1;
		}
		if (c == ' ' || c == '\t') {
			return 0;
		}
		return 2;
	};

	auto word_bounds = [&](const std::string_view text, const int index) -> std::pair<int, int> {
		const int n = static_cast<int>(text.size());
		if (n == 0) {
			return { 0, 0 };
		}
		const int i = std::clamp(index, 0, n - 1);
		const int cls = classify_char(text[static_cast<std::size_t>(i)]);
		int lo = i;
		int hi = i;
		while (lo > 0 && classify_char(text[static_cast<std::size_t>(lo - 1)]) == cls) {
			--lo;
		}
		while (hi + 1 < n && classify_char(text[static_cast<std::size_t>(hi + 1)]) == cls) {
			++hi;
		}
		return { lo, hi + 1 };
	};

	if (ctx.mouse_pressed_for(box_rect)) {
		const bool shift = ctx.input.key_held(key::left_shift) || ctx.input.key_held(key::right_shift);
		const float x_local = ctx.input.mouse_position().x() - box_rect.left();
		const int i = std::clamp(pick_index_from_x(x_local), 0, static_cast<int>(buffer.size()));
		interaction::register_click(state.click, { x_local, 0.f });
		state.select_origin = i;

		if (shift) {
			state.caret = i;
			state.select_granularity = 0;
		}
		else if (state.click.count == 3) {
			state.anchor = 0;
			state.caret = static_cast<int>(buffer.size());
			state.select_granularity = 2;
		}
		else if (state.click.count == 2) {
			const auto [lo, hi] = word_bounds(buffer, i);
			state.anchor = lo;
			state.caret = hi;
			state.select_granularity = 1;
		}
		else {
			state.caret = state.anchor = i;
			state.select_granularity = 0;
		}

		state.last_blink = system_clock::now<time>();
		state.blink_on = true;
	}

	auto has_sel = [](const text_input_state& s) -> bool {
		return s.caret != s.anchor;
	};

	auto sel_range = [](const text_input_state& s) -> std::pair<int, int> {
		return s.caret < s.anchor ? std::pair{ s.caret, s.anchor } : std::pair{ s.anchor, s.caret };
	};

	if (focused) {
		const bool shift = ctx.input.key_held(key::left_shift) || ctx.input.key_held(key::right_shift);
		const bool ctrl = ctx.input.key_held(key::left_control) || ctx.input.key_held(key::right_control);

		if (const std::string_view entered = ctx.input.text_entered(); !entered.empty() && !ctrl) {
			if (has_sel(state)) {
				auto [a, b] = sel_range(state);
				buffer.erase(a, b - a);
				state.caret = state.anchor = a;
			}

			buffer.insert(state.caret, entered);
			state.caret += static_cast<int>(entered.size());
			state.anchor = state.caret;
			state.last_blink = system_clock::now<time>();
			state.blink_on = true;
		}

		auto move_caret = [&](int new_i) {
			new_i = std::clamp(new_i, 0, static_cast<int>(buffer.size()));
			if (shift) {
				state.caret = new_i;
			}
			else {
				state.caret = state.anchor = new_i;
			}
			state.last_blink = system_clock::now<time>();
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

		if (ctrl && ctx.key_pressed_for(key::c) && has_sel(state)) {
			auto [a, b] = sel_range(state);
			ctx.set_clipboard(buffer.substr(a, b - a));
		}

		if (ctrl && ctx.key_pressed_for(key::a)) {
			state.anchor = 0;
			state.caret = static_cast<int>(buffer.size());
			state.select_granularity = 0;
			state.last_blink = system_clock::now<time>();
			state.blink_on = true;
		}

		if (ctrl && ctx.key_pressed_for(key::x) && has_sel(state)) {
			auto [a, b] = sel_range(state);
			ctx.set_clipboard(buffer.substr(a, b - a));
			buffer.erase(a, b - a);
			state.caret = state.anchor = a;
			state.last_blink = system_clock::now<time>();
			state.blink_on = true;
		}

		if (ctrl && ctx.key_pressed_for(key::v)) {
			std::string paste = ctx.clipboard();
			if (const std::size_t newline = paste.find_first_of("\r\n"); newline != std::string::npos) {
				paste.resize(newline);
			}
			if (!paste.empty()) {
				if (has_sel(state)) {
					auto [a, b] = sel_range(state);
					buffer.erase(a, b - a);
					state.caret = state.anchor = a;
				}
				buffer.insert(state.caret, paste);
				state.caret += static_cast<int>(paste.size());
				state.anchor = state.caret;
				state.last_blink = system_clock::now<time>();
				state.blink_on = true;
			}
		}

		if (ctx.key_pressed_for(key::home)) {
			move_caret(0);
		}

		if (ctx.key_pressed_for(key::end)) {
			move_caret(static_cast<int>(buffer.size()));
		}

		if (ctx.key_pressed_for(key::left)) {
			move_caret(ctrl ? word_left(buffer, state.caret) : state.caret - 1);
		}

		if (ctx.key_pressed_for(key::right)) {
			move_caret(ctrl ? word_right(buffer, state.caret) : state.caret + 1);
		}

		auto do_backspace = [&] {
			if (has_sel(state)) {
				auto [a, b] = sel_range(state);
				buffer.erase(a, b - a);
				state.caret = state.anchor = a;
			}
			else if (ctrl && state.caret > 0) {
				const int start = word_left(buffer, state.caret);
				buffer.erase(start, state.caret - start);
				state.caret = state.anchor = start;
			}
			else if (state.caret > 0) {
				buffer.erase(state.caret - 1, 1);
				--state.caret;
				state.anchor = state.caret;
			}
			state.last_blink = system_clock::now<time>();
			state.blink_on = true;
		};

		auto do_delete = [&] {
			if (has_sel(state)) {
				auto [a, b] = sel_range(state);
				buffer.erase(a, b - a);
				state.caret = state.anchor = a;
			}
			else if (ctrl && state.caret < static_cast<int>(buffer.size())) {
				const int end = word_right(buffer, state.caret);
				buffer.erase(state.caret, end - state.caret);
			}
			else if (state.caret < static_cast<int>(buffer.size())) {
				buffer.erase(state.caret, 1);
			}
			state.last_blink = system_clock::now<time>();
			state.blink_on = true;
		};

		if (ctx.key_pressed_for(key::backspace)) {
			do_backspace();
			state.rpt_active = true;
			state.rpt_next = system_clock::now<time>() + milliseconds(400);
		}

		if (ctx.key_pressed_for(key::del)) {
			do_delete();
			state.rpt_active = true;
			state.rpt_next = system_clock::now<time>() + milliseconds(400);
		}

		if (state.rpt_active && (ctx.input.key_held(key::backspace) || ctx.input.key_held(key::del))) {
			if (const auto t = system_clock::now<time>(); t >= state.rpt_next) {
				if (ctx.input.key_held(key::backspace)) {
					do_backspace();
				}
				if (ctx.input.key_held(key::del)) {
					do_delete();
				}
				state.rpt_next = t + milliseconds(33);
			}
		}
		else {
			state.rpt_active = false;
		}

		if (hovered && ctx.input.mouse_button_held(mouse_button::button_1)) {
			const float x_local = ctx.input.mouse_position().x() - box_rect.left();
			const int current = std::clamp(pick_index_from_x(x_local), 0, static_cast<int>(buffer.size()));
			if (state.select_granularity == 1) {
				const auto [anchor_lo, anchor_hi] = word_bounds(buffer, state.select_origin);
				const auto [current_lo, current_hi] = word_bounds(buffer, current);
				if (current < state.select_origin) {
					state.anchor = anchor_hi;
					state.caret = current_lo;
				}
				else {
					state.anchor = anchor_lo;
					state.caret = current_hi;
				}
			}
			else if (state.select_granularity == 0) {
				state.caret = current;
			}
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

		if (const auto t = system_clock::now<time>(); t - state.last_blink > milliseconds(500)) {
			state.last_blink = t;
			state.blink_on = !state.blink_on;
		}

		if (ctx.key_pressed_for(key::enter) || ctx.key_pressed_for(key::escape)) {
			focus_widget_id = {};
		}
	}

	ctx.queue_sprite({
		.rect = box_rect,
		.color = ctx.style.color_input_background,
		.texture = ctx.blank_texture,
		.corner_radius = ctx.style.corner_radius
	});

	constexpr float text_padding = 5.f;
	const rectf clip_rect = box_rect.inset({ text_padding, 0.f });
	const vec2f text_pos = { box_rect.left() + text_padding,
							 box_rect.center().y() + ctx.font->vertical_center_offset(ctx.style.font_size) };

	if (focused && has_sel(state)) {
		auto [a, b] = sel_range(state);
		const float ax = ctx.font->width(buffer.substr(0, a), ctx.style.font_size) - state.scroll_x;
		const float bx = ctx.font->width(buffer.substr(0, b), ctx.style.font_size) - state.scroll_x;

		const rectf sel_rect = rectf::from_position_size(
			{ text_pos.x() + ax, box_rect.top() - (box_rect.height() - ctx.style.font_size) / 2.f },
			{ std::max(1.f, bx - ax),
			  ctx.style.font_size }
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
		const rectf cursor_rect = rectf::from_position_size(
			{ text_pos.x() + cx, box_rect.top() - (box_rect.height() - ctx.style.font_size) / 2.f },
			{ 2.f, ctx.style.font_size }
		);

		ctx.queue_sprite({
			.rect = cursor_rect,
			.color = ctx.style.color_caret,
			.texture = ctx.blank_texture
		});
	}
}
