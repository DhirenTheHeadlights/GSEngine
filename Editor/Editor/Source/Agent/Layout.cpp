module gse.ide.agent:layout_impl;

import std;
import gse;
import gse.ide.highlight;

import :layout;
import :model;
import :stream;

auto gse::ide::agent::row_prefix(const row_kind kind) -> std::string_view {
	switch (kind) {
		case row_kind::user:
		case row_kind::text:
			return "";
		case row_kind::tool:
			return "- ";
		case row_kind::denial:
			return "x ";
		case row_kind::failure:
			return "! ";
		default:
			return "  ";
	}
}

auto gse::ide::agent::row_color(const gui::style& sty, const row_kind kind) -> vec4f {
	switch (kind) {
		case row_kind::user:
			return sty.color_text;
		case row_kind::text:
			return sty.color_accent;
		case row_kind::tool:
			return sty.color_accent_dim;
		case row_kind::denial:
			return sty.color_warning;
		case row_kind::failure:
			return sty.color_error;
		default:
			return sty.color_text_secondary;
	}
}

auto gse::ide::agent::line_base_style(const gui::style& sty, const markdown::line_info& info, const vec4f& fallback) -> markdown::display_style {
	if (info.shape == markdown::block::heading) {
		return {
			.color = sty.color_section_header,
			.face = gui::text_face::code_strong,
			.scale = markdown::heading_scale(info.heading_level),
		};
	}
	return markdown::style_of(markdown::base_tone(info), markdown::family::monospace, sty, fallback);
}

auto gse::ide::agent::table_extent(const std::span<const std::string> lines, const std::size_t first) -> std::size_t {
	const auto row_at = [lines](const std::size_t i) {
		std::string_view text = lines[i];
		const std::size_t start = text.find_first_not_of(" \t");
		return start == std::string_view::npos ? std::string_view{} : text.substr(start);
	};

	if (first + 1 >= lines.size() || !row_at(first).starts_with('|')) {
		return first;
	}
	if (!markdown::delimiter_row(lines[first + 1])) {
		return first;
	}

	std::size_t last = first + 1;
	while (last + 1 < lines.size() && row_at(last + 1).starts_with('|')) {
		++last;
	}
	return last;
}

auto gse::ide::agent::align_table(const std::span<const std::string> rows) -> std::vector<std::string> {
	std::vector<std::vector<std::string>> cells;
	cells.reserve(rows.size());
	std::size_t columns = 0;
	for (const std::string& row : rows) {
		markdown::table_row parsed = markdown::split_cells(row);
		if (parsed.ambiguous) {
			return {};
		}
		columns = std::max(columns, parsed.cells.size());
		cells.push_back(std::move(parsed.cells));
	}

	std::vector<std::size_t> widths(columns, 0);
	for (std::size_t r = 0; r < cells.size(); ++r) {
		if (r == 1) {
			continue;
		}
		for (std::size_t c = 0; c < cells[r].size(); ++c) {
			widths[c] = std::max(widths[c], cells[r][c].size());
		}
	}

	std::vector<std::string> out;
	out.reserve(rows.size());
	for (std::size_t r = 0; r < cells.size(); ++r) {
		std::string text = "|";
		for (std::size_t c = 0; c < columns; ++c) {
			if (r == 1) {
				text.append(widths[c] + 2, '-');
			}
			else {
				const std::string_view cell = c < cells[r].size() ? std::string_view(cells[r][c]) : std::string_view{};
				text += ' ';
				text += cell;
				text.append(widths[c] - cell.size() + 1, ' ');
			}
			text += '|';
		}
		out.push_back(std::move(text));
	}
	return out;
}

