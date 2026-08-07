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
import gse.assert;

import :types;
import :ids;
import :styles;
import :builder;
import :text_buffer;
import :font;
import :interaction;

export namespace gse::gui {
	struct text_edit_snapshot {
		std::vector<std::string> lines;
		buffer_position caret;
		buffer_position anchor;
	};

	enum class text_edit_action : std::uint32_t {
		none = 0,
		copy,
		cut,
		paste,
	};

	struct text_area_state {
		buffer_position caret;
		buffer_position anchor;
		scroll_state scroll{};
		time last_blink{};
		bool blink_on = true;
		bool rpt_active = false;
		time rpt_next{};
		std::vector<text_edit_snapshot> undo_stack;
		std::vector<text_edit_snapshot> redo_stack;
		int last_edit_kind = 0;
		bool selecting = false;
		interaction::click_state click;
		int select_granularity = 0;
		buffer_position select_origin{};
		float widest_line_px = 0.f;
		std::size_t width_sig = 0;
		text_edit_action pending_action = text_edit_action::none;
		id context_menu_tag{};
	};

	struct text_area {
		using result = void;

		struct params {
			text_buffer& buffer;
			text_area_state& state;
			std::span<const text_span> spans{};
			std::span<const text_underline> underlines{};
			std::span<const text_fade> fades{};
			std::optional<rectf> rect{};
			bool read_only = false;
			bool show_line_numbers = false;
			std::size_t indent_width = 4;
			bool indent_with_spaces = false;
			bool auto_indent = false;
			time blink_interval = milliseconds(500);
			resource::handle<font> font{};
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
		const text_area::params& params,
		id& hot_widget_id,
		id& focus_widget_id
	) -> bool;

