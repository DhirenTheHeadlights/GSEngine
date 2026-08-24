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
import gse.meta;
import gse.assert;

import :types;
import :ids;
import :styles;
import :builder;
import :text_buffer;
import :font;
import :interaction;
import :symbols;

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
		bool tail_pinned = true;
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
		std::vector<float> line_tops;
		std::uint64_t metrics_sig = 0;
		text_edit_action pending_action = text_edit_action::none;
		id context_menu_tag{};
	};

	struct text_area_layout {
		float line_height = 0.f;
		float top = 0.f;
		float text_left = 0.f;
		float view_width = 0.f;
		std::uint32_t line_count = 0;
		std::span<const float> line_tops{};

		[[nodiscard]] auto line_top(
			std::uint32_t line
		) const -> float;

		[[nodiscard]] auto line_extent(
			std::uint32_t line
		) const -> float;

		[[nodiscard]] auto content_extent() const -> float;

		[[nodiscard]] auto line_at(
			float offset
		) const -> std::uint32_t;
	};

	struct text_area {
		using result = void;

		struct params {
			text_buffer& buffer;
			text_area_state& state;
			std::span<const text_span> spans{};
			std::span<const text_underline> underlines{};
			std::span<const text_fade> fades{};
			std::span<const text_block> blocks{};
			std::span<const text_stop> stops{};
			std::optional<rectf> rect{};
			bool read_only = false;
			bool follow_tail = false;
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

export namespace gse::gui {
	struct text_area_geometry {
		const text_buffer& buffer;
		const text_area_state& state;
		rectf rect;
		std::span<const text_span> spans{};
		std::span<const text_stop> stops{};
		std::span<const text_block> blocks{};
		bool show_line_numbers = false;
		std::size_t indent_width = 4;
		resource::handle<font> font{};
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
		const text_area_geometry& geometry,
		vec2f mouse
	) -> buffer_position;

	auto text_area_line_height(
		const draw_context& ctx,
		resource::handle<font> font = {}
	) -> float;

	auto text_area_layout_of(
		const draw_context& ctx,
		const text_area_geometry& geometry
	) -> text_area_layout;

	auto follow_tail_button(
		builder& b,
		const rectf& area,
		text_area_state& state,
		id widget_id
	) -> interaction::press;
}

namespace gse::gui {
	struct block_layout {
		float offset = 0.f;
		float width = 0.f;
	};

	struct styled_run {
		std::size_t start = 0;
		std::size_t end = 0;
		vec4f color;
		resource::handle<font> handle;
		std::shared_ptr<const font> view;
		float scale = 1.f;
	};

	struct run_context {
		const font_set& fonts;
		resource::handle<font> inherited;
		std::shared_ptr<const font> base_view;
		vec4f base_color;
		float base_scale = 1.f;
		std::size_t tab_width = 4;
	};

	struct line_layout {
		std::vector<float> offsets;
		std::vector<std::size_t> display;
		std::string expanded;
		std::vector<std::size_t> to_expanded;
	};

	auto expanded_width(
		const font& face,
		std::string_view line,
		float scale,
		std::size_t tab_width
	) -> float;

	auto block_at_line(
		std::span<const text_block> blocks,
		std::uint32_t line
	) -> std::size_t;

	auto measure_block(
		const text_buffer& buffer,
		const text_block& block,
		std::span<const text_span> spans,
		std::span<const text_stop> stops,
		const run_context& style,
		float view_width
	) -> block_layout;

	auto spans_for_line(
		std::span<const text_span> spans,
		std::uint32_t line
	) -> std::span<const text_span>;

	auto stops_for_line(
		std::span<const text_stop> stops,
		std::uint32_t line
	) -> std::span<const text_stop>;

	auto build_runs(
		std::string_view line,
		std::span<const text_span> line_spans,
		const run_context& style,
		std::vector<styled_run>& out
	) -> void;

	auto layout_line(
		std::string_view line,
		std::span<const styled_run> runs,
		std::span<const text_stop> line_stops,
		std::size_t tab_width,
		line_layout& out
	) -> void;

	auto line_scale_of(
		std::span<const text_span> line_spans
	) -> float;

	auto styled_spans(
		std::span<const text_span> spans
	) -> bool;

	auto metrics_signature(
		const text_buffer& buffer,
		std::span<const text_span> spans,
		std::span<const text_stop> stops,
		std::size_t tab_width,
		float scale
	) -> std::uint64_t;

	auto refresh_metrics(
		const text_buffer& buffer,
		text_area_state& state,
		std::span<const text_span> spans,
		std::span<const text_stop> stops,
		const run_context& style,
		float base_height
	) -> void;
}

auto gse::gui::expanded_width(const font& face, const std::string_view line, const float scale, const std::size_t tab_width) -> float {
	if (line.find('\t') == std::string_view::npos) {
		return face.width(line, scale);
	}

	std::string expanded;
	std::size_t column = 0;
	for (const char c : line) {
		if (c == '\t') {
			const std::size_t fill = tab_width - column % tab_width;
			expanded.append(fill, ' ');
			column += fill;
		}
		else {
			expanded.push_back(c);
			++column;
		}
	}
	return face.width(expanded, scale);
}

auto gse::gui::block_at_line(const std::span<const text_block> blocks, const std::uint32_t line) -> std::size_t {
	const auto above = std::ranges::upper_bound(blocks, line, {}, &text_block::first_line);
	if (above == blocks.begin()) {
		return blocks.size();
	}

	const auto found = std::prev(above);
	return line <= found->last_line ? static_cast<std::size_t>(std::distance(blocks.begin(), found)) : blocks.size();
}

auto gse::gui::spans_for_line(const std::span<const text_span> spans, const std::uint32_t line) -> std::span<const text_span> {
	const auto found = std::ranges::equal_range(spans, line, {}, &text_span::line);
	return { found.begin(), found.end() };
}

auto gse::gui::stops_for_line(const std::span<const text_stop> stops, const std::uint32_t line) -> std::span<const text_stop> {
	const auto found = std::ranges::equal_range(stops, line, {}, &text_stop::line);
	return { found.begin(), found.end() };
}

auto gse::gui::build_runs(const std::string_view line, const std::span<const text_span> line_spans, const run_context& style, std::vector<styled_run>& out) -> void {
	out.clear();

	auto push = [&out, &style](const std::size_t a, const std::size_t b, const vec4f& color, const text_face which, const float scale) {
		if (b <= a) {
			return;
		}
		const bool inherited = which == text_face::inherit;
		resource::handle<font> handle = inherited ? style.inherited : style.fonts.face(which, style.inherited);
		std::shared_ptr<const font> view = inherited ? style.base_view : handle.resolve();
		out.push_back({
			.start = a,
			.end = b,
			.color = color,
			.handle = std::move(handle),
			.view = std::move(view),
			.scale = style.base_scale * scale,
		});
	};

	std::size_t col = 0;
	for (const text_span& sp : line_spans) {
		const std::size_t a = std::min<std::size_t>(sp.start_col, line.size());
		const std::size_t b = std::min<std::size_t>(sp.end_col, line.size());
		if (a > col) {
			push(col, a, style.base_color, text_face::inherit, 1.f);
		}
		push(std::max(col, a), b, sp.color, sp.face, sp.scale);
		col = std::max(col, b);
	}
	if (col < line.size()) {
		push(col, line.size(), style.base_color, text_face::inherit, 1.f);
	}
}

auto gse::gui::layout_line(const std::string_view line, const std::span<const styled_run> runs, const std::span<const text_stop> line_stops, const std::size_t tab_width, line_layout& out) -> void {
	out.offsets.assign(line.size() + 1, 0.f);
	out.display.assign(line.size() + 1, 0);

	auto stop_x = [line_stops](const std::size_t column) -> std::optional<float> {
		const auto found = std::ranges::find(line_stops, static_cast<std::uint32_t>(column), &text_stop::column);
		return found == line_stops.end() ? std::nullopt : std::optional(found->x);
	};

	float x = 0.f;
	std::size_t display_col = 0;

	for (std::size_t first = 0; first < runs.size();) {
		std::size_t last = first + 1;
		while (last < runs.size()
			&& runs[last].view == runs[first].view
			&& runs[last].scale == runs[first].scale
			&& !stop_x(runs[last].start)) {
			++last;
		}

		const std::size_t from = runs[first].start;
		const std::size_t to = runs[last - 1].end;
		if (const std::optional<float> forced = stop_x(from)) {
			x = *forced;
		}

		out.expanded.clear();
		out.to_expanded.assign(to - from + 1, 0);
		std::size_t column = display_col;
		for (std::size_t i = from; i < to; ++i) {
			out.to_expanded[i - from] = out.expanded.size();
			if (line[i] == '\t') {
				const std::size_t fill = tab_width - column % tab_width;
				out.expanded.append(fill, ' ');
				column += fill;
			}
			else {
				out.expanded.push_back(line[i]);
				++column;
			}
		}
		out.to_expanded[to - from] = out.expanded.size();

		const std::vector<float> caret = runs[first].view->caret_offsets(out.expanded, runs[first].scale);
		for (std::size_t i = from; i <= to; ++i) {
			const std::size_t at = out.to_expanded[i - from];
			out.offsets[i] = x + caret[at];
			out.display[i] = display_col + at;
		}

		x += caret.back();
		display_col = column;
		first = last;
	}
}

auto gse::gui::line_scale_of(const std::span<const text_span> line_spans) -> float {
	return std::ranges::fold_left(line_spans | std::views::transform(&text_span::scale), 1.f, [](const float a, const float b) {
		return std::max(a, b);
	});
}

auto gse::gui::styled_spans(const std::span<const text_span> spans) -> bool {
	return std::ranges::any_of(spans, [](const text_span& sp) {
		return sp.face != text_face::inherit || sp.scale != 1.f;
	});
}

auto gse::gui::metrics_signature(const text_buffer& buffer, const std::span<const text_span> spans, const std::span<const text_stop> stops, const std::size_t tab_width, const float scale) -> std::uint64_t {
	std::uint64_t seed = hash_combine(0, buffer.line_count());
	seed = hash_combine(seed, tab_width);
	seed = hash_combine(seed, spans.size());
	seed = hash_combine(seed, stops.size());
	seed = hash_combine(seed, std::bit_cast<std::uint32_t>(scale));
	for (std::size_t i = 0; i < buffer.line_count(); ++i) {
		seed = hash_combine(seed, buffer.line(i).size());
	}
	return seed;
}

auto gse::gui::refresh_metrics(const text_buffer& buffer, text_area_state& state, const std::span<const text_span> spans, const std::span<const text_stop> stops, const run_context& style, const float base_height) -> void {
	const std::uint64_t signature = metrics_signature(buffer, spans, stops, style.tab_width, style.base_scale);
	if (signature == state.metrics_sig) {
		return;
	}
	state.metrics_sig = signature;

	const auto lines = static_cast<std::uint32_t>(buffer.line_count());

	if (stops.empty() && !styled_spans(spans)) {
		state.line_tops.clear();
		std::size_t widest_cols = 0;
		std::string_view widest_line;
		for (std::uint32_t i = 0; i < lines; ++i) {
			const std::string_view row = buffer.line(i);
			std::size_t cols = 0;
			for (const char c : row) {
				cols += c == '\t' ? style.tab_width - cols % style.tab_width : 1;
			}
			if (cols > widest_cols) {
				widest_cols = cols;
				widest_line = row;
			}
		}
		state.widest_line_px = expanded_width(*style.base_view, widest_line, style.base_scale, style.tab_width);
		return;
	}

	state.line_tops.assign(static_cast<std::size_t>(lines) + 1, 0.f);
	std::vector<styled_run> runs;
	line_layout layout;
	float widest = 0.f;
	float top = 0.f;
	for (std::uint32_t i = 0; i < lines; ++i) {
		const std::string_view row = buffer.line(i);
		const std::span<const text_span> line_spans = spans_for_line(spans, i);
		build_runs(row, line_spans, style, runs);
		layout_line(row, runs, stops_for_line(stops, i), style.tab_width, layout);
		widest = std::max(widest, layout.offsets.back());
		state.line_tops[i] = top;
		top += base_height * line_scale_of(line_spans);
	}
	state.line_tops[lines] = top;
	state.widest_line_px = widest;
}

auto gse::gui::measure_block(const text_buffer& buffer, const text_block& block, const std::span<const text_span> spans, const std::span<const text_stop> stops, const run_context& style, const float view_width) -> block_layout {
	const auto lines = static_cast<std::uint32_t>(buffer.line_count());
	std::vector<styled_run> runs;
	line_layout layout;
	float widest = 0.f;
	for (std::uint32_t i = block.first_line; i <= block.last_line && i < lines; ++i) {
		const std::string_view row = buffer.line(i);
		build_runs(row, spans_for_line(spans, i), style, runs);
		layout_line(row, runs, stops_for_line(stops, i), style.tab_width, layout);
		widest = std::max(widest, layout.offsets.back());
	}

	return {
		.offset = block.align_right ? std::max(0.f, view_width - widest) : 0.f,
		.width = widest,
	};
}

auto gse::gui::text_area_layout::line_top(const std::uint32_t line) const -> float {
	if (line_tops.empty()) {
		return static_cast<float>(line) * line_height;
	}
	return line_tops[std::min<std::size_t>(line, line_tops.size() - 1)];
}

auto gse::gui::text_area_layout::line_extent(const std::uint32_t line) const -> float {
	if (line_tops.size() < 2) {
		return line_height;
	}
	const std::size_t index = std::min<std::size_t>(line, line_tops.size() - 2);
	return line_tops[index + 1] - line_tops[index];
}

auto gse::gui::text_area_layout::content_extent() const -> float {
	if (line_tops.empty()) {
		return static_cast<float>(line_count) * line_height;
	}
	return line_tops.back();
}

auto gse::gui::text_area_layout::line_at(const float offset) const -> std::uint32_t {
	if (line_count == 0) {
		return 0;
	}
	const auto last = line_count - 1;
	if (line_tops.empty()) {
		const auto picked = static_cast<int>(offset / line_height);
		return static_cast<std::uint32_t>(std::clamp(picked, 0, static_cast<int>(last)));
	}

	const auto above = std::ranges::upper_bound(line_tops, offset);
	if (above == line_tops.begin()) {
		return 0;
	}
	return std::min(static_cast<std::uint32_t>(std::distance(line_tops.begin(), std::prev(above))), last);
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
			.blocks = p.blocks,
			.stops = p.stops,
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

auto gse::gui::draw::text_area_line_height(const draw_context& ctx, const resource::handle<font> font) -> float {
	const auto fnt = font.valid() ? font : ctx.fonts.code;
	return fnt.resolve()->line_height(ctx.style.font_size) * 1.25f;
}

auto gse::gui::draw::text_area_layout_of(const draw_context& ctx, const text_area_geometry& geometry) -> text_area_layout {
	const text_buffer& buffer = geometry.buffer;
	const auto fnt = geometry.font.valid() ? geometry.font : ctx.fonts.code;
	const auto fnt_view = fnt.resolve();
	const float scale = ctx.style.font_size;
	const float pad = ctx.style.padding;
	const float line_h = text_area_line_height(ctx, fnt);
	const std::size_t line_digits = std::max<std::size_t>(2, std::to_string(std::max<std::size_t>(1, buffer.line_count())).size());
	const float gutter_width = geometry.show_line_numbers ? fnt_view->width(std::string(line_digits, '0'), scale) + pad * 2.f : 0.f;
	const float left_inset = geometry.show_line_numbers ? gutter_width : pad;
	const scroll_config scroll_cfg{};

	const text_area_layout extent{
		.line_height = line_h,
		.line_count = static_cast<std::uint32_t>(buffer.line_count()),
		.line_tops = geometry.state.line_tops,
	};
	const bool scrollable = extent.content_extent() + pad * 2.f > geometry.rect.height();

	return {
		.line_height = line_h,
		.top = geometry.rect.top() - pad + geometry.state.scroll.y.offset,
		.text_left = geometry.rect.left() + left_inset - geometry.state.scroll.x.offset,
		.view_width = std::max(0.f, geometry.rect.width() - left_inset - pad - (scrollable ? scroll_cfg.scrollbar_width : 0.f)),
		.line_count = extent.line_count,
		.line_tops = extent.line_tops,
	};
}

auto gse::gui::draw::text_area_position_at(const draw_context& ctx, const text_area_geometry& geometry, const vec2f mouse) -> buffer_position {
	const text_buffer& buffer = geometry.buffer;
	const auto fnt = geometry.font.valid() ? geometry.font : ctx.fonts.code;
	const run_context style{
		.fonts = ctx.fonts,
		.inherited = fnt,
		.base_view = fnt.resolve(),
		.base_color = ctx.style.color_text,
		.base_scale = ctx.style.font_size,
		.tab_width = std::clamp<std::size_t>(geometry.indent_width, 1, 16),
	};

	const text_area_layout placement = text_area_layout_of(ctx, geometry);
	const std::uint32_t picked_line = placement.line_at(placement.top - mouse.y());
	const std::string_view line = buffer.line(picked_line);

	const std::size_t block = block_at_line(geometry.blocks, picked_line);
	const float block_x = block < geometry.blocks.size()
		? measure_block(buffer, geometry.blocks[block], geometry.spans, geometry.stops, style, placement.view_width).offset
		: 0.f;

	std::vector<styled_run> runs;
	line_layout columns;
	build_runs(line, spans_for_line(geometry.spans, picked_line), style, runs);
	layout_line(line, runs, stops_for_line(geometry.stops, picked_line), style.tab_width, columns);

	const float origin = placement.text_left + block_x;
	std::size_t picked_col = 0;
	float best_dx = std::numeric_limits<float>::max();
	for (std::size_t k = 0; k <= line.size(); ++k) {
		if (const float dx = std::abs(origin + columns.offsets[k] - mouse.x()); dx < best_dx) {
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
	const std::span<const text_block> blocks = params.blocks;
	const std::span<const text_stop> stops = params.stops;
	const rectf& rect = *params.rect;
	const bool read_only = params.read_only;
	const bool follow_tail = params.follow_tail;
	const bool show_line_numbers = params.show_line_numbers;
	const std::size_t indent_width = params.indent_width;
	const bool indent_with_spaces = params.indent_with_spaces;
	const bool auto_indent = params.auto_indent;
	const time blink_interval = params.blink_interval;
	const resource::handle<font> font = params.font;
	const auto fnt = font.valid() ? font : ctx.fonts.code;
	const auto fnt_view = fnt.resolve();

	bool modified = false;
	bool caret_moved = false;

	state.caret = buffer.clamp(state.caret);
	state.anchor = buffer.clamp(state.anchor);

	const float scale = ctx.style.font_size;
	const float pad = ctx.style.padding;
	const float line_h = text_area_line_height(ctx, fnt);
	const std::size_t display_tab_width = std::clamp<std::size_t>(indent_width, 1, 16);

	const run_context style{
		.fonts = ctx.fonts,
		.inherited = fnt,
		.base_view = fnt_view,
		.base_color = ctx.style.color_text,
		.base_scale = scale,
		.tab_width = display_tab_width,
	};

	refresh_metrics(buffer, state, spans, stops, style, line_h);

	const text_area_geometry geometry{
		.buffer = buffer,
		.state = state,
		.rect = rect,
		.spans = spans,
		.stops = stops,
		.blocks = blocks,
		.show_line_numbers = show_line_numbers,
		.indent_width = indent_width,
		.font = font,
	};
	text_area_layout placement = text_area_layout_of(ctx, geometry);

	const scroll_config scroll_cfg{};
	const bool scrollable = placement.content_extent() + pad * 2.f > rect.height();
	const float scrollbar_gutter = scrollable ? scroll_cfg.scrollbar_width : 0.f;

	const std::size_t line_digits = std::max<std::size_t>(2, std::to_string(std::max<std::size_t>(1, buffer.line_count())).size());
	const float gutter_width = show_line_numbers ? fnt_view->width(std::string(line_digits, '0'), scale) + pad * 2.f : 0.f;

	const float left_inset = show_line_numbers ? gutter_width : pad;
	float text_x = placement.text_left;
	float top_y = placement.top;

	const std::string indent_text = indent_with_spaces ? std::string(display_tab_width, ' ') : std::string(1, '\t');

	float view_width = placement.view_width;
	std::vector<std::optional<block_layout>> block_layouts(blocks.size());

	auto layout_of = [&](const std::size_t index) -> const block_layout& {
		if (!block_layouts[index]) {
			block_layouts[index] = measure_block(buffer, blocks[index], spans, stops, style, view_width);
		}
		return *block_layouts[index];
	};

	auto block_x = [&](const std::uint32_t line_no) -> float {
		const std::size_t index = block_at_line(blocks, line_no);
		return index < blocks.size() ? layout_of(index).offset : 0.f;
	};

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

	std::vector<styled_run> scratch_runs;
	line_layout scratch_columns;

	auto place_line = [&](const std::uint32_t line_no, const std::string_view line) -> const line_layout& {
		build_runs(line, spans_for_line(spans, line_no), style, scratch_runs);
		layout_line(line, scratch_runs, stops_for_line(stops, line_no), display_tab_width, scratch_columns);
		return scratch_columns;
	};

	auto line_column_x = [&](const std::uint32_t line_no, const std::string_view line) -> std::vector<float> {
		const line_layout& columns = place_line(line_no, line);
		const float origin = text_x + block_x(line_no);
		std::vector<float> offsets(line.size() + 1);
		for (std::size_t k = 0; k <= line.size(); ++k) {
			offsets[k] = origin + columns.offsets[k];
		}
		return offsets;
	};

	auto pick_position = [&](const vec2f mouse) -> buffer_position {
		return text_area_position_at(ctx, geometry, mouse);
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
			const vec2f drag_mouse = ctx.mouse_position();
			const float above_top = drag_mouse.y() - text_hit_rect.top();
			const float below_bottom = text_hit_rect.bottom() - drag_mouse.y();
			if (const float overshoot = below_bottom > 0.f ? below_bottom : (above_top > 0.f ? -above_top : 0.f); overshoot != 0.f) {
				const float rows = std::clamp(std::abs(overshoot) / line_h, 1.f, 8.f);
				const float span = placement.content_extent() + pad * 2.f;
				const float limit = std::max(0.f, span - rect.height());
				const float next = std::clamp(state.scroll.y.offset + std::copysign(rows * line_h, overshoot), 0.f, limit);
				state.scroll.y.offset = next;
				state.scroll.y.target = next;
				state.tail_pinned = false;
			}

			const buffer_position current = pick_position(drag_mouse);
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

	refresh_metrics(buffer, state, spans, stops, style, line_h);
	placement = text_area_layout_of(ctx, geometry);
	text_x = placement.text_left;
	top_y = placement.top;
	view_width = placement.view_width;
	block_layouts.assign(blocks.size(), std::nullopt);

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
	const int first_line = std::max(0, static_cast<int>(placement.line_at(state.scroll.y.offset)) - 1);
	const int last_line = std::min(line_total, static_cast<int>(placement.line_at(state.scroll.y.offset + view_height)) + 2);
	const float block_pad_x = pad * 0.5f;
	const float block_pad_y = pad * 0.25f;
	const float block_border = ctx.style.separator_thickness;
	for (std::size_t bi = 0; bi < blocks.size(); ++bi) {
		const text_block& block = blocks[bi];
		if (static_cast<int>(block.last_line) < first_line || static_cast<int>(block.first_line) >= last_line) {
			continue;
		}

		const block_layout& layout = layout_of(bi);
		const float block_top = placement.line_top(block.first_line);
		const float block_bottom = placement.line_top(block.last_line) + placement.line_extent(block.last_line);
		const rectf bubble = rectf::from_position_size(
			{ text_x + layout.offset - block_pad_x, top_y - block_top + block_pad_y },
			{ layout.width + block_pad_x * 2.f, block_bottom - block_top + block_pad_y * 2.f }
		);

		if (block.border.w() > 0.f) {
			ctx.queue_sprite({
				.rect = bubble.inset({ -block_border, -block_border }),
				.color = block.border,
				.texture = ctx.blank_texture,
				.clip_rect = content_clip,
				.corner_radius = ctx.style.corner_radius + block_border,
			});
		}
		if (block.fill.w() > 0.f) {
			ctx.queue_sprite({
				.rect = bubble,
				.color = block.fill,
				.texture = ctx.blank_texture,
				.clip_rect = content_clip,
				.corner_radius = ctx.style.corner_radius,
			});
		}
	}

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
			const std::vector<float> offsets = line_column_x(static_cast<std::uint32_t>(i), sline);
			const float x_a = offsets[col_a];
			const float x_b = offsets[col_b];
			const float extra = static_cast<std::uint32_t>(i) < sel_hi.line ? fnt_view->width(" ", scale) : 0.f;
			const float row_height = placement.line_extent(static_cast<std::uint32_t>(i));
			const float row_top = top_y - placement.line_top(static_cast<std::uint32_t>(i));
			ctx.queue_sprite({
				.rect = rectf::from_position_size({ x_a, row_top }, { x_b - x_a + extra, row_height }),
				.color = ctx.style.color_selection,
				.texture = ctx.blank_texture,
				.clip_rect = content_clip,
			});
		}
	}

	std::string run_scratch;
	for (int i = first_line; i < last_line; ++i) {
		const auto line_no = static_cast<std::uint32_t>(i);
		const std::string_view line = buffer.line(line_no);
		const float row_height = placement.line_extent(line_no);
		const float line_center = top_y - placement.line_top(line_no) - row_height * 0.5f;
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

		const line_layout& columns = place_line(line_no, line);
		const std::span<const styled_run> runs = scratch_runs;
		const float origin = text_x + block_x(line_no);

		if (!line.empty()) {
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
			auto emit_run = [&](const styled_run& run, const std::size_t a, const std::size_t b, const vec4f& color) {
				const std::string_view seg = expand_from(line.substr(a, b - a), columns.display[a], run_scratch);
				ctx.queue_text({
					.font = run.handle,
					.text = seg,
					.position = { origin + columns.offsets[a], line_center + run.view->vertical_center_offset(run.scale) },
					.scale = run.scale,
					.color = color,
					.clip_rect = content_clip,
				});
			};
			for (const styled_run& run : runs) {
				if (!line_faded) {
					emit_run(run, run.start, run.end, run.color);
					continue;
				}
				std::size_t cursor = run.start;
				while (cursor < run.end) {
					const float alpha = fade_alpha(cursor);
					std::size_t split = cursor + 1;
					while (split < run.end && fade_alpha(split) == alpha) {
						++split;
					}
					vec4f faded = run.color;
					faded.w() *= alpha;
					emit_run(run, cursor, split, faded);
					cursor = split;
				}
			}
		}

		for (const text_underline& u : underlines) {
			if (u.line != line_no) {
				continue;
			}
			const std::size_t a = std::min<std::size_t>(u.start_col, line.size());
			const std::size_t b = std::min<std::size_t>(std::max<std::size_t>(a + 1, u.end_col), line.size());
			const float x0 = origin + columns.offsets[a];
			const float x1 = origin + columns.offsets[b];
			ctx.queue_sprite({
				.rect = rectf::from_position_size({ x0, line_center - row_height * 0.35f }, { std::max(x1 - x0, 3.f), 2.f }),
				.color = u.color,
				.texture = ctx.blank_texture,
				.clip_rect = content_clip,
			});
		}
	}

	if (focused && state.blink_on) {
		const std::string_view caret_line = buffer.line(state.caret.line);
		const std::vector<float> caret_col_x = line_column_x(state.caret.line, caret_line);
		const float caret_x = caret_col_x[std::min<std::size_t>(state.caret.column, caret_line.size())];
		const float caret_top = top_y - placement.line_top(state.caret.line) - placement.line_extent(state.caret.line) * 0.5f + fnt_view->vertical_center_offset(scale);
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
		const float caret_y = placement.line_top(state.caret.line);
		const float caret_h = placement.line_extent(state.caret.line);
		if (caret_y < state.scroll.y.offset) {
			state.scroll.y.offset = caret_y;
			state.scroll.y.target = caret_y;
		}
		else if (caret_y + caret_h > state.scroll.y.offset + view_height) {
			const float target = caret_y + caret_h - view_height;
			state.scroll.y.offset = target;
			state.scroll.y.target = target;
		}

		const std::string_view caret_row = buffer.line(state.caret.line);
		const std::vector<float> caret_row_x = line_column_x(state.caret.line, caret_row);
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

	const float content_height = placement.content_extent() + pad * 2.f;
	const float tail_scroll = std::max(0.f, content_height - rect.height());

	if (follow_tail && state.tail_pinned) {
		state.scroll.y.target = tail_scroll;
	}

	scroll_area(ctx, state.scroll, rect, { left_inset + state.widest_line_px + pad, content_height }, scroll_cfg);

	if (follow_tail) {
		state.tail_pinned = state.scroll.y.target >= tail_scroll - 1.f;
	}

	return modified;
}

auto gse::gui::draw::follow_tail_button(builder& b, const rectf& area, text_area_state& state, const id widget_id) -> interaction::press {
	if (state.tail_pinned) {
		return {};
	}

	const draw_context& ctx = b.ctx;
	const float extent = ctx.style.font_size * 1.9f;
	const float inset = std::max(1.f, ctx.style.separator_thickness);
	const rectf rect = rectf::from_position_size(
		{ area.right() - ctx.style.padding - scroll_config{}.scrollbar_width - extent, area.bottom() + ctx.style.padding + extent },
		{ extent, extent }
	);

	const interaction::press btn = interaction::press_in_rect(ctx, b.hot_widget_id, b.active_widget_id, widget_id, rect);
	const std::uint32_t z = ctx.current_z_order + 1;

	ctx.queue_sprite({
		.rect = rect,
		.color = ctx.style.color_border,
		.texture = ctx.blank_texture,
		.z_order = z,
		.corner_radius = extent * 0.5f,
	});
	ctx.queue_sprite({
		.rect = rect.inset({ inset, inset }),
		.color = btn.color({
			.idle = ctx.style.color_title_bar,
			.hot = ctx.style.color_tab_hovered,
			.active = ctx.style.color_tab_active,
			.disabled = ctx.style.color_title_bar,
		}),
		.texture = ctx.blank_texture,
		.z_order = z,
		.corner_radius = extent * 0.5f - inset,
	});
	symbol::draw(ctx, symbol::chevron_down(), rect, {
		.color = ctx.style.color_text,
		.extent = ctx.style.icon_extent,
		.z_order = z,
	});

	if (btn.activated) {
		state.tail_pinned = true;
	}

	return btn;
}