auto gse::ide::agent::push_markup_line(session& s, const gui::style& sty, const markup_line& parsed, const transcript_metrics& metrics, transcript_cursor& cursor) -> void {
	const std::vector<std::string_view> wrapped = cursor.wrap
		? metrics.face.wrap(parsed.text, cursor.available, metrics.scale * parsed.base.scale)
		: std::vector<std::string_view>{ parsed.text };

	for (std::string_view segment : wrapped) {
		if (cursor.wrap) {
			while (!segment.empty() && segment.back() == ' ') {
				segment.remove_suffix(1);
			}
		}

		const auto offset = static_cast<std::size_t>(segment.data() - parsed.text.data());
		const std::size_t last = offset + segment.size();
		std::string text = cursor.first ? std::string(cursor.prefix) : cursor.indent;
		const auto column = static_cast<std::uint32_t>(text.size());
		const auto index = static_cast<std::uint32_t>(s.buffer.lines.size());
		text += segment;

		if (column > 0) {
			s.spans.push_back({
				.line = index,
				.start_col = 0,
				.end_col = column,
				.color = cursor.prefix_color,
			});
		}

		std::size_t written = offset;
		for (const markdown::rendered_run& run : parsed.runs) {
			if (run.end <= written) {
				continue;
			}
			if (run.start >= last) {
				break;
			}
			const std::size_t from = std::max(run.start, written);
			const std::size_t to = std::min(run.end, last);
			if (to <= from) {
				continue;
			}
			if (from > written) {
				s.spans.push_back({
					.line = index,
					.start_col = column + static_cast<std::uint32_t>(written - offset),
					.end_col = column + static_cast<std::uint32_t>(from - offset),
					.color = parsed.base.color,
					.face = parsed.base.face,
					.scale = parsed.base.scale,
				});
			}
			const markdown::display_style shown = markdown::style_of(run.tone, markdown::family::monospace, sty, parsed.base.color);
			s.spans.push_back({
				.line = index,
				.start_col = column + static_cast<std::uint32_t>(from - offset),
				.end_col = column + static_cast<std::uint32_t>(to - offset),
				.color = shown.color,
				.face = shown.face,
				.scale = parsed.base.scale,
			});
			written = to;
		}
		if (written < last) {
			s.spans.push_back({
				.line = index,
				.start_col = column + static_cast<std::uint32_t>(written - offset),
				.end_col = static_cast<std::uint32_t>(text.size()),
				.color = parsed.base.color,
				.face = parsed.base.face,
				.scale = parsed.base.scale,
			});
		}

		s.buffer.lines.push_back(std::move(text));
		s.line_rows.push_back(cursor.row);
		cursor.first = false;
	}
}

auto gse::ide::agent::push_transcript_line(session& s, const gui::style& sty, const transcript_line& line, const transcript_metrics& metrics) -> void {
	const float wrap_width = line.wrap_width > 0.f ? line.wrap_width : metrics.width;
	transcript_cursor cursor{
		.prefix = line.prefix,
		.indent = std::string(line.prefix.size(), ' '),
		.prefix_color = line.color,
		.available = wrap_width - metrics.face.width(line.prefix, metrics.scale),
		.row = line.row,
	};

	const std::vector<std::string> raw = to_lines(line.text);
	std::vector<std::string> sources;
	sources.reserve(raw.size());
	for (const std::string& row : raw) {
		sources.push_back(expand_tabs(row));
	}

	if (!line.markdown) {
		for (const std::string& row : sources) {
			push_markup_line(s, sty, {
				.text = row,
				.base = { .color = line.color },
			}, metrics, cursor);
		}
		return;
	}

	const std::vector<std::string_view> views(sources.begin(), sources.end());
	const std::vector<markdown::line_info> classified = markdown::classify(views);

	std::vector<markdown::run> scratch;
	markdown::rendered_line rendered;

	for (std::size_t i = 0; i < sources.size(); ++i) {
		if (!markdown::verbatim(classified[i].shape)) {
			if (const std::size_t last = table_extent(sources, i); last > i) {
				const std::vector<std::string> aligned = align_table(std::span(sources).subspan(i, last - i + 1));
				const auto widest = std::ranges::max_element(aligned, {}, [&metrics](const std::string& row) {
					return metrics.face.width(row, metrics.scale);
				});
				if (widest != aligned.end() && metrics.face.width(*widest, metrics.scale) <= cursor.available) {
					cursor.wrap = false;
					for (const std::string& row : aligned) {
						push_markup_line(s, sty, {
							.text = row,
							.base = { .color = line.color },
						}, metrics, cursor);
					}
					cursor.wrap = true;
					i = last;
					continue;
				}
			}
		}

		markdown::render_line(views[i], classified[i], scratch, rendered);
		push_markup_line(s, sty, {
			.text = rendered.text,
			.runs = rendered.runs,
			.base = line_base_style(sty, classified[i], line.color),
		}, metrics, cursor);
	}
}