	auto text_area_position_at(
		const draw_context& ctx,
		const text_buffer& buffer,
		const text_area_state& state,
		const rectf& rect,
		bool show_line_numbers,
		std::size_t indent_width,
		vec2f mouse,
		resource::handle<font> font = {}
	) -> buffer_position;
}

auto gse::gui::text_area::draw(const draw_context& ctx, const params& p, id& hot, id& active, id& focus) -> void {
	(void)active;
	const rectf rect = p.rect.value_or(ctx.next_row(p.font.valid() ? p.font : ctx.fonts.code, 8.f));
	draw::text_area_in_rect(
		ctx,
		ids::make_from_key(stable_id("##TextArea")),
		{
			.buffer = p.buffer,
			.state = p.state,
			.spans = p.spans,
			.underlines = p.underlines,
			.fades = p.fades,
			.rect = rect,
			.read_only = p.read_only,
			.show_line_numbers = p.show_line_numbers,
			.indent_width = p.indent_width,
			.indent_with_spaces = p.indent_with_spaces,
			.auto_indent = p.auto_indent,
			.blink_interval = p.blink_interval,
			.font = p.font,
		},
		hot,
		focus
	);
}

auto gse::gui::draw::text_area_position_at(const draw_context& ctx, const text_buffer& buffer, const text_area_state& state, const rectf& rect, const bool show_line_numbers, const std::size_t indent_width, const vec2f mouse, const resource::handle<font> font) -> buffer_position {
	const auto fnt = font.valid() ? font : ctx.fonts.code;
	const auto fnt_view = fnt.resolve();
	const float scale = ctx.style.font_size;
	const float pad = ctx.style.padding;
	const float line_h = fnt_view->line_height(scale) * 1.25f;
	const std::size_t line_digits = std::max<std::size_t>(2, std::to_string(std::max<std::size_t>(1, buffer.line_count())).size());
	const float gutter_width = show_line_numbers ? fnt_view->width(std::string(line_digits, '0'), scale) + pad * 2.f : 0.f;
	const float left_inset = show_line_numbers ? gutter_width : pad;
	const float text_x = rect.left() + left_inset - state.scroll.x.offset;
	const float top_y = rect.top() - pad + state.scroll.y.offset;
	const std::size_t display_tab_width = std::clamp<std::size_t>(indent_width, 1, 16);

	const int line_count = static_cast<int>(buffer.line_count());
	const auto picked_line = static_cast<std::uint32_t>(std::clamp(static_cast<int>((top_y - mouse.y()) / line_h), 0, std::max(0, line_count - 1)));
	const std::string_view line = buffer.line(picked_line);

	std::string expanded;
	std::vector<std::size_t> col_to_expanded(line.size() + 1);
	std::size_t display_col = 0;
	for (std::size_t i = 0; i < line.size(); ++i) {
		col_to_expanded[i] = expanded.size();
		if (line[i] == '\t') {
			const std::size_t tab = display_tab_width - display_col % display_tab_width;
			expanded.append(tab, ' ');
			display_col += tab;
		}
		else {
			expanded.push_back(line[i]);
			++display_col;
		}
	}
	col_to_expanded[line.size()] = expanded.size();
	const std::vector<float> expanded_offsets = fnt_view->caret_offsets(expanded, scale);

	int picked_col = 0;
	float best_dx = std::numeric_limits<float>::max();
	for (int k = 0; k <= static_cast<int>(line.size()); ++k) {
		const float x = text_x + expanded_offsets[col_to_expanded[static_cast<std::size_t>(k)]];
		if (const float dx = std::abs(x - mouse.x()); dx < best_dx) {
			best_dx = dx;
			picked_col = k;
		}
	}
	return buffer.clamp({
		.line = picked_line,
		.column = static_cast<std::uint32_t>(picked_col),
	});
}

auto gse::gui::draw::text_area_in_rect(const draw_context& ctx, const id widget_id, const text_area::params& params, id& hot_widget_id, id& focus_widget_id) -> bool {
	assert(params.rect.has_value(), "text_area_in_rect requires a rect");
	text_buffer& buffer = params.buffer;
	text_area_state& state = params.state;
	const std::span<const text_span> spans = params.spans;
	const std::span<const text_underline> underlines = params.underlines;
	const std::span<const text_fade> fades = params.fades;
	const rectf& rect = *params.rect;
	const bool read_only = params.read_only;
	const bool show_line_numbers = params.show_line_numbers;
	const std::size_t indent_width = params.indent_width;
	const bool indent_with_spaces = params.indent_with_spaces;
	const bool auto_indent = params.auto_indent;
	const time blink_interval = params.blink_interval;
	const resource::handle<font> font = params.font;
	const auto fnt = font.valid() ? font : ctx.fonts.code;
	const auto fnt_view = fnt.resolve();
	(void)spans;

	bool modified = false;
	bool caret_moved = false;

	state.caret = buffer.clamp(state.caret);
	state.anchor = buffer.clamp(state.anchor);

	const float scale = ctx.style.font_size;
	const float pad = ctx.style.padding;
	const float line_h = fnt_view->line_height(scale) * 1.25f;
	const scroll_config scroll_cfg{};
	const bool scrollable = static_cast<float>(buffer.line_count()) * line_h + pad * 2.f > rect.height();
	const float scrollbar_gutter = scrollable ? scroll_cfg.scrollbar_width : 0.f;

	const std::size_t line_digits = std::max<std::size_t>(2, std::to_string(std::max<std::size_t>(1, buffer.line_count())).size());
	const float gutter_width = show_line_numbers ? fnt_view->width(std::string(line_digits, '0'), scale) + pad * 2.f : 0.f;

	const float left_inset = show_line_numbers ? gutter_width : pad;
	const float text_x = rect.left() + left_inset - state.scroll.x.offset;
	const float top_y = rect.top() - pad + state.scroll.y.offset;

	const std::size_t display_tab_width = std::clamp<std::size_t>(indent_width, 1, 16);
	const std::string indent_text = indent_with_spaces ? std::string(display_tab_width, ' ') : std::string(1, '\t');

	auto expand_from = [display_tab_width](std::string_view s, std::size_t start_col, std::string& scratch) -> std::string_view {
		if (s.find('\t') == std::string_view::npos) {
			return s;
		}
		scratch.clear();
		std::size_t col = start_col;
		for (const char c : s) {
			if (c == '\t') {
				const std::size_t pad = display_tab_width - col % display_tab_width;
				scratch.append(pad, ' ');
				col += pad;
			}
			else {
				scratch.push_back(c);
				++col;
			}
		}
		return scratch;
	};

	auto line_column_x = [&](const std::string_view line) -> std::vector<float> {
		std::string expanded;
		std::vector<std::size_t> col_to_expanded(line.size() + 1);
		std::size_t display_col = 0;
		for (std::size_t i = 0; i < line.size(); ++i) {
			col_to_expanded[i] = expanded.size();
			if (line[i] == '\t') {
				const std::size_t pad = display_tab_width - display_col % display_tab_width;
				expanded.append(pad, ' ');
				display_col += pad;
			}
			else {
				expanded.push_back(line[i]);
				++display_col;
			}
		}
		col_to_expanded[line.size()] = expanded.size();

		const std::vector<float> expanded_offsets = fnt_view->caret_offsets(expanded, scale);
		std::vector<float> offsets(line.size() + 1);
		for (std::size_t k = 0; k <= line.size(); ++k) {
			offsets[k] = text_x + expanded_offsets[col_to_expanded[k]];
		}
		return offsets;
	};

	auto pick_position = [&](const vec2f mouse) -> buffer_position {
		return text_area_position_at(ctx, buffer, state, rect, show_line_numbers, indent_width, mouse);
	};

	auto classify_char = [](const char c) -> int {
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
			return 1;
		}
		if (c == ' ' || c == '\t') {
			return 0;
		}
		return 2;
	};
	auto word_range = [&](const buffer_position p) -> std::pair<buffer_position, buffer_position> {
		const std::string_view line = buffer.line(p.line);
		if (line.empty()) {
			return { { p.line, 0 }, { p.line, 0 } };
		}
		const auto size = static_cast<std::uint32_t>(line.size());
		std::uint32_t idx = std::min(p.column, size);
		if (idx >= size) {
			idx = size - 1;
		}
		const int cls = classify_char(line[idx]);
		std::uint32_t lo = idx;
		std::uint32_t hi = idx;
		while (lo > 0 && classify_char(line[lo - 1]) == cls) {
			--lo;
		}
		while (hi + 1 < size && classify_char(line[hi + 1]) == cls) {
			++hi;
		}
		return { { p.line, lo }, { p.line, hi + 1 } };
	};
	auto line_range = [&](const buffer_position p) -> std::pair<buffer_position, buffer_position> {
		if (p.line + 1 < static_cast<std::uint32_t>(buffer.line_count())) {
			return { { p.line, 0 }, { p.line + 1, 0 } };
		}
		return { { p.line, 0 }, { p.line, static_cast<std::uint32_t>(buffer.line(p.line).size()) } };
	};
	auto granular_range = [&](const int granularity, const buffer_position p) -> std::pair<buffer_position, buffer_position> {
		return granularity == 2 ? line_range(p) : word_range(p);
	};

