export module gse.ide.highlight:markdown;

import std;
import gse;

import :lexer;

export namespace gse::ide::markdown {
	struct kind_info {
		std::uint32_t color = 0;
	};

	enum class kind {
		body [[= kind_info{ .color = 0xc2cce0 }]],
		heading [[= kind_info{ .color = 0xd57192 }]],
		marker [[= kind_info{ .color = 0x2a7195 }]],
		strong [[= kind_info{ .color = 0xe8edf7 }]],
		emphasis [[= kind_info{ .color = 0xb8a5df }]],
		code [[= kind_info{ .color = 0x9586df }]],
		link_text [[= kind_info{ .color = 0x4fa3c7 }]],
		link_url [[= kind_info{ .color = 0x4b5b7e }]],
		quote [[= kind_info{ .color = 0x4b5b7e }]],
		rule [[= kind_info{ .color = 0x2b3959 }]],
		strike [[= kind_info{ .color = 0x6b7590 }]],
	};

	struct fence_marker {
		char kind = '`';
		std::size_t length = 0;
	};

	struct table_row {
		std::vector<std::string> cells;
		bool ambiguous = false;
	};

	auto indent_width(
		std::string_view line
	) -> std::size_t;

	auto lead_of(
		std::string_view line
	) -> std::string_view;

	auto trim(
		std::string_view text
	) -> std::string_view;

	auto trim_back(
		std::string_view text
	) -> std::string_view;

	auto fence_at(
		std::string_view line
	) -> std::optional<fence_marker>;

	auto closes_fence(
		std::string_view line,
		const fence_marker& open
	) -> bool;

	auto thematic_break(
		std::string_view line
	) -> bool;

	auto split_cells(
		std::string_view line
	) -> table_row;

	auto delimiter_row(
		std::string_view line
	) -> bool;

	auto spans(
		std::string_view source
	) -> std::vector<gui::text_span>;
}

namespace gse::ide::markdown {
	constexpr std::size_t code_indent = 4;
	constexpr std::size_t max_heading_level = 6;
	constexpr std::size_t min_fence_length = 3;
	constexpr std::size_t max_ordered_digits = 9;

	struct run {
		std::size_t start = 0;
		std::size_t end = 0;
		kind tone = kind::body;
	};

	struct line_context {
		bool table_row = false;
	};

	auto color_of(
		kind k
	) -> vec4f;

	auto marker_extent(
		std::string_view line
	) -> std::size_t;

	auto scan(
		std::string_view line,
		std::size_t from,
		std::size_t to,
		kind base,
		const line_context& context,
		std::vector<run>& out
	) -> void;

	auto emit_line(
		std::vector<gui::text_span>& out,
		std::uint32_t index,
		std::string_view line,
		std::size_t from,
		kind base,
		std::span<const run> runs
	) -> void;
}

auto gse::ide::markdown::color_of(const kind k) -> vec4f {
	const kind_info info = annotation_from_enum<kind_info>(k, {
		.color = 0xc2cce0,
	});
	return {
		static_cast<float>((info.color >> 16) & 0xffu) / 255.f,
		static_cast<float>((info.color >> 8) & 0xffu) / 255.f,
		static_cast<float>(info.color & 0xffu) / 255.f,
		1.f
	};
}

auto gse::ide::markdown::indent_width(const std::string_view line) -> std::size_t {
	std::size_t width = 0;
	for (const char ch : line) {
		if (ch == ' ') {
			++width;
		}
		else if (ch == '\t') {
			width += code_indent - width % code_indent;
		}
		else {
			break;
		}
	}
	return width;
}

auto gse::ide::markdown::lead_of(const std::string_view line) -> std::string_view {
	const std::size_t first = line.find_first_not_of(" \t");
	return first == std::string_view::npos ? line : line.substr(0, first);
}