auto gse::ide::agent::command_row(const transcript_row& row) -> bool {
	return row.kind == row_kind::tool && (row.text == "Bash" || row.text == "PowerShell");
}

auto gse::ide::agent::chat_row(const transcript_row& row) -> bool {
	return row.kind == row_kind::user || row.kind == row_kind::text;
}

auto gse::ide::agent::after_bubble(const session& s) -> bool {
	return !s.blocks.empty() && s.blocks.back().last_line + 1 == s.buffer.lines.size();
}

auto gse::ide::agent::chat_bubble(const gui::style& sty, const transcript_row& row, const std::uint32_t first_line, const std::uint32_t last_line) -> gui::text_block {
	const bool mine = row.kind == row_kind::user;
	return {
		.first_line = first_line,
		.last_line = last_line,
		.fill = { vec3f(mine ? sty.color_tab_active : sty.color_panel_alt), 1.f },
		.border = mine ? sty.color_accent : sty.color_border,
		.align_right = mine,
	};
}

auto gse::ide::agent::link_at(const session& s, const std::uint32_t line) -> const link_marker* {
	const auto above = std::ranges::upper_bound(s.links, line, {}, &link_marker::first_line);
	if (above == s.links.begin()) {
		return nullptr;
	}

	const auto found = std::prev(above);
	return line <= found->last_line ? &*found : nullptr;
}

auto gse::ide::agent::group_expanded(const session& s, const std::uint32_t row) -> bool {
	return std::ranges::find(s.expanded_groups, row) != s.expanded_groups.end();
}

auto gse::ide::agent::expand_tabs(const std::string_view text) -> std::string {
	std::string out;
	out.reserve(text.size());
	for (const char c : text) {
		if (c == '\t') {
			out.append(transcript_tab_width - out.size() % transcript_tab_width, ' ');
		}
		else {
			out.push_back(c);
		}
	}
	return out;
}

auto gse::ide::agent::clip_columns(const std::string_view text, const std::size_t offset, const std::size_t width) -> std::string_view {
	if (offset >= text.size() || width == 0) {
		return {};
	}

	std::size_t start = offset;
	while (start < text.size() && (static_cast<unsigned char>(text[start]) & 0xc0) == 0x80) {
		++start;
	}

	std::size_t end = std::min(text.size(), start + width);
	while (end > start && end < text.size() && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) {
		--end;
	}
	return text.substr(start, end - start);
}

auto gse::ide::agent::column_overflow(const std::string_view text, const std::size_t width) -> std::size_t {
	return text.size() > width ? text.size() - width : 0;
}

auto gse::ide::agent::diff_view_for(session& s, const std::uint32_t row) -> diff_view& {
	if (const auto found = std::ranges::find(s.diffs, row, &diff_view::row); found != s.diffs.end()) {
		return *found;
	}

	s.diffs.push_back({ .row = row });
	return s.diffs.back();
}

auto gse::ide::agent::relayout_from(session& s, const std::uint32_t row) -> void {
	const auto found = std::ranges::find(s.line_rows, row);
	if (found == s.line_rows.end()) {
		return;
	}

	truncate_transcript(s, static_cast<std::uint32_t>(std::distance(s.line_rows.begin(), found)));
	while (!s.groups.empty() && s.groups.back().row >= row) {
		s.groups.pop_back();
	}
	s.flushed_rows = row;
}