	std::size_t sig = 14695981039346656037ull;
	auto fold = [&sig](const std::size_t value) {
		sig = (sig ^ value) * 1099511628211ull;
	};
	fold(buffer.line_count());
	fold(display_tab_width);
	for (std::size_t li = 0; li < buffer.line_count(); ++li) {
		fold(buffer.line(li).size());
	}
	fold(std::bit_cast<std::uint32_t>(scale));

	if (sig != state.width_sig) {
		std::size_t widest_cols = 0;
		std::string_view widest_line;
		for (std::size_t li = 0; li < buffer.line_count(); ++li) {
			const std::string_view row = buffer.line(li);
			std::size_t cols = 0;
			for (const char c : row) {
				if (c == '\t') {
					cols += display_tab_width - cols % display_tab_width;
				}
				else {
					++cols;
				}
			}
			if (cols > widest_cols) {
				widest_cols = cols;
				widest_line = row;
			}
		}
		std::string widest_scratch;
		state.widest_line_px = fnt_view->width(expand_from(widest_line, 0, widest_scratch), scale);
		state.width_sig = sig;
	}

	const float content_width = left_inset + state.widest_line_px + pad;
	const bool h_scrollable = content_width > rect.width();
	const float h_scrollbar_gutter = h_scrollable ? scroll_cfg.scrollbar_width : 0.f;
	const rectf text_hit_rect = rectf::from_position_size(
		{ rect.left(), rect.top() },
		{ std::max(0.f, rect.width() - scrollbar_gutter), std::max(0.f, rect.height() - h_scrollbar_gutter) }
	);

	const bool hovered = ctx.hovers(rect);
	if (hovered) {
		hot_widget_id = widget_id;
	}

	if (ctx.mouse_pressed_for(text_hit_rect)) {
		focus_widget_id = widget_id;

		const vec2f mouse = ctx.mouse_position();
		const buffer_position pos = pick_position(mouse);
		interaction::register_click(state.click, mouse);
		state.select_origin = pos;

		if (state.click.count >= 2) {
			state.select_granularity = state.click.count == 3 ? 2 : 1;
			const auto [lo, hi] = granular_range(state.select_granularity, pos);
			state.anchor = lo;
			state.caret = hi;
		}
		else {
			state.select_granularity = 0;
			state.caret = pos;
			state.anchor = pos;
		}

		state.selecting = true;
		state.last_blink = system_clock::now<time>();
		state.blink_on = true;
		state.last_edit_kind = 0;
	}

	if (state.selecting) {
		if (ctx.mouse_held()) {
			const buffer_position current = pick_position(ctx.mouse_position());
			if (state.select_granularity == 0) {
				state.caret = current;
			}
			else {
				const auto [anchor_lo, anchor_hi] = granular_range(state.select_granularity, state.select_origin);
				const auto [current_lo, current_hi] = granular_range(state.select_granularity, current);
				if (current < state.select_origin) {
					state.anchor = anchor_hi;
					state.caret = current_lo;
				}
				else {
					state.anchor = anchor_lo;
					state.caret = current_hi;
				}
			}
			state.last_blink = system_clock::now<time>();
			state.blink_on = true;
		}
		else {
			state.selecting = false;
		}
	}