auto gse::ide::markdown::trim(const std::string_view text) -> std::string_view {
	const std::size_t first = text.find_first_not_of(" \t");
	if (first == std::string_view::npos) {
		return {};
	}
	return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

auto gse::ide::markdown::trim_back(const std::string_view text) -> std::string_view {
	const std::size_t last = text.find_last_not_of(" \t");
	return last == std::string_view::npos ? std::string_view{} : text.substr(0, last + 1);
}

auto gse::ide::markdown::fence_at(const std::string_view line) -> std::optional<fence_marker> {
	if (indent_width(line) >= code_indent) {
		return std::nullopt;
	}
	const std::string_view rest = trim(line);
	if (rest.empty() || (rest.front() != '`' && rest.front() != '~')) {
		return std::nullopt;
	}
	const char marker = rest.front();
	const std::size_t run_end = rest.find_first_not_of(marker);
	const std::size_t length = run_end == std::string_view::npos ? rest.size() : run_end;
	if (length < min_fence_length) {
		return std::nullopt;
	}
	if (marker == '`' && rest.substr(length).contains('`')) {
		return std::nullopt;
	}
	return fence_marker{ .kind = marker, .length = length };
}

auto gse::ide::markdown::closes_fence(const std::string_view line, const fence_marker& open) -> bool {
	if (indent_width(line) >= code_indent) {
		return false;
	}
	const std::string_view rest = trim(line);
	if (rest.size() < open.length) {
		return false;
	}
	return std::ranges::all_of(rest, [&open](const char ch) {
		return ch == open.kind;
	});
}

auto gse::ide::markdown::thematic_break(const std::string_view line) -> bool {
	const std::string_view rest = trim(line);
	if (rest.empty() || (rest.front() != '-' && rest.front() != '*' && rest.front() != '_')) {
		return false;
	}
	const char marker = rest.front();
	std::size_t count = 0;
	for (const char ch : rest) {
		if (ch == marker) {
			++count;
		}
		else if (ch != ' ' && ch != '\t') {
			return false;
		}
	}
	return count >= 3;
}

auto gse::ide::markdown::split_cells(const std::string_view line) -> table_row {
	const std::string_view rest = trim(line);
	table_row row;
	std::string segment;
	std::size_t open_ticks = 0;
	for (std::size_t i = rest.starts_with('|') ? 1 : 0; i < rest.size(); ++i) {
		if (rest[i] == '\\' && i + 1 < rest.size() && rest[i + 1] == '|') {
			segment += "\\|";
			++i;
			continue;
		}
		if (rest[i] == '`') {
			const std::size_t next = rest.find_first_not_of('`', i);
			const std::size_t length = (next == std::string_view::npos ? rest.size() : next) - i;
			if (open_ticks == 0) {
				open_ticks = length;
			}
			else if (open_ticks == length) {
				open_ticks = 0;
			}
			segment += rest.substr(i, length);
			i += length - 1;
			continue;
		}
		if (rest[i] == '|') {
			if (open_ticks > 0) {
				row.ambiguous = true;
				return row;
			}
			row.cells.emplace_back(trim(segment));
			segment.clear();
			continue;
		}
		segment += rest[i];
	}
	if (!trim(segment).empty()) {
		row.cells.emplace_back(trim(segment));
	}
	return row;
}

auto gse::ide::markdown::delimiter_row(const std::string_view line) -> bool {
	if (!trim(line).starts_with('|')) {
		return false;
	}
	const table_row row = split_cells(line);
	if (row.ambiguous || row.cells.empty()) {
		return false;
	}
	return std::ranges::all_of(row.cells, [](const std::string& cell) {
		std::string_view body = cell;
		if (body.starts_with(':')) {
			body.remove_prefix(1);
		}
		if (body.ends_with(':')) {
			body.remove_suffix(1);
		}
		return !body.empty() && body.find_first_not_of('-') == std::string_view::npos;
	});
}

auto gse::ide::markdown::marker_extent(const std::string_view line) -> std::size_t {
	const std::size_t lead = lead_of(line).size();
	const std::string_view rest = line.substr(lead);
	if (rest.empty()) {
		return lead;
	}
	if (rest.front() == '>') {
		return lead + (rest.size() > 1 && rest[1] == ' ' ? 2 : 1);
	}
	if ((rest.front() == '-' || rest.front() == '*' || rest.front() == '+')
		&& rest.size() > 1 && (rest[1] == ' ' || rest[1] == '\t')) {
		return lead + 2;
	}
	const std::size_t digits = rest.find_first_not_of("0123456789");
	if (digits != std::string_view::npos && digits > 0 && digits <= max_ordered_digits
		&& (rest[digits] == '.' || rest[digits] == ')')
		&& digits + 1 < rest.size() && (rest[digits + 1] == ' ' || rest[digits + 1] == '\t')) {
		return lead + digits + 2;
	}
	return lead;
}

auto gse::ide::markdown::scan(const std::string_view line, const std::size_t from, const std::size_t to, const kind base, const line_context& context, std::vector<run>& out) -> void {
	std::size_t plain = from;
	auto flush = [&out, &plain, base](const std::size_t at) {
		if (at > plain) {
			out.push_back({ .start = plain, .end = at, .tone = base });
		}
	};

	for (std::size_t i = from; i < to;) {
		if (line[i] == '\\' && i + 1 < to) {
			i += 2;
			continue;
		}
		if (line[i] == '`') {
			const std::size_t next = line.find_first_not_of('`', i);
			const std::size_t length = (next == std::string_view::npos ? line.size() : next) - i;
			const std::string ticks(length, '`');
			if (const std::size_t close = line.find(ticks, i + length); close != std::string_view::npos && close + length <= to) {
				flush(i);
				out.push_back({ .start = i, .end = close + length, .tone = kind::code });
				i = close + length;
				plain = i;
				continue;
			}
			i += length;
			continue;
		}
		if (context.table_row && line[i] == '|') {
			flush(i);
			out.push_back({ .start = i, .end = i + 1, .tone = kind::marker });
			++i;
			plain = i;
			continue;
		}
		if (line.compare(i, 2, "[[") == 0) {
			if (const std::size_t close = line.find("]]", i + 2); close != std::string_view::npos && close + 2 <= to) {
				flush(i);
				out.push_back({ .start = i, .end = close + 2, .tone = kind::link_text });
				i = close + 2;
				plain = i;
				continue;
			}
		}
		if (line.compare(i, 2, "**") == 0 || line.compare(i, 2, "~~") == 0) {
			const std::string_view pair = line.substr(i, 2);
			if (const std::size_t close = line.find(pair, i + 2); close != std::string_view::npos && close > i + 2 && close + 2 <= to) {
				const kind tone = pair == "**" ? kind::strong : kind::strike;
				flush(i);
				out.push_back({ .start = i, .end = i + 2, .tone = tone });
				scan(line, i + 2, close, tone, context, out);
				out.push_back({ .start = close, .end = close + 2, .tone = tone });
				i = close + 2;
				plain = i;
				continue;
			}
		}
		if (line[i] == '*' && i + 1 < to && line[i + 1] != '*' && line[i + 1] != ' ') {
			if (const std::size_t close = line.find('*', i + 1); close != std::string_view::npos && close < to && line[close - 1] != ' ') {
				flush(i);
				out.push_back({ .start = i, .end = i + 1, .tone = kind::emphasis });
				scan(line, i + 1, close, kind::emphasis, context, out);
				out.push_back({ .start = close, .end = close + 1, .tone = kind::emphasis });
				i = close + 1;
				plain = i;
				continue;
			}
		}
		if (line[i] == '[') {
			const std::size_t close = line.find(']', i + 1);
			if (close != std::string_view::npos && line.compare(close + 1, 1, "(") == 0) {
				if (const std::size_t target = line.find(')', close + 2); target != std::string_view::npos && target < to) {
					flush(i);
					out.push_back({ .start = i, .end = i + 1, .tone = kind::link_text });
					scan(line, i + 1, close, kind::link_text, context, out);
					out.push_back({ .start = close, .end = close + 1, .tone = kind::link_text });
					out.push_back({ .start = close + 1, .end = target + 1, .tone = kind::link_url });
					i = target + 1;
					plain = i;
					continue;
				}
			}
		}
		if (line[i] == '<' && line.compare(i + 1, 4, "http") == 0) {
			if (const std::size_t close = line.find('>', i + 1); close != std::string_view::npos && close < to) {
				flush(i);
				out.push_back({ .start = i, .end = close + 1, .tone = kind::link_url });
				i = close + 1;
				plain = i;
				continue;
			}
		}
		++i;
	}
	flush(to);
}

auto gse::ide::markdown::emit_line(std::vector<gui::text_span>& out, const std::uint32_t index, const std::string_view line, const std::size_t from, const kind base, const std::span<const run> runs) -> void {
	std::size_t written = from;
	for (const run& r : runs) {
		if (r.start > written) {
			out.push_back({
				.line = index,
				.start_col = static_cast<std::uint32_t>(written),
				.end_col = static_cast<std::uint32_t>(r.start),
				.color = color_of(base),
			});
		}
		out.push_back({
			.line = index,
			.start_col = static_cast<std::uint32_t>(r.start),
			.end_col = static_cast<std::uint32_t>(r.end),
			.color = color_of(r.tone),
		});
		written = r.end;
	}
	if (written < line.size()) {
		out.push_back({
			.line = index,
			.start_col = static_cast<std::uint32_t>(written),
			.end_col = static_cast<std::uint32_t>(line.size()),
			.color = color_of(base),
		});
	}
}

auto gse::ide::markdown::spans(const std::string_view source) -> std::vector<gui::text_span> {
	const std::vector<std::string_view> lines = syntax::split_lines(source);
	std::vector<gui::text_span> out;

	std::size_t start = 0;
	if (!lines.empty() && lines.front() == "---") {
		for (std::size_t i = 1; i < lines.size(); ++i) {
			if (lines[i] == "---" || lines[i] == "...") {
				start = i + 1;
				break;
			}
		}
		for (std::size_t i = 0; i < start; ++i) {
			out.push_back({
				.line = static_cast<std::uint32_t>(i),
				.start_col = 0,
				.end_col = static_cast<std::uint32_t>(lines[i].size()),
				.color = color_of(kind::quote),
			});
		}
	}

	std::optional<fence_marker> open;
	for (std::size_t i = start; i < lines.size(); ++i) {
		const std::string_view line = lines[i];
		const auto index = static_cast<std::uint32_t>(i);

		if (line.empty()) {
			continue;
		}

		if (open) {
			out.push_back({
				.line = index,
				.start_col = 0,
				.end_col = static_cast<std::uint32_t>(line.size()),
				.color = color_of(closes_fence(line, *open) ? kind::marker : kind::code),
			});
			if (closes_fence(line, *open)) {
				open.reset();
			}
			continue;
		}

		if (const std::optional<fence_marker> marker = fence_at(line)) {
			open = marker;
			out.push_back({
				.line = index,
				.start_col = 0,
				.end_col = static_cast<std::uint32_t>(line.size()),
				.color = color_of(kind::marker),
			});
			continue;
		}

		if (indent_width(line) >= code_indent) {
			out.push_back({
				.line = index,
				.start_col = 0,
				.end_col = static_cast<std::uint32_t>(line.size()),
				.color = color_of(kind::code),
			});
			continue;
		}

		if (thematic_break(line)) {
			out.push_back({
				.line = index,
				.start_col = 0,
				.end_col = static_cast<std::uint32_t>(line.size()),
				.color = color_of(kind::rule),
			});
			continue;
		}

		const std::string_view rest = trim(line);
		const std::size_t hashes = rest.find_first_not_of('#');
		if (hashes != std::string_view::npos && hashes > 0 && hashes <= max_heading_level
			&& (rest[hashes] == ' ' || rest[hashes] == '\t')) {
			out.push_back({
				.line = index,
				.start_col = 0,
				.end_col = static_cast<std::uint32_t>(line.size()),
				.color = color_of(kind::heading),
			});
			continue;
		}

		const bool table = rest.starts_with('|');
		if (table && delimiter_row(line)) {
			out.push_back({
				.line = index,
				.start_col = 0,
				.end_col = static_cast<std::uint32_t>(line.size()),
				.color = color_of(kind::marker),
			});
			continue;
		}

		const bool quoted = rest.starts_with('>');
		const std::size_t content = marker_extent(line);
		if (content > lead_of(line).size()) {
			out.push_back({
				.line = index,
				.start_col = static_cast<std::uint32_t>(lead_of(line).size()),
				.end_col = static_cast<std::uint32_t>(content),
				.color = color_of(kind::marker),
			});
		}

		const kind base = quoted ? kind::quote : kind::body;
		std::vector<run> runs;
		scan(line, content, line.size(), base, { .table_row = table }, runs);
		emit_line(out, index, line, content, base, runs);
	}

	return out;
}