auto gse::ide::agent::draw_diff_bars(const gui::draw_context& ctx, session& s, const rectf& area, const float advance) -> void {
	if (advance <= 0.f) {
		return;
	}

	constexpr gui::scroll_config bar = { .auto_hide_scrollbar = false };
	const gui::text_area_layout geometry = gui::draw::text_area_layout_of(ctx, {
		.buffer = s.buffer,
		.state = s.view,
		.rect = area,
		.spans = s.spans,
		.blocks = s.blocks,
		.indent_width = transcript_tab_width,
	});
	std::optional<std::uint32_t> stale;

	for (diff_view& view : s.diffs) {
		if (view.line == unplaced_line || view.overflow == 0 || view.width <= diff_indent) {
			if (view.columns > 0) {
				view.columns = 0;
				view.scroll = {};
				stale = stale ? std::min(*stale, view.row) : view.row;
			}
			continue;
		}

		const float center = geometry.top - geometry.line_top(view.line) - geometry.line_extent(view.line) * 0.5f;
		const rectf track = rectf::from_position_size(
			{ geometry.text_left + static_cast<float>(diff_indent) * advance, center + bar.scrollbar_width * 0.5f },
			{ static_cast<float>(view.width - diff_indent) * advance, bar.scrollbar_width }
		);

		if (track.top() > area.top() || track.bottom() < area.bottom()) {
			continue;
		}

		gui::scroll_area(ctx, view.scroll, track, { track.width() + static_cast<float>(view.overflow) * advance, track.height() }, bar);

		if (const auto columns = static_cast<std::size_t>(std::max(0.f, view.scroll.x.offset) / advance); columns != view.columns) {
			view.columns = columns;
			stale = stale ? std::min(*stale, view.row) : view.row;
		}
	}

	if (stale) {
		relayout_from(s, *stale);
	}
}

auto gse::ide::agent::hunk_start_line(const transcript_row& row) -> std::uint32_t {
	std::string_view needle;
	if (!row.removed.empty()) {
		needle = row.removed.front();
	}
	else if (!row.added.empty()) {
		needle = row.added.front();
	}

	if (row.file.empty() || needle.empty()) {
		return 1;
	}

	std::ifstream in(row.file);
	if (!in) {
		return 1;
	}

	std::string line;
	std::uint32_t index = 1;
	while (std::getline(in, line)) {
		if (line.find(needle) != std::string::npos) {
			return index;
		}
		++index;
	}
	return 1;
}

auto gse::ide::agent::push_diff_line(session& s, const gui::style& sty, const std::uint32_t index, const diff_layout& layout, const std::span<const diff_cell> cells) -> void {
	const auto line = static_cast<std::uint32_t>(s.buffer.lines.size());
	std::string text(diff_indent, ' ');

	for (std::size_t i = 0; i < cells.size(); ++i) {
		const diff_cell& cell = cells[i];
		const std::string number = cell.number > 0 ? std::to_string(cell.number) : std::string{};
		const auto marked = static_cast<std::uint32_t>(text.size());

		text.append(layout.gutter - std::min(layout.gutter, number.size()), ' ');
		text += number;
		text.push_back(' ');
		s.spans.push_back({
			.line = line,
			.start_col = marked,
			.end_col = static_cast<std::uint32_t>(text.size()),
			.color = sty.color_text_disabled,
		});

		const auto body = static_cast<std::uint32_t>(text.size());
		text += cell.text;
		s.spans.push_back({
			.line = line,
			.start_col = body,
			.end_col = static_cast<std::uint32_t>(text.size()),
			.color = cell.color,
		});

		if (i + 1 < cells.size()) {
			text.append(layout.width - std::min(layout.width, cell.text.size()) + diff_gap, ' ');
		}
	}

	s.buffer.lines.push_back(std::move(text));
	s.line_rows.push_back(index);
}