	if (state.context_menu_tag.exists() && ctx.mouse_pressed_for(text_hit_rect, mouse_button::button_2)) {
		focus_widget_id = widget_id;
		const vec2f menu_pos = ctx.mouse_position();
		const buffer_position click_pos = pick_position(menu_pos);
		buffer_position sel_lo = state.anchor;
		buffer_position sel_hi = state.caret;
		if (sel_hi.line < sel_lo.line || (sel_hi.line == sel_lo.line && sel_hi.column < sel_lo.column)) {
			std::swap(sel_lo, sel_hi);
		}
		const bool had_selection = state.anchor != state.caret;
		const bool before_lo = click_pos.line < sel_lo.line || (click_pos.line == sel_lo.line && click_pos.column < sel_lo.column);
		const bool after_hi = click_pos.line > sel_hi.line || (click_pos.line == sel_hi.line && click_pos.column > sel_hi.column);
		if (!had_selection || before_lo || after_hi) {
			state.caret = click_pos;
			state.anchor = click_pos;
			state.last_blink = system_clock::now<time>();
			state.blink_on = true;
		}
		const bool selection_now = state.anchor != state.caret;
		std::vector<menu_item> items{
			{
				.label = "Copy",
				.action_id = static_cast<std::uint32_t>(text_edit_action::copy),
				.enabled = selection_now,
			},
			{
				.label = "Cut",
				.action_id = static_cast<std::uint32_t>(text_edit_action::cut),
				.enabled = selection_now && !read_only,
			},
			{
				.label = "Paste",
				.action_id = static_cast<std::uint32_t>(text_edit_action::paste),
				.enabled = !read_only && !ctx.clipboard().empty(),
			},
		};
		ctx.open_context_menu({
			.position = menu_pos,
			.items = std::move(items),
			.tag = state.context_menu_tag,
		});
	}

	const bool focused = (focus_widget_id == widget_id);

	auto begin_edit = [&](const int kind) {
		if (state.last_edit_kind != kind || state.undo_stack.empty()) {
			state.undo_stack.push_back({ buffer.lines, state.caret, state.anchor });
			state.redo_stack.clear();
		}
		state.last_edit_kind = kind;
	};
	auto has_selection = [&]() -> bool {
		return state.anchor != state.caret;
	};
	auto normalized_sel = [&]() -> std::pair<buffer_position, buffer_position> {
		buffer_position a = state.anchor;
		buffer_position b = state.caret;
		if (b.line < a.line || (b.line == a.line && b.column < a.column)) {
			std::swap(a, b);
		}
		return { a, b };
	};
	auto delete_selection = [&]() {
		const auto [lo, hi] = normalized_sel();
		buffer.erase(lo, hi);
		state.caret = lo;
		state.anchor = lo;
	};
	auto selection_string = [&]() -> std::string {
		const auto [a, b] = normalized_sel();
		if (a.line == b.line) {
			const std::string_view row = buffer.line(a.line);
			const std::size_t lo = std::min<std::size_t>(a.column, row.size());
			const std::size_t hi = std::min<std::size_t>(b.column, row.size());
			return std::string(row.substr(lo, hi - lo));
		}
		std::string out;
		const std::string_view first = buffer.line(a.line);
		out += first.substr(std::min<std::size_t>(a.column, first.size()));
		out += '\n';
		for (std::uint32_t l = a.line + 1; l < b.line; ++l) {
			out += buffer.line(l);
			out += '\n';
		}
		const std::string_view last = buffer.line(b.line);
		out += last.substr(0, std::min<std::size_t>(b.column, last.size()));
		return out;
	};
	auto is_indent_char = [](const char c) -> bool {
		return c == ' ' || c == '\t';
	};
	auto display_column = [&](const std::string_view s) -> std::size_t {
		std::size_t col = 0;
		for (const char c : s) {
			if (c == '\t') {
				col += display_tab_width - col % display_tab_width;
			}
			else {
				++col;
			}
		}
		return col;
	};
	auto leading_indent = [&](const std::string_view line) -> std::string {
		std::size_t end = 0;
		while (end < line.size() && is_indent_char(line[end])) {
			++end;
		}
		return std::string(line.substr(0, end));
	};
	auto line_wants_extra_indent = [&](const std::string_view line, const std::uint32_t column) -> bool {
		std::size_t end = std::min<std::size_t>(column, line.size());
		while (end > 0 && is_indent_char(line[end - 1])) {
			--end;
		}
		if (end == 0) {
			return false;
		}
		const char c = line[end - 1];
		return c == '{' || c == '(' || c == '[';
	};
	auto newline_text = [&]() -> std::string {
		std::string text = "\n";
		if (auto_indent) {
			const std::string_view row = buffer.line(state.caret.line);
			text += leading_indent(row);
			if (line_wants_extra_indent(row, state.caret.column)) {
				text += indent_text;
			}
		}
		return text;
	};
	auto selected_line_range = [&]() -> std::pair<std::uint32_t, std::uint32_t> {
		const auto [lo, hi] = normalized_sel();
		std::uint32_t last = hi.line;
		if (hi.column == 0 && hi.line > lo.line) {
			--last;
		}
		return { lo.line, last };
	};
	auto adjust_after_prefix_insert = [](buffer_position p, const std::uint32_t first, const std::uint32_t last, const std::uint32_t count) -> buffer_position {
		if (p.line >= first && p.line <= last) {
			p.column += count;
		}
		return p;
	};
	auto tab_insert_text = [&]() -> std::string {
		if (!indent_with_spaces) {
			return indent_text;
		}
		const std::string_view row = buffer.line(state.caret.line);
		const std::size_t column = std::min<std::size_t>(state.caret.column, row.size());
		const std::size_t display_col = display_column(row.substr(0, column));
		const std::size_t space_count = display_tab_width - display_col % display_tab_width;
		return std::string(space_count, ' ');
	};
	auto indent_lines = [&](const std::uint32_t first, const std::uint32_t last) {
		if (buffer.lines.empty()) {
			buffer.lines.emplace_back();
		}
		for (std::uint32_t line = first; line <= last; ++line) {
			buffer.lines[line].insert(0, indent_text);
		}
		const auto count = static_cast<std::uint32_t>(indent_text.size());
		state.anchor = adjust_after_prefix_insert(state.anchor, first, last, count);
		state.caret = adjust_after_prefix_insert(state.caret, first, last, count);
	};
	auto line_outdent_count = [&](const std::string_view line) -> std::size_t {
		if (line.empty()) {
			return 0;
		}
		if (line.front() == '\t') {
			return 1;
		}
		std::size_t count = 0;
		while (count < line.size() && count < display_tab_width && line[count] == ' ') {
			++count;
		}
		return count;
	};
	auto outdent_lines = [&](const std::uint32_t first, const std::uint32_t last) -> bool {
		if (buffer.lines.empty()) {
			buffer.lines.emplace_back();
		}
		std::vector<std::uint32_t> counts;
		counts.reserve(static_cast<std::size_t>(last - first) + 1);
		bool any = false;
		for (std::uint32_t line = first; line <= last; ++line) {
			const auto count = static_cast<std::uint32_t>(line_outdent_count(buffer.line(line)));
			counts.push_back(count);
			any = any || count > 0;
		}
		if (!any) {
			return false;
		}
		for (std::uint32_t line = first; line <= last; ++line) {
			buffer.lines[line].erase(0, counts[static_cast<std::size_t>(line - first)]);
		}
		auto adjust = [&](buffer_position p) -> buffer_position {
			if (p.line >= first && p.line <= last) {
				const std::uint32_t count = counts[static_cast<std::size_t>(p.line - first)];
				p.column = p.column > count ? p.column - count : 0;
			}
			return p;
		};
		state.anchor = adjust(state.anchor);
		state.caret = adjust(state.caret);
		return true;
	};
	auto auto_outdent_before_closing_brace = [&]() {
		if (!auto_indent || has_selection() || state.caret.line >= buffer.lines.size()) {
			return;
		}
		std::string& row = buffer.lines[state.caret.line];
		const std::size_t column = std::min<std::size_t>(state.caret.column, row.size());
		const std::string_view prefix(row.data(), column);
		if (!std::ranges::all_of(prefix, is_indent_char)) {
			return;
		}
		const auto count = static_cast<std::uint32_t>(line_outdent_count(row));
		if (count == 0 || count > state.caret.column) {
			return;
		}
		row.erase(0, count);
		state.caret.column -= count;
		state.anchor = state.caret;
	};

