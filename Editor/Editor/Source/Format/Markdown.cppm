export module gse.ide.format:markdown;

import std;
import gse.ide.highlight;

import :formatter;

export namespace gse::ide::format {
	auto compute_markdown(
		std::span<const std::string> lines
	) -> std::vector<line_edit>;
}

namespace gse::ide::format {
	constexpr std::size_t code_indent = 4;
	constexpr std::size_t max_heading_level = 6;
	constexpr std::size_t max_ordered_digits = 9;

	using markdown::fence_marker;
	using markdown::table_row;

	struct table_span {
		std::size_t first = 0;
		std::size_t last = 0;
	};

	struct flow_state {
		bool list_allowed = true;
	};

	using markdown::closes_fence;
	using markdown::delimiter_row;
	using markdown::fence_at;
	using markdown::indent_width;
	using markdown::lead_of;
	using markdown::split_cells;
	using markdown::thematic_break;
	using markdown::trim;
	using markdown::trim_back;

	auto canonical_cells(
		std::string_view lead,
		std::span<const std::string> cells
	) -> std::string;

	auto canonical_delimiters(
		std::string_view lead,
		std::span<const std::string> cells
	) -> std::string;

	auto rewrite_heading(
		std::string_view line
	) -> std::optional<std::string>;

	auto rewrite_bullet(
		std::string_view line
	) -> std::optional<std::string>;

	auto rewrite_ordered(
		std::string_view line
	) -> std::optional<std::string>;

	auto rewrite_body(
		std::string_view line,
		const flow_state& flow
	) -> std::string;

	auto verbatim_regions(
		std::span<const std::string> lines,
		std::vector<char>& out
	) -> void;

	auto table_regions(
		std::span<const std::string> lines,
		std::span<const char> verbatim
	) -> std::vector<table_span>;

	auto push_edit(
		std::vector<line_edit>& edits,
		std::size_t line,
		std::string_view before,
		std::string_view after
	) -> void;
}

auto gse::ide::format::canonical_cells(const std::string_view lead, const std::span<const std::string> cells) -> std::string {
	std::string out(lead);
	out += '|';
	for (const std::string& cell : cells) {
		out += ' ';
		out += cell;
		out += " |";
	}
	return out;
}

auto gse::ide::format::canonical_delimiters(const std::string_view lead, const std::span<const std::string> cells) -> std::string {
	std::string out(lead);
	out += '|';
	for (const std::string& cell : cells) {
		out += cell.starts_with(':') ? ":---" : "---";
		out += cell.ends_with(':') ? ":|" : "|";
	}
	return out;
}

auto gse::ide::format::rewrite_heading(const std::string_view line) -> std::optional<std::string> {
	const std::string_view rest = trim(line);
	const std::size_t hashes = rest.find_first_not_of('#');
	if (hashes == std::string_view::npos || hashes == 0 || hashes > max_heading_level) {
		return std::nullopt;
	}
	if (rest[hashes] != ' ' && rest[hashes] != '\t') {
		return std::nullopt;
	}

	std::string_view body = trim(rest.substr(hashes));
	const std::size_t closing = body.find_last_not_of('#');
	if (closing == std::string_view::npos) {
		body = {};
	}
	else if (closing + 1 < body.size() && (body[closing] == ' ' || body[closing] == '\t')) {
		body = trim_back(body.substr(0, closing));
	}

	std::string out(lead_of(line));
	out += rest.substr(0, hashes);
	if (!body.empty()) {
		out += ' ';
		out += body;
	}
	return out;
}

auto gse::ide::format::rewrite_bullet(const std::string_view line) -> std::optional<std::string> {
	const std::string_view rest = trim(line);
	if (rest.size() < 2 || (rest[0] != '-' && rest[0] != '*' && rest[0] != '+')) {
		return std::nullopt;
	}
	if (rest[1] != ' ' && rest[1] != '\t') {
		return std::nullopt;
	}
	std::string out(lead_of(line));
	out += "- ";
	out += trim(rest.substr(1));
	return out;
}

auto gse::ide::format::rewrite_ordered(const std::string_view line) -> std::optional<std::string> {
	const std::string_view rest = trim(line);
	const std::size_t digits = rest.find_first_not_of("0123456789");
	if (digits == std::string_view::npos || digits == 0 || digits > max_ordered_digits) {
		return std::nullopt;
	}
	if (rest[digits] != '.' && rest[digits] != ')') {
		return std::nullopt;
	}
	if (digits + 1 >= rest.size() || (rest[digits + 1] != ' ' && rest[digits + 1] != '\t')) {
		return std::nullopt;
	}
	std::string out(lead_of(line));
	out += rest.substr(0, digits + 1);
	out += ' ';
	out += trim(rest.substr(digits + 1));
	return out;
}