auto gse::ide::agent::push_diff_side(session& s, const gui::style& sty, diff_view& view, const std::span<const std::string> lines, const std::uint32_t start, const std::uint32_t index, const diff_layout& layout, const vec4f& color) -> void {
	for (std::size_t i = 0; i < lines.size(); ++i) {
		const std::string expanded = expand_tabs(lines[i]);
		view.overflow = std::max(view.overflow, column_overflow(expanded, layout.width));

		const std::array<diff_cell, 1> cells = {
			diff_cell{
				.number = start + static_cast<std::uint32_t>(i),
				.text = clip_columns(expanded, layout.offset, layout.width),
				.color = color,
			},
		};
		push_diff_line(s, sty, index, layout, cells);
	}
}

auto gse::ide::agent::push_diff(session& s, const gui::style& sty, transcript_row& row, const std::uint32_t index, const transcript_metrics& metrics) -> void {
	if (row.removed.empty() && row.added.empty()) {
		return;
	}

	if (!row.start_line) {
		row.start_line = hunk_start_line(row);
	}

	const std::uint32_t start = *row.start_line;
	const std::size_t pairs = std::max(row.removed.size(), row.added.size());
	const std::size_t gutter = std::to_string(start + static_cast<std::uint32_t>(pairs)).size();

	const float advance = metrics.face.width("0", metrics.scale);
	const auto columns = advance > 0.f ? static_cast<std::size_t>(metrics.width / advance) : 0;
	const std::size_t fixed = diff_indent + (gutter + 1) * 2 + diff_gap;
	const std::size_t side = columns > fixed ? (columns - fixed) / 2 : 0;

	const std::size_t stacked = diff_indent + gutter + 1;
	const bool split = !row.removed.empty() && !row.added.empty() && side >= diff_min_side_columns;

	diff_view& view = diff_view_for(s, index);
	view.overflow = 0;

	const diff_layout layout = {
		.gutter = gutter,
		.width = split ? side : std::max(diff_min_columns, columns > stacked ? columns - stacked : 0),
		.offset = view.columns,
	};
	view.width = static_cast<std::uint32_t>(split ? fixed + side * 2 : stacked + layout.width);

	if (!split) {
		push_diff_side(s, sty, view, row.removed, start, index, layout, sty.color_removed);
		push_diff_side(s, sty, view, row.added, start, index, layout, sty.color_added);
	}

	for (std::size_t i = 0; split && i < pairs; ++i) {
		const std::string left = i < row.removed.size() ? expand_tabs(row.removed[i]) : std::string{};
		const std::string right = i < row.added.size() ? expand_tabs(row.added[i]) : std::string{};
		view.overflow = std::max({ view.overflow, column_overflow(left, side), column_overflow(right, side) });

		const std::array<diff_cell, 2> cells = {
			diff_cell{
				.number = i < row.removed.size() ? start + static_cast<std::uint32_t>(i) : 0,
				.text = clip_columns(left, layout.offset, side),
				.color = sty.color_removed,
			},
			diff_cell{
				.number = i < row.added.size() ? start + static_cast<std::uint32_t>(i) : 0,
				.text = clip_columns(right, layout.offset, side),
				.color = sty.color_added,
			},
		};
		push_diff_line(s, sty, index, layout, cells);
	}

	if (view.overflow == 0) {
		view.line = unplaced_line;
		return;
	}

	view.line = static_cast<std::uint32_t>(s.buffer.lines.size());
	s.buffer.lines.emplace_back();
	s.line_rows.push_back(index);
}

auto gse::ide::agent::toggle_group(session& s, const std::size_t index) -> void {
	const group_marker group = s.groups[index];

	if (const auto found = std::ranges::find(s.expanded_groups, group.row); found != s.expanded_groups.end()) {
		s.expanded_groups.erase(found);
	}
	else {
		s.expanded_groups.push_back(group.row);
	}

	truncate_transcript(s, group.line);
	s.groups.resize(index);
	s.flushed_rows = group.row;
}