	if (state.pending_action != text_edit_action::none) {
		if (state.pending_action == text_edit_action::copy) {
			if (has_selection()) {
				ctx.set_clipboard(selection_string());
			}
		}
		else if (state.pending_action == text_edit_action::cut) {
			if (!read_only && has_selection()) {
				ctx.set_clipboard(selection_string());
				begin_edit(2);
				delete_selection();
				modified = true;
				caret_moved = true;
			}
		}
		else if (state.pending_action == text_edit_action::paste) {
			if (!read_only) {
				if (std::string paste = ctx.clipboard(); !paste.empty()) {
					begin_edit(1);
					if (has_selection()) {
						delete_selection();
					}
					state.caret = buffer.insert(state.caret, paste);
					state.anchor = state.caret;
					modified = true;
					caret_moved = true;
				}
			}
		}
		if (modified) {
			state.caret = buffer.clamp(state.caret);
			state.anchor = buffer.clamp(state.anchor);
			state.last_blink = system_clock::now<time>();
			state.blink_on = true;
		}
		state.pending_action = text_edit_action::none;
	}

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

		auto is_word_char = [](const char c) -> bool {
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
		};
		auto is_ws = [](const char c) -> bool {
			return c == ' ' || c == '\t';
		};
		auto word_left = [&](const buffer_position p) -> buffer_position {
			if (p.column == 0) {
				return pos_left(p);
			}
			const std::string_view line = buffer.line(p.line);
			std::uint32_t c = p.column;
			while (c > 0 && is_ws(line[c - 1])) {
				--c;
			}
			if (c > 0 && is_word_char(line[c - 1])) {
				while (c > 0 && is_word_char(line[c - 1])) {
					--c;
				}
			}
			else {
				while (c > 0 && !is_word_char(line[c - 1]) && !is_ws(line[c - 1])) {
					--c;
				}
			}
			return { p.line, c };
		};
		auto word_right = [&](const buffer_position p) -> buffer_position {
			const std::string_view line = buffer.line(p.line);
			if (p.column >= line.size()) {
				return pos_right(p);
			}
			std::uint32_t c = p.column;
			while (c < line.size() && is_ws(line[c])) {
				++c;
			}
			if (c < line.size() && is_word_char(line[c])) {
				while (c < line.size() && is_word_char(line[c])) {
					++c;
				}
			}
			else {
				while (c < line.size() && !is_word_char(line[c]) && !is_ws(line[c])) {
					++c;
				}
			}
			return { p.line, c };
		};