auto gse::ide::format::rewrite_body(const std::string_view line, const flow_state& flow) -> std::string {
	if (trim(line).empty()) {
		return {};
	}
	if (thematic_break(line)) {
		return std::string(trim_back(line));
	}
	if (const std::optional<std::string> heading = rewrite_heading(line)) {
		return *heading;
	}
	if (flow.list_allowed) {
		if (const std::optional<std::string> bullet = rewrite_bullet(line)) {
			return *bullet;
		}
		if (const std::optional<std::string> ordered = rewrite_ordered(line)) {
			return *ordered;
		}
	}
	return std::string(trim_back(line));
}

auto gse::ide::format::verbatim_regions(const std::span<const std::string> lines, std::vector<char>& out) -> void {
	out.assign(lines.size(), 0);
	std::size_t start = 0;

	if (!lines.empty() && lines.front() == "---") {
		const auto tail = lines.subspan(1);
		const auto close = std::ranges::find_if(tail, [](const std::string& line) {
			return line == "---" || line == "...";
		});
		if (close != tail.end()) {
			start = static_cast<std::size_t>(std::ranges::distance(tail.begin(), close)) + 2;
			std::ranges::fill(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(start), 1);
		}
	}

	std::optional<fence_marker> open;
	for (std::size_t i = start; i < lines.size(); ++i) {
		if (open) {
			out[i] = 1;
			if (closes_fence(lines[i], *open)) {
				open.reset();
			}
			continue;
		}
		if (const std::optional<fence_marker> marker = fence_at(lines[i])) {
			out[i] = 1;
			open = marker;
			continue;
		}
		if (indent_width(lines[i]) >= code_indent) {
			out[i] = 1;
		}
	}
}

auto gse::ide::format::table_regions(const std::span<const std::string> lines, const std::span<const char> verbatim) -> std::vector<table_span> {
	std::vector<table_span> spans;
	for (std::size_t i = 0; i + 1 < lines.size(); ++i) {
		if (verbatim[i] || verbatim[i + 1]) {
			continue;
		}
		if (!trim(lines[i]).starts_with('|') || !delimiter_row(lines[i + 1])) {
			continue;
		}
		std::size_t last = i + 1;
		while (last + 1 < lines.size() && !verbatim[last + 1] && trim(lines[last + 1]).starts_with('|')) {
			++last;
		}
		spans.push_back({ .first = i, .last = last });
		i = last;
	}
	return spans;
}

auto gse::ide::format::push_edit(std::vector<line_edit>& edits, const std::size_t line, const std::string_view before, const std::string_view after) -> void {
	if (before == after) {
		return;
	}
	const std::size_t limit = std::min(before.size(), after.size());
	std::size_t shared = 0;
	while (shared < limit && before[before.size() - 1 - shared] == after[after.size() - 1 - shared]) {
		++shared;
	}
	edits.push_back({
		.line = static_cast<std::uint32_t>(line),
		.expected = std::string(before.substr(0, before.size() - shared)),
		.replacement = std::string(after.substr(0, after.size() - shared)),
	});
}

auto gse::ide::format::compute_markdown(const std::span<const std::string> lines) -> std::vector<line_edit> {
	std::vector<char> verbatim;
	verbatim_regions(lines, verbatim);

	const std::vector<table_span> tables = table_regions(lines, verbatim);
	std::vector<char> tabled(lines.size(), 0);
	std::vector<line_edit> edits;

	for (const table_span& table : tables) {
		for (std::size_t i = table.first; i <= table.last; ++i) {
			tabled[i] = 1;
			const table_row row = split_cells(lines[i]);
			if (row.ambiguous) {
				continue;
			}
			const std::string_view lead = lead_of(lines[i]);
			const std::string canonical = i == table.first + 1
				? canonical_delimiters(lead, row.cells)
				: canonical_cells(lead, row.cells);
			push_edit(edits, i, lines[i], canonical);
		}
	}

	flow_state flow;
	for (std::size_t i = 0; i < lines.size(); ++i) {
		if (verbatim[i] || tabled[i]) {
			flow.list_allowed = true;
			continue;
		}
		const bool listish = flow.list_allowed
			&& (rewrite_bullet(lines[i]).has_value() || rewrite_ordered(lines[i]).has_value());
		const bool boundary = listish
			|| trim(lines[i]).empty()
			|| thematic_break(lines[i])
			|| rewrite_heading(lines[i]).has_value();
		push_edit(edits, i, lines[i], rewrite_body(lines[i], flow));
		flow.list_allowed = boundary;
	}

	std::ranges::sort(edits, {}, &line_edit::line);
	return edits;
}