auto gse::ide::agent::truncate_transcript(session& s, const std::uint32_t line) -> void {
	s.buffer.lines.resize(line);
	s.line_rows.resize(line);
	while (!s.spans.empty() && s.spans.back().line >= line) {
		s.spans.pop_back();
	}
	while (!s.blocks.empty() && s.blocks.back().last_line >= line) {
		s.blocks.pop_back();
	}
	while (!s.links.empty() && s.links.back().last_line >= line) {
		s.links.pop_back();
	}
	for (diff_view& view : s.diffs) {
		if (view.line != unplaced_line && view.line >= line) {
			view.line = unplaced_line;
		}
	}
}

auto gse::ide::agent::push_row(session& s, const gui::style& sty, transcript_row& row, const std::uint32_t index, const transcript_metrics& metrics) -> void {
	const bool chat = chat_row(row);
	if ((chat || after_bubble(s)) && !s.buffer.lines.empty()) {
		s.buffer.lines.emplace_back();
		s.line_rows.push_back(index);
	}

	const auto opened = static_cast<std::uint32_t>(s.buffer.lines.size());

	push_transcript_line(s, sty, {
		.prefix = row_prefix(row.kind),
		.text = row.text,
		.color = row_color(sty, row.kind),
		.row = index,
		.wrap_width = row.kind == row_kind::user ? metrics.width * chat_wrap_fraction : 0.f,
		.markdown = row.kind == row_kind::text,
	}, metrics);

	if (!row.detail.empty()) {
		push_transcript_line(s, sty, {
			.prefix = "    ",
			.text = row.detail,
			.color = row.file.empty() ? sty.color_text_secondary : sty.color_file,
			.row = index,
		}, metrics);
	}

	if (!row.file.empty()) {
		s.links.push_back({
			.first_line = opened,
			.last_line = static_cast<std::uint32_t>(s.buffer.lines.size()) - 1,
			.row = index,
		});
	}

	push_diff(s, sty, row, index, metrics);

	if (chat && s.buffer.lines.size() > opened) {
		s.blocks.push_back(chat_bubble(sty, row, opened, static_cast<std::uint32_t>(s.buffer.lines.size()) - 1));
	}
}

auto gse::ide::agent::sync_transcript(session& s, const gui::style& sty, const transcript_metrics& metrics) -> void {
	if (std::abs(s.wrap_width - metrics.width) > 0.5f) {
		s.wrap_width = metrics.width;
		s.buffer.lines.clear();
		s.spans.clear();
		s.blocks.clear();
		s.links.clear();
		s.line_rows.clear();
		s.groups.clear();
		s.flushed_rows = 0;
		for (diff_view& view : s.diffs) {
			view.line = unplaced_line;
		}
	}

	for (; s.flushed_rows < s.rows.size(); ++s.flushed_rows) {
		transcript_row& row = s.rows[s.flushed_rows];
		const auto row_index = static_cast<std::uint32_t>(s.flushed_rows);

		if (!command_row(row)) {
			push_row(s, sty, row, row_index, metrics);
			continue;
		}

		if (s.flushed_rows > 0 && !s.groups.empty() && command_row(s.rows[s.flushed_rows - 1])) {
			truncate_transcript(s, s.groups.back().line);
		}
		else {
			if (after_bubble(s)) {
				s.buffer.lines.emplace_back();
				s.line_rows.push_back(row_index);
			}
			s.groups.push_back({
				.row = row_index,
				.line = static_cast<std::uint32_t>(s.buffer.lines.size()),
			});
		}

		group_marker& group = s.groups.back();
		group.rows = row_index - group.row + 1;

		if (group.rows == 1) {
			push_row(s, sty, row, row_index, metrics);
			continue;
		}

		const bool expanded = group_expanded(s, group.row);

		push_transcript_line(s, sty, {
			.prefix = expanded ? "- " : "+ ",
			.text = std::format("ran {} commands", group.rows),
			.color = row_color(sty, row_kind::tool),
			.row = group.row,
		}, metrics);

		if (!expanded) {
			continue;
		}

		for (std::uint32_t i = group.row; i <= row_index; ++i) {
			push_row(s, sty, s.rows[i], i, metrics);
		}
	}
}