		const bool ctrl = ctx.key_held(key::left_control) || ctx.key_held(key::right_control);

		bool changed = false;
		auto move_to = [&](const buffer_position p) {
			state.caret = buffer.clamp(p);
			state.anchor = state.caret;
			changed = true;
			state.last_edit_kind = 0;
		};
		if (ctx.key_pressed_for(key::left)) {
			move_to(ctrl ? word_left(state.caret) : pos_left(state.caret));
		}
		if (ctx.key_pressed_for(key::right)) {
			move_to(ctrl ? word_right(state.caret) : pos_right(state.caret));
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

		if (ctrl && ctx.key_pressed_for(key::c) && has_selection()) {
			ctx.set_clipboard(selection_string());
		}

		if (ctrl && ctx.key_pressed_for(key::a)) {
			const std::uint32_t last_line = buffer.line_count() > 0 ? static_cast<std::uint32_t>(buffer.line_count() - 1) : 0;
			state.anchor = { 0, 0 };
			state.caret = buffer.clamp({ last_line, line_len(last_line) });
			state.last_edit_kind = 0;
		}

		if (!read_only) {
			if (ctrl && ctx.key_pressed_for(key::z)) {
				if (!state.undo_stack.empty()) {
					state.redo_stack.push_back({ buffer.lines, state.caret, state.anchor });
					text_edit_snapshot snap = std::move(state.undo_stack.back());
					state.undo_stack.pop_back();
					buffer.lines = std::move(snap.lines);
					state.caret = buffer.clamp(snap.caret);
					state.anchor = buffer.clamp(snap.anchor);
					state.last_edit_kind = 0;
					changed = true;
					modified = true;
				}
			}
			else if (ctrl && ctx.key_pressed_for(key::y)) {
				if (!state.redo_stack.empty()) {
					state.undo_stack.push_back({ buffer.lines, state.caret, state.anchor });
					text_edit_snapshot snap = std::move(state.redo_stack.back());
					state.redo_stack.pop_back();
					buffer.lines = std::move(snap.lines);
					state.caret = buffer.clamp(snap.caret);
					state.anchor = buffer.clamp(snap.anchor);
					state.last_edit_kind = 0;
					changed = true;
					modified = true;
				}
			}

			if (ctrl && ctx.key_pressed_for(key::x) && has_selection()) {
				ctx.set_clipboard(selection_string());
				begin_edit(2);
				delete_selection();
				changed = true;
				modified = true;
			}
			if (ctrl && ctx.key_pressed_for(key::v)) {
				std::string paste = ctx.clipboard();
				if (!paste.empty()) {
					begin_edit(1);
					if (has_selection()) {
						delete_selection();
					}
					state.caret = buffer.insert(state.caret, paste);
					state.anchor = state.caret;
					changed = true;
					modified = true;
				}
			}

			const bool shift = ctx.key_held(key::left_shift) || ctx.key_held(key::right_shift);
			bool handled_tab = false;

			if (!ctrl && ctx.key_pressed_for(key::tab)) {
				handled_tab = true;
				if (shift) {
					const auto [first, last] = has_selection() ? selected_line_range() : std::pair{ state.caret.line, state.caret.line };
					bool can_outdent = false;
					for (std::uint32_t line = first; line <= last; ++line) {
						can_outdent = can_outdent || line_outdent_count(buffer.line(line)) > 0;
					}
					if (can_outdent) {
						begin_edit(3);
						outdent_lines(first, last);
						changed = true;
						modified = true;
					}
				}
				else if (has_selection()) {
					const auto [first, last] = selected_line_range();
					begin_edit(3);
					indent_lines(first, last);
					changed = true;
					modified = true;
				}
				else {
					begin_edit(3);
					state.caret = buffer.insert(state.caret, tab_insert_text());
					state.anchor = state.caret;
					changed = true;
					modified = true;
				}
			}

			if (const std::string_view entered = ctx.text_entered(); !ctrl && !handled_tab && !entered.empty()) {
				begin_edit(1);
				if (has_selection()) {
					delete_selection();
				}
				if (entered == "}") {
					auto_outdent_before_closing_brace();
				}
				state.caret = buffer.insert(state.caret, entered);
				state.anchor = state.caret;
				changed = true;
				modified = true;
			}
			if (ctx.key_pressed_for(key::enter)) {
				begin_edit(1);
				if (has_selection()) {
					delete_selection();
				}
				state.caret = buffer.insert(state.caret, newline_text());
				state.anchor = state.caret;
				changed = true;
				modified = true;
			}
			if (ctx.key_pressed_for(key::backspace)) {
				if (has_selection()) {
					begin_edit(2);
					delete_selection();
					changed = true;
					modified = true;
				}
				else if (const buffer_position from = ctrl ? word_left(state.caret) : pos_left(state.caret); from != state.caret) {
					begin_edit(2);
					buffer.erase(from, state.caret);
					state.caret = from;
					state.anchor = from;
					changed = true;
					modified = true;
				}
			}
			if (ctx.key_pressed_for(key::del)) {
				if (has_selection()) {
					begin_edit(2);
					delete_selection();
					changed = true;
					modified = true;
				}
				else if (const buffer_position to = ctrl ? word_right(state.caret) : pos_right(state.caret); to != state.caret) {
					begin_edit(2);
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
			caret_moved = true;
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
	});

	if (gutter_width > 0.f) {
		ctx.queue_sprite({
			.rect = rectf::from_position_size({ rect.left(), rect.top() }, { gutter_width, rect.height() }),
			.color = ctx.style.color_panel_alt,
			.texture = ctx.blank_texture,
			.clip_rect = rect,
		});
	}

	const rectf content_clip = gutter_width > 0.f
		? rectf::from_position_size({ rect.left() + gutter_width, rect.top() }, { std::max(0.f, rect.width() - gutter_width), rect.height() })
		: rect;

	const float view_height = std::max(0.f, rect.height() - pad * 2.f);
	const int line_total = static_cast<int>(buffer.line_count());
	const int first_line = std::max(0, static_cast<int>(state.scroll.y.offset / line_h) - 1);
	const int last_line = std::min(line_total, first_line + static_cast<int>(view_height / line_h) + 3);
	buffer_position sel_lo = state.anchor;
	buffer_position sel_hi = state.caret;
	const bool has_sel = sel_lo != sel_hi;
	if (sel_hi.line < sel_lo.line || (sel_hi.line == sel_lo.line && sel_hi.column < sel_lo.column)) {
		std::swap(sel_lo, sel_hi);
	}
	if (has_sel) {
		for (int i = std::max(first_line, static_cast<int>(sel_lo.line)); i <= std::min(last_line - 1, static_cast<int>(sel_hi.line)); ++i) {
			const std::string_view sline = buffer.line(static_cast<std::uint32_t>(i));
			const std::size_t col_a = static_cast<std::uint32_t>(i) == sel_lo.line ? std::min<std::size_t>(sel_lo.column, sline.size()) : 0;
			const std::size_t col_b = static_cast<std::uint32_t>(i) == sel_hi.line ? std::min<std::size_t>(sel_hi.column, sline.size()) : sline.size();
			const std::vector<float> offsets = line_column_x(sline);
			const float x_a = offsets[col_a];
			const float x_b = offsets[col_b];
			const float extra = static_cast<std::uint32_t>(i) < sel_hi.line ? fnt_view->width(" ", scale) : 0.f;
			const float sel_center = top_y - static_cast<float>(i) * line_h - line_h * 0.5f;
			ctx.queue_sprite({
				.rect = rectf::from_position_size({ x_a, sel_center + line_h * 0.5f }, { x_b - x_a + extra, line_h }),
				.color = ctx.style.color_selection,
				.texture = ctx.blank_texture,
				.clip_rect = content_clip,
			});
		}
	}

	std::size_t span_cursor = 0;
	std::string run_scratch;
	for (int i = first_line; i < last_line; ++i) {
		const std::string_view line = buffer.line(static_cast<std::uint32_t>(i));
		const float line_center = top_y - static_cast<float>(i) * line_h - line_h * 0.5f;
		if (show_line_numbers) {
			const std::string num = std::to_string(i + 1);
			ctx.queue_text({
				.font = fnt,
				.text = num,
				.position = { rect.left() + gutter_width - pad - fnt_view->width(num, scale), line_center + fnt_view->vertical_center_offset(scale) },
				.scale = scale,
				.color = ctx.style.color_text_disabled,
				.clip_rect = rect,
			});
		}
		const auto line_no = static_cast<std::uint32_t>(i);
		while (span_cursor < spans.size() && spans[span_cursor].line < line_no) {
			++span_cursor;
		}
		std::size_t line_span_end = span_cursor;
		while (line_span_end < spans.size() && spans[line_span_end].line == line_no) {
			++line_span_end;
		}

		if (!line.empty()) {
			std::size_t col = 0;
			std::size_t disp = 0;
			float run_x = text_x;
			std::array<const text_fade*, 8> line_fades{};
			std::size_t line_fade_count = 0;
			for (const text_fade& f : fades) {
				if (f.line == line_no && line_fade_count < line_fades.size()) {
					line_fades[line_fade_count++] = &f;
				}
			}
			const bool line_faded = line_fade_count > 0;
			auto fade_alpha = [&](std::size_t c) -> float {
				float alpha = 1.f;
				for (std::size_t k = 0; k < line_fade_count; ++k) {
					if (c >= line_fades[k]->start_col && c < line_fades[k]->end_col) {
						alpha = std::min(alpha, line_fades[k]->alpha);
					}
				}
				return alpha;
			};
			auto emit_run = [&](std::size_t a, std::size_t b, const vec4f& color) {
				const std::string_view seg = expand_from(line.substr(a, b - a), disp, run_scratch);
				ctx.queue_text({
					.font = fnt,
					.text = seg,
					.position = { run_x, line_center + fnt_view->vertical_center_offset(scale) },
					.scale = scale,
					.color = color,
					.clip_rect = content_clip,
				});
				run_x += fnt_view->width(seg, scale);
				disp += seg.size();
			};
			auto draw_run = [&](std::size_t a, std::size_t b, const vec4f& color) {
				if (b <= a) {
					return;
				}
				if (!line_faded) {
					emit_run(a, b, color);
				}
				else {
					std::size_t cursor = a;
					while (cursor < b) {
						const float alpha = fade_alpha(cursor);
						std::size_t split = cursor + 1;
						while (split < b && fade_alpha(split) == alpha) {
							++split;
						}
						vec4f faded = color;
						faded.w() *= alpha;
						emit_run(cursor, split, faded);
						cursor = split;
					}
				}
				col = b;
			};
			for (std::size_t k = span_cursor; k < line_span_end; ++k) {
				const text_span& sp = spans[k];
				const std::size_t a = std::min<std::size_t>(sp.start_col, line.size());
				const std::size_t b = std::min<std::size_t>(sp.end_col, line.size());
				if (a > col) {
					draw_run(col, a, ctx.style.color_text);
				}
				draw_run(std::max(col, a), b, sp.color);
			}
			if (col < line.size()) {
				draw_run(col, line.size(), ctx.style.color_text);
			}
		}

		std::optional<std::vector<float>> underline_offsets;
		for (const text_underline& u : underlines) {
			if (u.line != line_no) {
				continue;
			}
			if (!underline_offsets) {
				underline_offsets = line_column_x(line);
			}
			const std::size_t a = std::min<std::size_t>(u.start_col, line.size());
			const std::size_t b = std::min<std::size_t>(std::max<std::size_t>(a + 1, u.end_col), line.size());
			const float x0 = (*underline_offsets)[a];
			const float x1 = (*underline_offsets)[b];
			ctx.queue_sprite({
				.rect = rectf::from_position_size({ x0, line_center - line_h * 0.35f }, { std::max(x1 - x0, 3.f), 2.f }),
				.color = u.color,
				.texture = ctx.blank_texture,
				.clip_rect = content_clip,
			});
		}

		span_cursor = line_span_end;
	}

	if (focused && state.blink_on) {
		const std::string_view caret_line = buffer.line(state.caret.line);
		const std::vector<float> caret_col_x = line_column_x(caret_line);
		const float caret_x = caret_col_x[std::min<std::size_t>(state.caret.column, caret_line.size())];
		const float caret_top = top_y - static_cast<float>(state.caret.line) * line_h - line_h * 0.5f + fnt_view->vertical_center_offset(scale);
		const float baseline = caret_top - fnt_view->ascender_height(scale);
		const float ink_top = fnt_view->max_glyph_top(scale);
		const float ink_bottom = fnt_view->min_glyph_bottom(scale);
		ctx.queue_sprite({
			.rect = rectf::from_position_size({ caret_x, baseline + ink_top }, { 2.f, ink_top - ink_bottom }),
			.color = ctx.style.color_caret,
			.texture = ctx.blank_texture,
			.clip_rect = content_clip,
		});
	}

	if (caret_moved) {
		const float caret_y = static_cast<float>(state.caret.line) * line_h;
		if (caret_y < state.scroll.y.offset) {
			state.scroll.y.offset = caret_y;
			state.scroll.y.target = caret_y;
		}
		else if (caret_y + line_h > state.scroll.y.offset + view_height) {
			const float target = caret_y + line_h - view_height;
			state.scroll.y.offset = target;
			state.scroll.y.target = target;
		}

		const std::string_view caret_row = buffer.line(state.caret.line);
		const std::vector<float> caret_row_x = line_column_x(caret_row);
		const float caret_x_content = left_inset + caret_row_x[std::min<std::size_t>(state.caret.column, caret_row.size())] - text_x;
		if (caret_x_content - pad - left_inset < state.scroll.x.offset) {
			const float target = std::max(0.f, caret_x_content - pad - left_inset);
			state.scroll.x.offset = target;
			state.scroll.x.target = target;
		}
		else if (caret_x_content + pad > state.scroll.x.offset + rect.width()) {
			const float target = caret_x_content + pad - rect.width();
			state.scroll.x.offset = target;
			state.scroll.x.target = target;
		}
	}

	const float content_height = static_cast<float>(buffer.line_count()) * line_h + pad * 2.f;
	scroll_area(ctx, state.scroll, rect, { content_width, content_height }, scroll_cfg);

	return modified;
}
