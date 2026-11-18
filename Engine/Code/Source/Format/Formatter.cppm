export module gse.format:formatter;

import std;
import gse.syntax;

export namespace gse::format {
	struct options {
		int indent_width = 4;
		bool indent_with_spaces = false;
		int column_limit = 100;
	};

	struct line_edit {
		std::uint32_t line = 0;
		std::string expected;
		std::string replacement;
	};

	auto compute(
		std::span<const std::string> lines,
		const options& opts
	) -> std::vector<line_edit>;

	auto apply(
		std::vector<std::string>& lines,
		std::span<const line_edit> edits
	) -> std::size_t;

	struct block_edit {
		std::uint32_t first_line = 0;
		std::uint32_t last_line = 0;
		std::vector<std::string> replacement;
	};

	auto compute_imports(
		std::span<const std::string> lines
	) -> std::optional<block_edit>;

	auto apply_block(
		std::vector<std::string>& lines,
		const block_edit& edit
	) -> bool;

	auto compute_annotations(
		std::span<const std::string> lines,
		const options& opts
	) -> std::vector<block_edit>;

	auto compute_initializers(
		std::span<const std::string> lines,
		const options& opts
	) -> std::vector<block_edit>;

	auto compute_signatures(
		std::span<const std::string> lines,
		const options& opts
	) -> std::vector<block_edit>;

	auto compute_calls(
		std::span<const std::string> lines,
		const options& opts
	) -> std::vector<block_edit>;

	auto compute_templates(
		std::span<const std::string> lines,
		const options& opts
	) -> std::vector<block_edit>;

	auto compute_case_bodies(
		std::span<const std::string> lines,
		const options& opts
	) -> std::vector<block_edit>;

	auto compute_else_breaks(
		std::span<const std::string> lines,
		const options& opts
	) -> std::vector<block_edit>;

	auto compute_enum_bodies(
		std::span<const std::string> lines,
		const options& opts
	) -> std::vector<block_edit>;

	auto compute_body_braces(
		std::span<const std::string> lines,
		const options& opts
	) -> std::vector<block_edit>;

	auto compute_blank_lines(
		std::span<const std::string> lines,
		const options& opts
	) -> std::vector<block_edit>;

	struct format_result {
		std::vector<block_edit> blocks;
		std::vector<line_edit> edits;
		std::size_t reindented = 0;
	};

	auto format_lines(
		std::vector<std::string>& lines,
		const options& opts
	) -> format_result;
}

namespace gse::format {
	enum class directive_kind {
		none,
		conditional_begin,
		conditional_alternative,
		conditional_else,
		conditional_end,
	};

	struct bracket {
		char kind = '{';
		int open_indent = 0;
		int content_indent = 0;
		bool switch_body = false;
		std::uint32_t open_line = 0;
		int angle_level = 0;
	};

	struct formatter_state {
		std::vector<bracket> stack;
		int paren_depth = 0;
		bool switch_pending = false;
		int switch_paren_base = 0;
		char last_code = ';';
		bool continuation = false;
		int statement_indent = 0;
		int angle_depth = 0;
	};

	struct conditional_state {
		formatter_state entry;
		std::optional<formatter_state> completed_branch;
		bool has_else = false;
	};

	auto is_opener(char c) -> bool;
	auto is_closer(char c) -> bool;
	auto matching_opener(char c) -> char;
	auto starts_statement(char c) -> bool;
	auto ends_statement(char c) -> bool;
	auto clears_continuation(char c) -> bool;
	auto punct_starts_statement(
		std::string_view text,
		char last_code
	) -> bool;
	auto directive_of(std::string_view text) -> directive_kind;
	auto continues_directive(std::string_view text) -> bool;
	auto structurally_equivalent(
		const formatter_state& left,
		const formatter_state& right
	) -> bool;
	auto merge_branch(
		conditional_state& conditional,
		const formatter_state& branch
	) -> bool;
	auto has_matching_angle(
		std::span<const syntax::token> tokens,
		std::size_t token_index
	) -> bool;
	auto is_comment(syntax::token_type type) -> bool;
	auto is_label_keyword(std::string_view w) -> bool;
	auto leading_whitespace(std::string_view s) -> std::string_view;
	auto indent_of(std::string_view ws, int width) -> int;
	auto make_indent(
		int level,
		int width,
		const options& opts
	) -> std::string;
	auto leading_closer_count(
		std::span<const syntax::token> line_tokens
	) -> int;
	auto is_access_specifier(
		std::span<const syntax::token> line_tokens
	) -> bool;

	struct import_line {
		std::string text;
		std::string name;
		bool partition = false;
	};

	auto trimmed(std::string_view s) -> std::string_view;

	auto parse_import(
		std::string_view s
	) -> std::optional<import_line>;

	struct source_position {
		std::uint32_t line = 0;
		std::uint32_t column = 0;
	};

	struct literal_list {
		source_position open_end;
		source_position close;
		std::vector<std::string_view> literals;
	};

	struct annotation_item {
		source_position start;
		source_position end;
		std::vector<literal_list> lists;
	};

	struct annotation_block {
		std::uint32_t first_line = 0;
		std::uint32_t last_line = 0;
		std::vector<annotation_item> items;
	};

	auto opens_template(
		std::span<const syntax::token> tokens,
		std::size_t token_index
	) -> bool;

	auto parse_annotation(
		std::span<const syntax::token> tokens,
		std::size_t open_index
	) -> std::optional<annotation_block>;

	auto render_annotation(
		std::span<const std::string> lines,
		const annotation_block& block,
		const options& opts
	) -> std::vector<std::string>;

	struct line_writer {
		std::span<const std::string> lines;
		bool keep_blank = false;
		std::vector<std::string> out;
		std::string current;

		auto trim_end() -> void;
		auto flush() -> void;

		auto copy(
			source_position from,
			source_position to
		) -> void;
	};

	struct designated_list;

	struct designated_item {
		source_position start;
		source_position end;
		std::vector<designated_list> children;
	};

	struct designated_list {
		source_position open;
		source_position close;
		std::vector<designated_item> items;
	};

	auto advance_char(
		std::span<const syntax::token> tokens,
		std::size_t& token_index,
		std::size_t& char_index
	) -> void;

	auto parse_designated(
		std::span<const syntax::token> tokens,
		std::size_t& token_index,
		std::size_t& char_index
	) -> std::optional<designated_list>;

	auto visual_width(
		std::string_view s,
		int width
	) -> int;

	auto suffix_width(
		std::span<const std::string> lines,
		source_position close,
		source_position limit
	) -> int;

	auto flat_designated(
		std::span<const std::string> lines,
		const designated_list& list
	) -> std::optional<std::string>;

	auto write_designated(
		line_writer& writer,
		const designated_list& list,
		int suffix,
		const options& opts
	) -> void;

	struct code_unit {
		std::size_t token = 0;
		char c = '\0';
		source_position start;
		source_position end;
	};

	enum class scope_kind {
		space,
		record,
		other,
	};

	struct parameter_range {
		std::size_t first = 0;
		std::size_t last = 0;
	};

	auto flatten_units(
		std::span<const syntax::token> tokens
	) -> std::vector<code_unit>;

	auto word_of(
		std::span<const syntax::token> tokens,
		const code_unit& unit
	) -> std::string_view;

	auto excluded_name(
		std::string_view word
	) -> bool;

	auto opens_angle(
		std::span<const syntax::token> tokens,
		std::span<const code_unit> units,
		std::size_t index
	) -> bool;

	auto closes_angle(
		std::span<const code_unit> units,
		std::size_t index
	) -> bool;

	auto valid_tail(
		std::span<const syntax::token> tokens,
		std::span<const code_unit> units,
		std::size_t close
	) -> bool;

	auto argument_ranges(
		std::span<const syntax::token> tokens,
		std::span<const code_unit> units,
		std::size_t open,
		std::size_t close
	) -> std::optional<std::vector<parameter_range>>;

	auto parameter_ranges(
		std::span<const syntax::token> tokens,
		std::span<const code_unit> units,
		std::size_t open,
		std::size_t close
	) -> std::optional<std::vector<parameter_range>>;

	auto render_parenthesised(
		std::span<const std::string> lines,
		std::span<const code_unit> units,
		std::span<const parameter_range> parameters,
		std::size_t open,
		std::size_t close,
		bool wrap,
		const options& opts
	) -> std::vector<std::string>;

	struct line_break {
		std::uint32_t line = 0;
		std::uint32_t from = 0;
		std::uint32_t to = 0;
		bool split = false;
	};

	auto render_breaks(
		std::span<const std::string> lines,
		std::vector<line_break>& breaks
	) -> std::vector<block_edit>;

	enum class brace_kind {
		scope,
		function,
		other,
	};

	auto classify_brace(
		std::span<const syntax::token> tokens,
		std::span<const code_unit> units,
		std::size_t first,
		std::size_t open
	) -> brace_kind;
}

auto gse::format::is_opener(const char c) -> bool {
	return c == '(' || c == '[' || c == '{';
}

auto gse::format::is_closer(const char c) -> bool {
	return c == ')' || c == ']' || c == '}';
}

auto gse::format::matching_opener(const char c) -> char {
	switch (c) {
		case ')':
			return '(';
		case ']':
			return '[';
		case '}':
			return '{';
		default:
			return '\0';
	}
}

auto gse::format::starts_statement(const char c) -> bool {
	return c == ';' || c == '{' || c == '}' || c == ',' || c == ':' || c == '(' || c == '[' || c == ']' || c == '>';
}

auto gse::format::ends_statement(const char c) -> bool {
	return c == ';' || c == '{' || c == '}' || c == '>';
}

auto gse::format::clears_continuation(const char c) -> bool {
	return ends_statement(c) || c == ',' || c == '(' || c == '[';
}

auto gse::format::punct_starts_statement(const std::string_view text, const char last_code) -> bool {
	const char c = text.front();
	if (is_closer(c) || c == '{' || c == '[' || c == '(' || c == '~' || c == '^') {
		return true;
	}
	if (c == '+') {
		return text.size() > 1 && text[1] == '[';
	}
	if (c == '.') {
		return last_code == '{' || last_code == ',';
	}
	return false;
}

auto gse::format::directive_of(const std::string_view text) -> directive_kind {
	std::size_t i = 1;
	while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) {
		++i;
	}
	const std::string_view directive = text.substr(i);
	if (directive == "if" || directive == "ifdef" || directive == "ifndef") {
		return directive_kind::conditional_begin;
	}
	if (directive == "elif" || directive == "elifdef" || directive == "elifndef") {
		return directive_kind::conditional_alternative;
	}
	if (directive == "else") {
		return directive_kind::conditional_else;
	}
	if (directive == "endif") {
		return directive_kind::conditional_end;
	}
	return directive_kind::none;
}

auto gse::format::continues_directive(const std::string_view text) -> bool {
	const std::size_t last = text.find_last_not_of(" \t\r");
	return last != std::string_view::npos && text[last] == '\\';
}

auto gse::format::structurally_equivalent(const formatter_state& left, const formatter_state& right) -> bool {
	if (left.paren_depth != right.paren_depth || left.switch_pending != right.switch_pending || left.switch_paren_base != right.switch_paren_base || left.angle_depth != right.angle_depth || left.stack.size() != right.stack.size()) {
		return false;
	}
	for (std::size_t i = 0; i < left.stack.size(); ++i) {
		const bracket& a = left.stack[i];
		const bracket& b = right.stack[i];
		if (a.kind != b.kind || a.open_indent != b.open_indent || a.content_indent != b.content_indent || a.switch_body != b.switch_body) {
			return false;
		}
	}
	return true;
}

auto gse::format::merge_branch(conditional_state& conditional, const formatter_state& branch) -> bool {
	if (!conditional.completed_branch) {
		conditional.completed_branch = branch;
		return true;
	}
	return structurally_equivalent(*conditional.completed_branch, branch);
}

auto gse::format::has_matching_angle(const std::span<const syntax::token> tokens, const std::size_t token_index) -> bool {
	int depth = 1;
	for (std::size_t i = token_index + 1; i < tokens.size(); ++i) {
		const syntax::token& token = tokens[i];
		if (token.type != syntax::token_type::punctuation) {
			continue;
		}
		if (token.text.contains("&&") || token.text.contains("||") || token.text.contains("==") || token.text.contains("!=") || token.text.contains("<=") || token.text.contains(">=") || token.text.contains('?')) {
			return false;
		}
		for (const char c : token.text) {
			if (c == '<') {
				++depth;
			}
			else if (c == '>') {
				--depth;
				if (depth == 0) {
					return true;
				}
			}
			else if (c == ';' || c == '{' || c == '}') {
				return false;
			}
		}
	}
	return false;
}

auto gse::format::is_comment(const syntax::token_type type) -> bool {
	return type == syntax::token_type::line_comment || type == syntax::token_type::block_comment;
}

auto gse::format::is_label_keyword(const std::string_view w) -> bool {
	return w == "case" || w == "default";
}

auto gse::format::leading_whitespace(const std::string_view s) -> std::string_view {
	std::size_t i = 0;
	while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
		++i;
	}
	return s.substr(0, i);
}

auto gse::format::indent_of(const std::string_view ws, const int width) -> int {
	int col = 0;
	for (const char c : ws) {
		col = c == '\t' ? (col / width + 1) * width : col + 1;
	}
	return col / width;
}

auto gse::format::make_indent(const int level, const int width, const options& opts) -> std::string {
	if (opts.indent_with_spaces) {
		return std::string(static_cast<std::size_t>(level) * width, ' ');
	}
	return std::string(static_cast<std::size_t>(level), '\t');
}

auto gse::format::leading_closer_count(const std::span<const syntax::token> line_tokens) -> int {
	int count = 0;
	for (const syntax::token& t : line_tokens) {
		if (t.type != syntax::token_type::punctuation) {
			return count;
		}
		for (const char c : t.text) {
			if (!is_closer(c)) {
				return count;
			}
			++count;
		}
	}
	return count;
}

auto gse::format::is_access_specifier(const std::span<const syntax::token> line_tokens) -> bool {
	if (line_tokens.size() < 2) {
		return false;
	}
	const syntax::token& first = line_tokens[0];
	const syntax::token& second = line_tokens[1];
	if (first.type != syntax::token_type::identifier || second.type != syntax::token_type::punctuation) {
		return false;
	}
	if (first.text != "public" && first.text != "private" && first.text != "protected") {
		return false;
	}
	return second.text.front() == ':' && second.text != "::";
}

auto gse::format::compute(const std::span<const std::string> lines, const options& opts) -> std::vector<line_edit> {
	std::vector<line_edit> edits;

	std::vector<std::string_view> views;
	views.reserve(lines.size());
	for (const std::string& l : lines) {
		views.emplace_back(l);
	}

	const syntax::lex_result lex = syntax::tokenize(views);
	const int width = std::max(1, opts.indent_width);

	formatter_state state;
	std::vector<conditional_state> conditionals;
	bool directive_continuation = false;
	std::size_t token_index = 0;

	for (std::size_t li = 0; li < views.size(); ++li) {
		const std::string_view s = views[li];
		const auto line = static_cast<std::uint32_t>(li);

		const std::size_t first_token = token_index;
		while (token_index < lex.tokens.size() && lex.tokens[token_index].line == line) {
			++token_index;
		}
		const std::span<const syntax::token> line_tokens{ lex.tokens.data() + first_token, token_index - first_token };

		const std::string_view ws = leading_whitespace(s);
		const bool normal_start = lex.line_start_modes[li] == syntax::lex_mode::normal;
		if (directive_continuation) {
			directive_continuation = continues_directive(s);
			continue;
		}

		if (line_tokens.empty()) {
			if (normal_start && !ws.empty() && ws.size() == s.size()) {
				edits.push_back({
					.line = line,
					.expected = std::string(ws),
					.replacement = "",
				});
			}
			continue;
		}

		if (std::ranges::all_of(line_tokens, is_comment, &syntax::token::type)) {
			continue;
		}

		const syntax::token& first = line_tokens.front();

		if (first.type == syntax::token_type::preprocessor) {
			if (normal_start && !ws.empty()) {
				edits.push_back({
					.line = line,
					.expected = std::string(ws),
					.replacement = "",
				});
			}
			const directive_kind directive = directive_of(first.text);
			if (directive == directive_kind::conditional_begin) {
				conditionals.push_back({ .entry = state });
			}
			else if (directive == directive_kind::conditional_alternative || directive == directive_kind::conditional_else) {
				if (conditionals.empty() || conditionals.back().has_else || !merge_branch(conditionals.back(), state)) {
					return {};
				}
				conditional_state& conditional = conditionals.back();
				conditional.has_else = directive == directive_kind::conditional_else;
				state = conditional.entry;
			}
			else if (directive == directive_kind::conditional_end) {
				if (conditionals.empty()) {
					return {};
				}
				conditional_state conditional = std::move(conditionals.back());
				conditionals.pop_back();
				if (!merge_branch(conditional, state) || (!conditional.has_else && !merge_branch(conditional, conditional.entry))) {
					return {};
				}
				state = std::move(*conditional.completed_branch);
				state.last_code = ';';
				state.continuation = false;
			}
			else {
				state.last_code = ';';
				state.continuation = false;
			}
			directive_continuation = continues_directive(s);
			continue;
		}

		const char trigger = state.last_code;
		const bool in_annotation = !state.stack.empty() && state.stack.back().kind == '[' && (trigger == '[' || trigger == ',');
		const bool annotation_start = in_annotation && first.type == syntax::token_type::punctuation && first.text.front() == '=';
		const bool operator_start = first.type == syntax::token_type::punctuation && !annotation_start && !punct_starts_statement(first.text, trigger);
		const bool chain_start = first.type == syntax::token_type::punctuation
			&& (first.text.front() == '.' || (first.text.size() > 1 && first.text[0] == '-' && first.text[1] == '>'));
		const bool angle_closer = first.type == syntax::token_type::punctuation && first.text.front() == '>';
		const bool in_angle_block = !state.stack.empty() && state.stack.back().kind == '<' && state.angle_depth == state.stack.back().angle_level;
		const bool angle_line = in_angle_block && normal_start && !is_comment(first.type)
			&& (trigger == '<' || trigger == ',' || angle_closer || (trigger == '"' && first.type == syntax::token_type::string));
		const bool eligible = angle_line || (normal_start && !state.continuation && state.angle_depth == 0
			&& starts_statement(trigger) && !is_comment(first.type) && !operator_start);

		const syntax::token* last_code_token = nullptr;
		for (const syntax::token& t : line_tokens) {
			if (!is_comment(t.type)) {
				last_code_token = &t;
			}
		}

		int effective = indent_of(ws, width);
		if (eligible) {
			int level = 0;
			if (angle_line) {
				level = angle_closer ? state.stack.back().open_indent : state.stack.back().content_indent;
			}
			else if (const int closers = leading_closer_count(line_tokens); closers > 0) {
				level = state.stack.empty() ? 0 : state.stack.back().open_indent;
			}
			else if (!state.stack.empty()) {
				const bracket& top = state.stack.back();
				if (is_access_specifier(line_tokens)) {
					level = top.open_indent;
				}
				else if (top.switch_body && !(first.type == syntax::token_type::identifier && is_label_keyword(first.text))) {
					level = top.content_indent + 1;
				}
				else {
					level = top.content_indent;
				}
			}
			level = std::max(level, 0);
			effective = level;
			if (ends_statement(trigger) || trigger == ':') {
				state.statement_indent = level;
			}

			if (std::string replacement = make_indent(level, width, opts); replacement != ws) {
				edits.push_back({
					.line = line,
					.expected = std::string(ws),
					.replacement = std::move(replacement),
				});
			}
		}

		int cross_line_pop = -1;
		const syntax::token* prev_token = nullptr;
		for (const syntax::token& t : line_tokens) {
			if (is_comment(t.type)) {
				continue;
			}
			if (t.type != syntax::token_type::punctuation) {
				state.last_code = t.text.back();
				if (t.type == syntax::token_type::identifier && t.text == "switch") {
					state.switch_pending = true;
					state.switch_paren_base = state.paren_depth;
				}
				prev_token = &t;
				continue;
			}
			const bool after_ident = prev_token && prev_token->type == syntax::token_type::identifier && prev_token->end == t.start;
			prev_token = &t;
			for (std::size_t ci = 0; ci < t.text.size(); ++ci) {
				const char c = t.text[ci];
				if (c == '<' && ci == 0 && t.text == "<" && after_ident && has_matching_angle(lex.tokens, static_cast<std::size_t>(&t - lex.tokens.data()))) {
					++state.angle_depth;
					if (&t == last_code_token) {
						state.stack.push_back({
							.kind = '<',
							.open_indent = effective,
							.content_indent = effective + 1,
							.open_line = line,
							.angle_level = state.angle_depth,
						});
					}
				}
				else if (c == '>' && state.angle_depth > 0 && !(ci > 0 && t.text[ci - 1] == '-')) {
					if (!state.stack.empty() && state.stack.back().kind == '<' && state.stack.back().angle_level == state.angle_depth) {
						state.stack.pop_back();
					}
					--state.angle_depth;
				}
				else if (c == ';' || c == '{' || c == '}') {
					const auto block = std::ranges::find(state.stack | std::views::reverse, '<', &bracket::kind);
					state.angle_depth = block == std::ranges::end(state.stack | std::views::reverse) ? 0 : block->angle_level;
				}
				if (is_opener(c)) {
					const bool body = c == '{' && state.switch_pending && state.paren_depth == state.switch_paren_base;
					if (body) {
						state.switch_pending = false;
					}
					if (c == '(') {
						++state.paren_depth;
					}
					int anchor = effective;
					if (c == '{') {
						if (cross_line_pop >= 0) {
							anchor = cross_line_pop;
						}
						else if (!eligible && operator_start && !chain_start) {
							anchor = state.statement_indent;
						}
					}
					state.stack.push_back({
						.kind = c,
						.open_indent = anchor,
						.content_indent = anchor + 1,
						.switch_body = body,
						.open_line = line,
					});
				}
				else if (is_closer(c)) {
					if (state.stack.empty() || state.stack.back().kind != matching_opener(c)) {
						return {};
					}
					if (state.stack.back().kind == '(') {
						--state.paren_depth;
					}
					if (state.stack.back().open_line != line) {
						cross_line_pop = state.stack.back().open_indent;
					}
					state.stack.pop_back();
				}
				else if (c == ';') {
					state.switch_pending = false;
				}
				state.last_code = c;
			}
		}

		state.continuation = !eligible && !clears_continuation(state.last_code);
	}
	if (!conditionals.empty() || directive_continuation || !state.stack.empty()) {
		return {};
	}

	return edits;
}

auto gse::format::trimmed(const std::string_view s) -> std::string_view {
	const auto first = s.find_first_not_of(" \t\r");
	if (first == std::string_view::npos) {
		return {};
	}
	return s.substr(first, s.find_last_not_of(" \t\r") - first + 1);
}

auto gse::format::parse_import(const std::string_view s) -> std::optional<import_line> {
	std::string_view text = trimmed(s);
	if (!text.ends_with(";")) {
		return std::nullopt;
	}
	const bool exported = text.starts_with("export ");
	if (exported) {
		text = trimmed(text.substr(std::string_view("export ").size()));
	}
	if (!text.starts_with("import ")) {
		return std::nullopt;
	}
	text.remove_suffix(1);
	const std::string_view name = trimmed(text.substr(std::string_view("import ").size()));
	if (name.empty()) {
		return std::nullopt;
	}
	return import_line{
		.text = (exported ? "export import " : "import ") + std::string(name) + ";",
		.name = std::string(name),
		.partition = name.front() == ':',
	};
}

auto gse::format::compute_imports(const std::span<const std::string> lines) -> std::optional<block_edit> {
	std::size_t first = lines.size();
	std::size_t last = 0;
	std::size_t module_line = lines.size();
	std::vector<import_line> imports;

	for (std::size_t index = 0; index < lines.size(); ++index) {
		const std::string_view s = trimmed(lines[index]);
		if (s.empty()) {
			continue;
		}
		if (std::optional<import_line> parsed = parse_import(s)) {
			if (imports.empty()) {
				first = index;
			}
			last = index;
			imports.push_back(std::move(*parsed));
			continue;
		}
		if (!imports.empty()) {
			break;
		}
		if (s.starts_with("module ") || s.starts_with("export module ") || s == "module;") {
			module_line = index;
			continue;
		}
		if (s.front() != '#') {
			break;
		}
	}

	if (imports.empty()) {
		return std::nullopt;
	}

	std::vector<import_line> plain;
	std::vector<import_line> partitions;
	for (import_line& entry : imports) {
		(entry.partition ? partitions : plain).push_back(std::move(entry));
	}
	const auto by_name = [](const import_line& a, const import_line& b) {
		return a.name < b.name;
	};
	std::ranges::stable_sort(plain, by_name);
	std::ranges::stable_sort(partitions, by_name);

	std::vector<std::string> replacement;
	replacement.reserve(plain.size() + partitions.size() + 1);
	for (const import_line& entry : plain) {
		replacement.push_back(entry.text);
	}
	if (!plain.empty() && !partitions.empty()) {
		replacement.emplace_back();
	}
	for (const import_line& entry : partitions) {
		replacement.push_back(entry.text);
	}

	if (module_line < first) {
		const std::span<const std::string> gap = lines.subspan(module_line + 1, first - module_line - 1);
		if (std::ranges::all_of(gap, [](const std::string& line) { return trimmed(line).empty(); })) {
			first = module_line + 1;
			replacement.insert(replacement.begin(), std::string());
		}
	}

	if (replacement.size() == last - first + 1 && std::ranges::equal(replacement, lines.subspan(first, replacement.size()))) {
		return std::nullopt;
	}

	return block_edit{
		.first_line = static_cast<std::uint32_t>(first),
		.last_line = static_cast<std::uint32_t>(last),
		.replacement = std::move(replacement),
	};
}

auto gse::format::apply_block(std::vector<std::string>& lines, const block_edit& edit) -> bool {
	if (edit.first_line > edit.last_line || edit.last_line >= lines.size()) {
		return false;
	}
	lines.erase(lines.begin() + edit.first_line, lines.begin() + edit.last_line + 1);
	lines.insert(lines.begin() + edit.first_line, edit.replacement.begin(), edit.replacement.end());
	return true;
}

auto gse::format::opens_template(const std::span<const syntax::token> tokens, const std::size_t token_index) -> bool {
	const syntax::token& t = tokens[token_index];
	if (t.type != syntax::token_type::punctuation || t.text != "<" || token_index == 0) {
		return false;
	}
	const syntax::token& prev = tokens[token_index - 1];
	return prev.type == syntax::token_type::identifier && prev.line == t.line && prev.end == t.start && has_matching_angle(tokens, token_index);
}

auto gse::format::parse_annotation(const std::span<const syntax::token> tokens, const std::size_t open_index) -> std::optional<annotation_block> {
	annotation_block block{ .first_line = tokens[open_index].line };
	annotation_item current;
	std::vector<std::size_t> angles;
	int depth = 0;
	bool expect_item = true;

	for (std::size_t i = open_index; i < tokens.size(); ++i) {
		const syntax::token& t = tokens[i];
		if (is_comment(t.type) || t.type == syntax::token_type::preprocessor) {
			return std::nullopt;
		}
		if (t.type != syntax::token_type::punctuation) {
			if (expect_item) {
				return std::nullopt;
			}
			continue;
		}
		for (std::size_t ci = i == open_index ? 2 : 0; ci < t.text.size(); ++ci) {
			const char c = t.text[ci];
			const source_position here{
				.line = t.line,
				.column = t.start + static_cast<std::uint32_t>(ci),
			};
			if (expect_item) {
				if (c != '=') {
					return std::nullopt;
				}
				current = { .start = here };
				expect_item = false;
				continue;
			}
			if (c == '<' && opens_template(tokens, i)) {
				angles.push_back(i);
				continue;
			}
			if (c == '>' && !angles.empty() && !(ci > 0 && t.text[ci - 1] == '-')) {
				const std::size_t open_angle = angles.back();
				angles.pop_back();
				const auto inner = tokens.subspan(open_angle + 1, i - open_angle - 1);
				if (ci == 0 && inner.size() >= 2 && std::ranges::all_of(inner, [](const syntax::token& s) { return s.type == syntax::token_type::string; })) {
					literal_list list{
						.open_end = { .line = tokens[open_angle].line, .column = tokens[open_angle].end },
						.close = here,
					};
					for (const syntax::token& s : inner) {
						list.literals.push_back(s.text);
					}
					current.lists.push_back(std::move(list));
				}
				continue;
			}
			if (is_opener(c)) {
				++depth;
				continue;
			}
			if (c == ',' && depth == 0 && angles.empty()) {
				current.end = here;
				block.items.push_back(std::move(current));
				expect_item = true;
				continue;
			}
			if (!is_closer(c)) {
				continue;
			}
			if (depth > 0) {
				--depth;
				continue;
			}
			if (c != ']' || !angles.empty() || ci + 2 != t.text.size() || t.text[ci + 1] != ']') {
				return std::nullopt;
			}
			if (i + 1 < tokens.size() && tokens[i + 1].line == t.line) {
				return std::nullopt;
			}
			current.end = here;
			block.items.push_back(std::move(current));
			block.last_line = t.line;
			return block;
		}
	}
	return std::nullopt;
}

auto gse::format::render_annotation(const std::span<const std::string> lines, const annotation_block& block, const options& opts) -> std::vector<std::string> {
	const int width = std::max(1, opts.indent_width);
	const std::string ws(leading_whitespace(lines[block.first_line]));
	const int base = indent_of(ws, width);
	const std::string item_indent = make_indent(base + 1, width, opts);
	const std::string literal_indent = make_indent(base + 2, width, opts);

	line_writer writer{ .lines = lines };
	writer.out.push_back(ws + "[[");

	for (std::size_t k = 0; k < block.items.size(); ++k) {
		const annotation_item& item = block.items[k];
		writer.current = item_indent;
		source_position from = item.start;
		for (const literal_list& list : item.lists) {
			writer.copy(from, list.open_end);
			writer.flush();
			for (const std::string_view literal : list.literals) {
				writer.out.push_back(literal_indent + std::string(literal));
			}
			writer.current = item_indent;
			from = list.close;
		}
		writer.copy(from, item.end);
		if (k + 1 < block.items.size()) {
			writer.trim_end();
			writer.current += ',';
		}
		writer.flush();
	}
	writer.out.push_back(ws + "]]");
	return std::move(writer.out);
}

auto gse::format::line_writer::trim_end() -> void {
	current.erase(current.find_last_not_of(" \t\r") + 1);
}

auto gse::format::line_writer::flush() -> void {
	trim_end();
	if (keep_blank || !trimmed(current).empty()) {
		out.push_back(std::move(current));
	}
	current.clear();
}

auto gse::format::line_writer::copy(const source_position from, const source_position to) -> void {
	const std::string_view first(lines[from.line]);
	if (from.line == to.line) {
		current += first.substr(from.column, to.column - from.column);
		return;
	}
	current += first.substr(from.column);
	flush();
	for (std::uint32_t l = from.line + 1; l < to.line; ++l) {
		current = lines[l];
		flush();
	}
	current = std::string_view(lines[to.line]).substr(0, to.column);
}

auto gse::format::compute_annotations(const std::span<const std::string> lines, const options& opts) -> std::vector<block_edit> {
	std::vector<std::string_view> views;
	views.reserve(lines.size());
	for (const std::string& l : lines) {
		views.emplace_back(l);
	}
	const syntax::lex_result lex = syntax::tokenize(views);
	const std::span<const syntax::token> tokens = lex.tokens;

	std::vector<block_edit> edits;
	for (std::size_t i = 0; i < tokens.size(); ++i) {
		const syntax::token& t = tokens[i];
		const bool line_start = i == 0 || tokens[i - 1].line != t.line;
		if (t.type != syntax::token_type::punctuation || !t.text.starts_with("[[") || !line_start || lex.line_start_modes[t.line] != syntax::lex_mode::normal) {
			continue;
		}
		const std::optional<annotation_block> block = parse_annotation(tokens, i);
		if (!block || block->items.size() < 2) {
			continue;
		}
		std::vector<std::string> replacement = render_annotation(lines, *block, opts);
		const std::size_t span = block->last_line - block->first_line + 1;
		if (replacement.size() != span || !std::ranges::equal(replacement, lines.subspan(block->first_line, span))) {
			edits.push_back({
				.first_line = block->first_line,
				.last_line = block->last_line,
				.replacement = std::move(replacement),
			});
		}
		while (i + 1 < tokens.size() && tokens[i + 1].line <= block->last_line) {
			++i;
		}
	}
	std::ranges::reverse(edits);
	return edits;
}

auto gse::format::advance_char(const std::span<const syntax::token> tokens, std::size_t& token_index, std::size_t& char_index) -> void {
	const syntax::token& t = tokens[token_index];
	if (t.type == syntax::token_type::punctuation && char_index + 1 < t.text.size()) {
		++char_index;
		return;
	}
	++token_index;
	char_index = 0;
}

auto gse::format::parse_designated(const std::span<const syntax::token> tokens, std::size_t& token_index, std::size_t& char_index) -> std::optional<designated_list> {
	designated_list list{
		.open = {
			.line = tokens[token_index].line,
			.column = tokens[token_index].start + static_cast<std::uint32_t>(char_index),
		},
	};
	designated_item current;
	source_position content_end;
	std::vector<std::size_t> angles;
	int depth = 0;
	bool expect_item = true;

	std::size_t ti = token_index;
	std::size_t ci = char_index;
	for (advance_char(tokens, ti, ci); ti < tokens.size(); advance_char(tokens, ti, ci)) {
		const syntax::token& t = tokens[ti];
		if (is_comment(t.type) || t.type == syntax::token_type::preprocessor) {
			return std::nullopt;
		}
		if (t.type != syntax::token_type::punctuation) {
			if (expect_item) {
				return std::nullopt;
			}
			content_end = { .line = t.line, .column = t.end };
			continue;
		}
		const char c = t.text[ci];
		const source_position here{
			.line = t.line,
			.column = t.start + static_cast<std::uint32_t>(ci),
		};
		const source_position after{
			.line = t.line,
			.column = here.column + 1,
		};
		if (expect_item) {
			if (c == '}' && !list.items.empty()) {
				list.close = here;
				token_index = ti;
				char_index = ci;
				return list;
			}
			const bool names_member = ci + 1 == t.text.size() && ti + 1 < tokens.size() && tokens[ti + 1].type == syntax::token_type::identifier;
			if (c != '.' || !names_member) {
				return std::nullopt;
			}
			current = { .start = here };
			content_end = after;
			expect_item = false;
			continue;
		}
		if (c == '{') {
			std::size_t child_ti = ti;
			std::size_t child_ci = ci;
			if (std::optional<designated_list> child = parse_designated(tokens, child_ti, child_ci)) {
				content_end = {
					.line = child->close.line,
					.column = child->close.column + 1,
				};
				current.children.push_back(std::move(*child));
				ti = child_ti;
				ci = child_ci;
				continue;
			}
		}
		const bool top = depth == 0 && angles.empty();
		if (c == ',' && top) {
			current.end = content_end;
			list.items.push_back(std::move(current));
			current = {};
			expect_item = true;
			continue;
		}
		if (c == '}' && top) {
			current.end = content_end;
			list.items.push_back(std::move(current));
			list.close = here;
			token_index = ti;
			char_index = ci;
			return list;
		}
		content_end = after;
		if (c == '<' && opens_template(tokens, ti)) {
			angles.push_back(ti);
			continue;
		}
		if (c == '>' && !angles.empty() && !(ci > 0 && t.text[ci - 1] == '-')) {
			angles.pop_back();
			continue;
		}
		if (is_opener(c)) {
			++depth;
			continue;
		}
		if (!is_closer(c)) {
			continue;
		}
		if (depth == 0) {
			return std::nullopt;
		}
		--depth;
	}
	return std::nullopt;
}

auto gse::format::visual_width(const std::string_view s, const int width) -> int {
	const std::string_view ws = leading_whitespace(s);
	return indent_of(ws, width) * width + static_cast<int>(s.size() - ws.size());
}

auto gse::format::suffix_width(const std::span<const std::string> lines, const source_position close, const source_position limit) -> int {
	const std::string_view line(lines[close.line]);
	const std::size_t from = std::min<std::size_t>(close.column + 1, line.size());
	const std::size_t to = limit.line == close.line ? std::min<std::size_t>(limit.column, line.size()) : line.size();
	const std::string_view rest = line.substr(from, to > from ? to - from : 0);
	return static_cast<int>(rest.find_last_not_of(" \t\r") + 1);
}

auto gse::format::flat_designated(const std::span<const std::string> lines, const designated_list& list) -> std::optional<std::string> {
	std::string out = "{ ";
	for (std::size_t k = 0; k < list.items.size(); ++k) {
		const designated_item& item = list.items[k];
		if (k > 0) {
			out += ", ";
		}
		source_position from = item.start;
		for (const designated_list& child : item.children) {
			if (child.open.line != from.line) {
				return std::nullopt;
			}
			const std::optional<std::string> inner = flat_designated(lines, child);
			if (!inner) {
				return std::nullopt;
			}
			out += std::string_view(lines[from.line]).substr(from.column, child.open.column - from.column);
			out += *inner;
			from = {
				.line = child.close.line,
				.column = child.close.column + 1,
			};
		}
		if (item.end.line != from.line) {
			return std::nullopt;
		}
		out += std::string_view(lines[from.line]).substr(from.column, item.end.column - from.column);
	}
	out += " }";
	return out;
}

auto gse::format::write_designated(line_writer& writer, const designated_list& list, const int suffix, const options& opts) -> void {
	const int width = std::max(1, opts.indent_width);
	if (const std::optional<std::string> flat = flat_designated(writer.lines, list)) {
		if (visual_width(writer.current, width) + static_cast<int>(flat->size()) + suffix <= opts.column_limit) {
			writer.current += *flat;
			return;
		}
	}
	const int level = indent_of(leading_whitespace(writer.current), width);
	writer.current += '{';
	writer.flush();
	for (const designated_item& item : list.items) {
		writer.current = make_indent(level + 1, width, opts);
		source_position from = item.start;
		for (std::size_t k = 0; k < item.children.size(); ++k) {
			const designated_list& child = item.children[k];
			writer.copy(from, child.open);
			const bool next_on_line = k + 1 < item.children.size() && item.children[k + 1].open.line == child.close.line;
			const source_position limit = next_on_line ? item.children[k + 1].open : item.end;
			write_designated(writer, child, suffix_width(writer.lines, child.close, limit) + 1, opts);
			from = {
				.line = child.close.line,
				.column = child.close.column + 1,
			};
		}
		writer.copy(from, item.end);
		writer.trim_end();
		writer.current += ',';
		writer.flush();
	}
	writer.current = make_indent(level, width, opts) + '}';
}

auto gse::format::compute_initializers(const std::span<const std::string> lines, const options& opts) -> std::vector<block_edit> {
	std::vector<std::string_view> views;
	views.reserve(lines.size());
	for (const std::string& l : lines) {
		views.emplace_back(l);
	}
	const syntax::lex_result lex = syntax::tokenize(views);
	const std::span<const syntax::token> tokens = lex.tokens;

	std::vector<block_edit> edits;
	std::vector<designated_list> group;

	const auto emit = [&] {
		if (group.empty()) {
			return;
		}
		const std::uint32_t first = group.front().open.line;
		const std::uint32_t last = group.back().close.line;
		const bool plain = std::ranges::all_of(std::views::iota(first, last + 1), [&](const std::uint32_t l) {
			return lex.line_start_modes[l] == syntax::lex_mode::normal;
		});
		if (plain) {
			line_writer writer{
				.lines = lines,
				.keep_blank = true,
			};
			source_position from{ .line = first };
			for (std::size_t k = 0; k < group.size(); ++k) {
				const designated_list& list = group[k];
				writer.copy(from, list.open);
				const bool next_on_line = k + 1 < group.size() && group[k + 1].open.line == list.close.line;
				const source_position limit = next_on_line ? group[k + 1].open : source_position{ .line = last + 1 };
				write_designated(writer, list, suffix_width(lines, list.close, limit), opts);
				from = {
					.line = list.close.line,
					.column = list.close.column + 1,
				};
			}
			writer.copy(from, {
				.line = last,
				.column = static_cast<std::uint32_t>(lines[last].size()),
			});
			writer.flush();

			const std::size_t span = last - first + 1;
			const auto same = [](const std::string& a, const std::string& b) {
				return trimmed(a) == trimmed(b);
			};
			if (!std::ranges::equal(writer.out, lines.subspan(first, span), same)) {
				edits.push_back({
					.first_line = first,
					.last_line = last,
					.replacement = std::move(writer.out),
				});
			}
		}
		group.clear();
	};

	std::size_t ti = 0;
	std::size_t ci = 0;
	while (ti < tokens.size()) {
		const syntax::token& t = tokens[ti];
		if (t.type == syntax::token_type::punctuation && t.text[ci] == '{') {
			std::size_t end_ti = ti;
			std::size_t end_ci = ci;
			if (std::optional<designated_list> list = parse_designated(tokens, end_ti, end_ci)) {
				if (!group.empty() && list->open.line > group.back().close.line) {
					emit();
				}
				group.push_back(std::move(*list));
				ti = end_ti;
				ci = end_ci;
			}
		}
		advance_char(tokens, ti, ci);
	}
	emit();

	std::ranges::reverse(edits);
	return edits;
}

auto gse::format::flatten_units(const std::span<const syntax::token> tokens) -> std::vector<code_unit> {
	std::vector<code_unit> units;
	units.reserve(tokens.size());
	for (std::size_t i = 0; i < tokens.size(); ++i) {
		const syntax::token& t = tokens[i];
		if (t.type != syntax::token_type::punctuation) {
			units.push_back({
				.token = i,
				.start = { .line = t.line, .column = t.start },
				.end = { .line = t.line, .column = t.end },
			});
			continue;
		}
		for (std::size_t ci = 0; ci < t.text.size(); ++ci) {
			const auto column = t.start + static_cast<std::uint32_t>(ci);
			units.push_back({
				.token = i,
				.c = t.text[ci],
				.start = { .line = t.line, .column = column },
				.end = { .line = t.line, .column = column + 1 },
			});
		}
	}
	return units;
}

auto gse::format::word_of(const std::span<const syntax::token> tokens, const code_unit& unit) -> std::string_view {
	if (unit.c != '\0' || tokens[unit.token].type != syntax::token_type::identifier) {
		return {};
	}
	return tokens[unit.token].text;
}

auto gse::format::excluded_name(const std::string_view word) -> bool {
	static constexpr std::array<std::string_view, 22> keywords{
		"alignas", "alignof", "decltype", "sizeof", "noexcept", "requires", "static_assert", "explicit",
		"if", "for", "while", "switch", "return", "typeid", "catch", "throw",
		"co_await", "co_return", "co_yield", "new", "delete", "template",
	};
	return std::ranges::contains(keywords, word);
}

auto gse::format::opens_angle(const std::span<const syntax::token> tokens, const std::span<const code_unit> units, const std::size_t index) -> bool {
	const code_unit& unit = units[index];
	if (unit.c != '<' || tokens[unit.token].text != "<") {
		return false;
	}
	return opens_template(tokens, unit.token) || (index > 0 && word_of(tokens, units[index - 1]) == "template");
}

auto gse::format::closes_angle(const std::span<const code_unit> units, const std::size_t index) -> bool {
	return units[index].c == '>' && !(index > 0 && units[index - 1].c == '-' && units[index - 1].token == units[index].token);
}

auto gse::format::valid_tail(const std::span<const syntax::token> tokens, const std::span<const code_unit> units, const std::size_t close) -> bool {
	if (close + 1 >= units.size()) {
		return false;
	}
	const code_unit& next = units[close + 1];
	if (const std::string_view word = word_of(tokens, next); !word.empty()) {
		return word == "const" || word == "volatile" || word == "noexcept" || word == "override" || word == "final" || word == "requires" || word == "try";
	}
	const bool joined = close + 2 < units.size() && units[close + 2].token == next.token;
	switch (next.c) {
		case ';':
		case '{':
		case '=':
		case '&':
			return true;
		case '-':
			return joined && units[close + 2].c == '>';
		case ':':
			return !(joined && units[close + 2].c == ':');
		default:
			return false;
	}
}

auto gse::format::parameter_ranges(const std::span<const syntax::token> tokens, const std::span<const code_unit> units, const std::size_t open, const std::size_t close) -> std::optional<std::vector<parameter_range>> {
	std::optional<std::vector<parameter_range>> ranges = argument_ranges(tokens, units, open, close);
	if (!ranges) {
		return std::nullopt;
	}
	for (const parameter_range& range : *ranges) {
		const code_unit& head = units[range.first];
		const code_unit& tail = units[range.last];
		const bool declares = !word_of(tokens, head).empty() || head.c == ':' || head.c == '[';
		if (!declares || range.last == range.first || head.start.line != tail.end.line) {
			return std::nullopt;
		}
	}
	return ranges;
}

auto gse::format::argument_ranges(const std::span<const syntax::token> tokens, const std::span<const code_unit> units, const std::size_t open, const std::size_t close) -> std::optional<std::vector<parameter_range>> {
	std::vector<parameter_range> ranges;
	std::vector<char> nest;
	std::size_t first = open + 1;
	for (std::size_t i = open + 1; i <= close; ++i) {
		const code_unit& unit = units[i];
		if (i < close && unit.c == '\0') {
			const syntax::token_type type = tokens[unit.token].type;
			if (is_comment(type) || type == syntax::token_type::preprocessor) {
				return std::nullopt;
			}
			continue;
		}
		const bool top = nest.empty();
		if (i == close || (unit.c == ',' && top)) {
			if (first == i) {
				return std::nullopt;
			}
			ranges.push_back({
				.first = first,
				.last = i - 1,
			});
			first = i + 1;
			continue;
		}
		if (unit.c == '(' || unit.c == '[' || unit.c == '{') {
			nest.push_back(unit.c);
		}
		else if (opens_angle(tokens, units, i)) {
			nest.push_back('<');
		}
		else if (closes_angle(units, i) && !nest.empty() && nest.back() == '<') {
			nest.pop_back();
		}
		else if (is_closer(unit.c)) {
			while (!nest.empty() && nest.back() == '<') {
				nest.pop_back();
			}
			if (nest.empty() || nest.back() != matching_opener(unit.c)) {
				return std::nullopt;
			}
			nest.pop_back();
		}
	}
	return ranges;
}

auto gse::format::render_parenthesised(const std::span<const std::string> lines, const std::span<const code_unit> units, const std::span<const parameter_range> parameters, const std::size_t open, const std::size_t close, const bool wrap, const options& opts) -> std::vector<std::string> {
	const std::string_view open_line(lines[units[open].start.line]);
	const std::string_view close_line(lines[units[close].start.line]);
	std::string head(open_line.substr(0, units[open].end.column));
	std::string tail(trimmed(close_line.substr(units[close].start.column)));

	const auto text_of = [&](const parameter_range& range) {
		const source_position from = units[range.first].start;
		const source_position to = units[range.last].end;
		return std::string(std::string_view(lines[from.line]).substr(from.column, to.column - from.column));
	};

	if (!wrap) {
		for (std::size_t k = 0; k < parameters.size(); ++k) {
			if (k > 0) {
				head += ", ";
			}
			head += text_of(parameters[k]);
		}
		return { head + tail };
	}

	const int width = std::max(1, opts.indent_width);
	const int base = indent_of(leading_whitespace(open_line), width);
	std::vector<std::string> out;
	out.push_back(std::move(head));
	for (std::size_t k = 0; k < parameters.size(); ++k) {
		const source_position from = units[parameters[k].first].start;
		const source_position to = units[parameters[k].last].end;
		const int reference = indent_of(leading_whitespace(lines[from.line]), width);
		for (std::uint32_t l = from.line; l <= to.line; ++l) {
			const std::string_view line(lines[l]);
			const std::size_t begin = l == from.line ? from.column : 0;
			const std::size_t end = l == to.line ? to.column : line.size();
			const std::string_view text = trimmed(line.substr(begin, end - begin));
			const int level = l == from.line ? base + 1 : std::max(base + 1, base + 1 + indent_of(leading_whitespace(line), width) - reference);
			out.push_back(text.empty() ? std::string() : make_indent(level, width, opts) + std::string(text));
		}
		if (k + 1 < parameters.size()) {
			out.back() += ',';
		}
	}
	out.push_back(make_indent(base, width, opts) + tail);
	return out;
}

auto gse::format::compute_signatures(const std::span<const std::string> lines, const options& opts) -> std::vector<block_edit> {
	std::vector<std::string_view> views;
	views.reserve(lines.size());
	for (const std::string& l : lines) {
		views.emplace_back(l);
	}
	const syntax::lex_result lex = syntax::tokenize(views);
	const std::span<const syntax::token> tokens = lex.tokens;
	const std::vector<code_unit> units = flatten_units(tokens);

	std::vector<block_edit> edits;
	std::vector<scope_kind> scopes;

	const auto skip_body = [&](std::size_t index) {
		int depth = 0;
		for (; index < units.size(); ++index) {
			if (units[index].c == '{') {
				++depth;
			}
			else if (units[index].c == '}' && --depth == 0) {
				return index + 1;
			}
		}
		return units.size();
	};

	const auto qualified_at = [&](std::size_t name) {
		if (name > 0 && units[name - 1].c == '~') {
			--name;
		}
		return name >= 2 && units[name - 1].c == ':' && units[name - 2].c == ':';
	};

	std::size_t u = 0;
	while (u < units.size()) {
		std::vector<char> nest;
		std::optional<std::size_t> open;
		std::optional<std::size_t> close;
		std::optional<std::size_t> operator_index;
		bool qualified = false;
		bool has_eq = false;
		bool poisoned = false;
		bool saw_word = false;
		bool only_linkage = true;
		bool is_namespace = false;
		bool is_record = false;
		bool is_enum = false;
		int symbols = 0;

		std::size_t v = u;
		for (; v < units.size(); ++v) {
			const code_unit& unit = units[v];
			const syntax::token& t = tokens[unit.token];
			if (unit.c == '\0') {
				if (t.type == syntax::token_type::preprocessor) {
					break;
				}
				if (is_comment(t.type)) {
					poisoned = poisoned || (open && !close);
					continue;
				}
				if (!nest.empty()) {
					continue;
				}
				const std::string_view word = word_of(tokens, unit);
				saw_word = saw_word || !word.empty();
				is_namespace = is_namespace || word == "namespace";
				is_record = is_record || word == "struct" || word == "class" || word == "union";
				is_enum = is_enum || word == "enum";
				only_linkage = only_linkage && (word == "export" || word == "extern" || t.type == syntax::token_type::string);
				if (word == "operator" && !open) {
					operator_index = v;
				}
				continue;
			}

			const char c = unit.c;
			if (!nest.empty()) {
				if (c == '(' || c == '[' || c == '{') {
					nest.push_back(c);
				}
				else if (opens_angle(tokens, units, v)) {
					nest.push_back('<');
				}
				else if (closes_angle(units, v) && nest.back() == '<') {
					nest.pop_back();
				}
				else if (is_closer(c)) {
					while (!nest.empty() && nest.back() == '<') {
						nest.pop_back();
					}
					if (nest.empty() || nest.back() != matching_opener(c)) {
						poisoned = true;
						nest.clear();
						continue;
					}
					nest.pop_back();
					if (nest.empty() && open && !close) {
						close = v;
					}
				}
				continue;
			}

			if (c == ';' || c == '{' || c == '}') {
				break;
			}
			if (c == '(') {
				if (!open && !has_eq) {
					if (operator_index) {
						if (symbols == 0 && v + 1 < units.size() && units[v + 1].c == ')') {
							symbols = 2;
							++v;
							continue;
						}
						open = v;
						qualified = qualified_at(*operator_index);
					}
					else if (const std::string_view name = v > 0 ? word_of(tokens, units[v - 1]) : std::string_view{}; !name.empty() && !excluded_name(name)) {
						open = v;
						qualified = qualified_at(v - 1);
					}
				}
				nest.push_back('(');
				continue;
			}
			if (operator_index && !open) {
				++symbols;
				continue;
			}
			if (c == '[') {
				nest.push_back('[');
			}
			else if (opens_angle(tokens, units, v)) {
				nest.push_back('<');
			}
			else if (c == '=' && !open) {
				has_eq = true;
			}
		}

		if (v >= units.size()) {
			break;
		}

		const char terminator = units[v].c;
		if (terminator == '\0') {
			u = v + 1;
			continue;
		}

		const scope_kind here = scopes.empty() ? scope_kind::space : scopes.back();
		const bool candidate = open && close && !poisoned && terminator != '}' && valid_tail(tokens, units, *close);
		const bool wrap = candidate && terminator == ';';
		const bool join = candidate && here == scope_kind::space && terminator == '{' && qualified;
		if (wrap || join) {
			const std::uint32_t first = units[*open].start.line;
			const std::uint32_t last = units[*close].start.line;
			const bool fresh = edits.empty() || first > edits.back().last_line;
			const bool plain = std::ranges::all_of(std::views::iota(first, last + 1), [&](const std::uint32_t l) {
				return lex.line_start_modes[l] == syntax::lex_mode::normal;
			});
			if (fresh && plain) {
				if (const std::optional<std::vector<parameter_range>> parameters = parameter_ranges(tokens, units, *open, *close)) {
					std::vector<std::string> replacement = render_parenthesised(lines, units, *parameters, *open, *close, wrap, opts);
					const std::size_t span = last - first + 1;
					const auto same = [](const std::string& a, const std::string& b) {
						return trimmed(a) == trimmed(b);
					};
					if (!std::ranges::equal(replacement, lines.subspan(first, span), same)) {
						edits.push_back({
							.first_line = first,
							.last_line = last,
							.replacement = std::move(replacement),
						});
					}
				}
			}
		}

		if (terminator == '}') {
			if (!scopes.empty()) {
				scopes.pop_back();
			}
			u = v + 1;
			continue;
		}
		if (terminator == ';') {
			u = v + 1;
			continue;
		}

		scope_kind kind = scope_kind::other;
		if (!open && !is_enum) {
			if (is_namespace || (only_linkage && saw_word)) {
				kind = scope_kind::space;
			}
			else if (is_record) {
				kind = scope_kind::record;
			}
		}
		if (kind == scope_kind::other) {
			u = skip_body(v);
			continue;
		}
		scopes.push_back(kind);
		u = v + 1;
	}

	std::ranges::reverse(edits);
	return edits;
}

auto gse::format::apply(std::vector<std::string>& lines, const std::span<const line_edit> edits) -> std::size_t {
	std::size_t applied = 0;
	for (const line_edit& e : edits) {
		if (e.line >= lines.size()) {
			continue;
		}
		std::string& s = lines[e.line];
		if (!s.starts_with(e.expected)) {
			continue;
		}
		s.replace(0, e.expected.size(), e.replacement);
		++applied;
	}
	return applied;
}

auto gse::format::compute_calls(const std::span<const std::string> lines, const options& opts) -> std::vector<block_edit> {
	std::vector<std::string_view> views;
	views.reserve(lines.size());
	for (const std::string& l : lines) {
		views.emplace_back(l);
	}
	const syntax::lex_result lex = syntax::tokenize(views);
	const std::span<const syntax::token> tokens = lex.tokens;
	const std::vector<code_unit> units = flatten_units(tokens);

	struct open_bracket {
		char c = '(';
		std::size_t index = 0;
		bool code = false;
		std::uint32_t body_line = 0;
	};

	std::vector<std::size_t> close_of(units.size(), units.size());
	std::vector<bool> in_body(units.size(), false);
	std::vector<std::uint32_t> body_line(units.size(), 0);
	std::vector<bool> template_close(units.size(), false);
	std::vector<open_bracket> nest;
	bool scope_word = false;
	bool only_linkage = true;
	bool saw_word = false;
	bool saw_paren = false;

	const auto reset = [&] {
		scope_word = false;
		only_linkage = true;
		saw_word = false;
		saw_paren = false;
	};

	for (std::size_t i = 0; i < units.size(); ++i) {
		const code_unit& unit = units[i];
		const bool code = !nest.empty() && nest.back().code;
		const bool statement = nest.empty() || nest.back().c == '{';
		const std::uint32_t enclosing = nest.empty() ? 0 : nest.back().body_line;
		if (unit.c == '\0') {
			const syntax::token& t = tokens[unit.token];
			if (statement && !code && !is_comment(t.type) && t.type != syntax::token_type::preprocessor) {
				const std::string_view word = word_of(tokens, unit);
				saw_word = saw_word || !word.empty();
				scope_word = scope_word || word == "namespace" || word == "struct" || word == "class" || word == "union" || word == "enum";
				only_linkage = only_linkage && (word == "export" || word == "extern" || t.type == syntax::token_type::string);
			}
			continue;
		}
		if (unit.c == '(' || unit.c == '[') {
			in_body[i] = code;
			body_line[i] = enclosing;
			saw_paren = saw_paren || statement;
			nest.push_back({
				.c = unit.c,
				.index = i,
				.code = code,
				.body_line = enclosing,
			});
		}
		else if (unit.c == '{') {
			const bool declares_scope = !saw_paren && (scope_word || (only_linkage && saw_word));
			nest.push_back({
				.c = '{',
				.index = i,
				.code = code || !declares_scope,
				.body_line = unit.start.line,
			});
			reset();
		}
		else if (opens_angle(tokens, units, i)) {
			nest.push_back({
				.c = '<',
				.index = i,
				.code = code,
				.body_line = enclosing,
			});
		}
		else if (closes_angle(units, i) && !nest.empty() && nest.back().c == '<') {
			nest.pop_back();
			template_close[i] = true;
		}
		else if (is_closer(unit.c)) {
			while (!nest.empty() && nest.back().c == '<') {
				nest.pop_back();
			}
			if (nest.empty() || nest.back().c != matching_opener(unit.c)) {
				return {};
			}
			if (unit.c == ')') {
				close_of[nest.back().index] = i;
			}
			nest.pop_back();
			if (unit.c == '}') {
				reset();
			}
		}
		else if (unit.c == ';' && statement) {
			reset();
		}
	}

	static constexpr std::array<std::string_view, 7> statement_words{
		"constexpr", "consteval", "and", "or", "not", "case", "else",
	};

	const auto opens_call = [&](const std::size_t i) {
		if (units[i].c != '(' || !in_body[i] || close_of[i] == units.size() || i == 0) {
			return false;
		}
		if (template_close[i - 1]) {
			return true;
		}
		const std::string_view name = word_of(tokens, units[i - 1]);
		return !name.empty() && !excluded_name(name) && !std::ranges::contains(statement_words, name);
	};

	const int width = std::max(1, opts.indent_width);
	const auto needs_wrap = [&](const std::size_t open, const std::size_t close, const std::span<const parameter_range> arguments) {
		const std::uint32_t first = units[open].start.line;
		if (first == units[close].start.line) {
			const std::string_view rest = std::string_view(lines[first]).substr(units[close].end.column);
			const bool ends_line = rest.find_first_not_of(" \t\r;,){") == std::string_view::npos;
			return ends_line && body_line[open] != first && visual_width(lines[first], width) > opts.column_limit;
		}
		std::uint32_t previous = first;
		bool spans = false;
		bool lambda = false;
		bool broken = false;
		for (const parameter_range& argument : arguments) {
			const code_unit& head = units[argument.first];
			const code_unit& tail = units[argument.last];
			const bool multiline = head.start.line != tail.end.line;
			broken = broken || head.start.line != previous;
			spans = spans || multiline;
			lambda = lambda || (multiline && head.c == '[');
			previous = tail.end.line;
		}
		broken = broken || units[close].start.line != previous;
		return broken || lambda || (arguments.size() >= 2 && spans);
	};

	std::vector<block_edit> edits;
	std::size_t i = 0;
	while (i < units.size()) {
		if (!opens_call(i)) {
			++i;
			continue;
		}
		const std::size_t close = close_of[i];
		const std::uint32_t first = units[i].start.line;
		const std::uint32_t last = units[close].start.line;
		const bool plain = std::ranges::all_of(std::views::iota(first, last + 1), [&](const std::uint32_t l) {
			return lex.line_start_modes[l] == syntax::lex_mode::normal;
		});
		const std::optional<std::vector<parameter_range>> arguments = plain ? argument_ranges(tokens, units, i, close) : std::nullopt;
		if (!arguments || !needs_wrap(i, close, *arguments)) {
			++i;
			continue;
		}
		std::vector<std::string> replacement = render_parenthesised(lines, units, *arguments, i, close, true, opts);
		const std::size_t span = last - first + 1;
		const auto same = [](const std::string& a, const std::string& b) {
			return trimmed(a) == trimmed(b);
		};
		if (std::ranges::equal(replacement, lines.subspan(first, span), same)) {
			++i;
			continue;
		}
		if (edits.empty() || first > edits.back().last_line) {
			edits.push_back({
				.first_line = first,
				.last_line = last,
				.replacement = std::move(replacement),
			});
		}
		i = close + 1;
	}

	std::ranges::reverse(edits);
	return edits;
}

auto gse::format::compute_templates(const std::span<const std::string> lines, const options&) -> std::vector<block_edit> {
	std::vector<std::string_view> views;
	views.reserve(lines.size());
	for (const std::string& l : lines) {
		views.emplace_back(l);
	}
	const syntax::lex_result lex = syntax::tokenize(views);
	const std::span<const syntax::token> tokens = lex.tokens;
	const std::vector<code_unit> units = flatten_units(tokens);

	std::vector<line_break> breaks;
	std::vector<std::size_t> nest;
	std::vector<std::size_t> open_of(units.size(), units.size());

	const auto declaration_scope = [&] {
		return nest.empty() || units[nest.back()].c == '{';
	};

	const auto plain = [&](const std::uint32_t line) {
		return lex.line_start_modes[line] == syntax::lex_mode::normal;
	};

	const auto closing_angle = [&](const std::size_t open) -> std::optional<std::size_t> {
		int depth = 0;
		for (std::size_t j = open; j < units.size(); ++j) {
			const char c = units[j].c;
			if (c == ';' || c == '{' || c == '}') {
				return std::nullopt;
			}
			if (opens_angle(tokens, units, j) || c == '(' || c == '[') {
				++depth;
			}
			else if ((closes_angle(units, j) || c == ')' || c == ']') && --depth == 0) {
				return c == '>' ? std::optional(j) : std::nullopt;
			}
		}
		return std::nullopt;
	};

	for (std::size_t i = 0; i < units.size(); ++i) {
		const code_unit& unit = units[i];
		if (unit.c == '\0') {
			const std::string_view word = word_of(tokens, unit);
			if (word == "template" && declaration_scope() && i + 1 < units.size() && opens_angle(tokens, units, i + 1)) {
				const char before = i > 0 ? units[i - 1].c : ';';
				const bool joined = i > 1 && units[i - 1].token == units[i - 2].token;
				const bool member = before == '.' || ((before == ':' || before == '>') && joined);
				const std::optional<std::size_t> close = member ? std::nullopt : closing_angle(i + 1);
				if (close) {
					const code_unit& angle = units[i + 1];
					const std::string_view gap = std::string_view(lines[unit.end.line]).substr(unit.end.column, angle.start.column - unit.end.column);
					if (angle.start.line == unit.end.line && gap != " " && plain(unit.end.line)) {
						breaks.push_back({
							.line = unit.end.line,
							.from = unit.end.column,
							.to = angle.start.column,
						});
					}
					const code_unit& end = units[*close];
					if (*close + 1 < units.size() && plain(end.end.line)) {
						const code_unit& next = units[*close + 1];
						const bool comment = next.c == '\0' && is_comment(tokens[next.token].type);
						if (next.start.line == end.end.line && !comment) {
							breaks.push_back({
								.line = end.end.line,
								.from = end.end.column,
								.to = next.start.column,
								.split = true,
							});
						}
					}
				}
			}
			else if (word == "requires" && declaration_scope() && i > 0) {
				std::size_t p = i - 1;
				if (const std::string_view qualifier = word_of(tokens, units[p]); (qualifier == "const" || qualifier == "noexcept") && p > 0) {
					--p;
				}
				const std::size_t open = units[p].c == ')' ? open_of[p] : units.size();
				const bool lambda = open == units.size() || (open > 0 && units[open - 1].c == ']');
				if (!lambda && units[i - 1].end.line == unit.start.line && plain(unit.start.line)) {
					breaks.push_back({
						.line = unit.start.line,
						.from = units[i - 1].end.column,
						.to = unit.start.column,
						.split = true,
					});
				}
			}
			continue;
		}
		if (unit.c == '(' || unit.c == '[' || unit.c == '{' || opens_angle(tokens, units, i)) {
			nest.push_back(i);
		}
		else if (closes_angle(units, i) && !nest.empty() && units[nest.back()].c == '<') {
			nest.pop_back();
		}
		else if (is_closer(unit.c)) {
			while (!nest.empty() && units[nest.back()].c == '<') {
				nest.pop_back();
			}
			if (nest.empty() || units[nest.back()].c != matching_opener(unit.c)) {
				return {};
			}
			open_of[i] = nest.back();
			nest.pop_back();
		}
	}

	return render_breaks(lines, breaks);
}

auto gse::format::compute_case_bodies(const std::span<const std::string> lines, const options&) -> std::vector<block_edit> {
	std::vector<std::string_view> views;
	views.reserve(lines.size());
	for (const std::string& l : lines) {
		views.emplace_back(l);
	}
	const syntax::lex_result lex = syntax::tokenize(views);
	const std::span<const syntax::token> tokens = lex.tokens;
	const std::vector<code_unit> units = flatten_units(tokens);

	std::vector<line_break> breaks;
	std::vector<bool> switch_scopes;
	bool switch_pending = false;

	const auto joined_colon = [&](const std::size_t j) {
		return (j > 0 && units[j - 1].c == ':' && units[j - 1].token == units[j].token) || (j + 1 < units.size() && units[j + 1].c == ':' && units[j + 1].token == units[j].token);
	};

	const auto label_end = [&](const std::size_t from) -> std::optional<std::size_t> {
		int depth = 0;
		int ternary = 0;
		for (std::size_t j = from; j < units.size(); ++j) {
			const char c = units[j].c;
			if (c == ';' || c == '{' || c == '}') {
				return std::nullopt;
			}
			if (c == '(' || c == '[') {
				++depth;
			}
			else if (c == ')' || c == ']') {
				--depth;
			}
			else if (c == '?') {
				++ternary;
			}
			else if (c == ':' && depth == 0 && !joined_colon(j)) {
				if (ternary == 0) {
					return j;
				}
				--ternary;
			}
		}
		return std::nullopt;
	};

	for (std::size_t i = 0; i < units.size(); ++i) {
		const code_unit& unit = units[i];
		if (unit.c == '\0') {
			const std::string_view word = word_of(tokens, unit);
			if (word == "switch") {
				switch_pending = true;
			}
			else if (is_label_keyword(word) && !switch_scopes.empty() && switch_scopes.back()) {
				const std::optional<std::size_t> colon = label_end(i + 1);
				if (!colon || *colon + 1 >= units.size()) {
					continue;
				}
				const code_unit& end = units[*colon];
				const code_unit& next = units[*colon + 1];
				const bool comment = next.c == '\0' && is_comment(tokens[next.token].type);
				if (next.start.line == end.end.line && next.c != '{' && !comment && lex.line_start_modes[end.end.line] == syntax::lex_mode::normal) {
					breaks.push_back({
						.line = end.end.line,
						.from = end.end.column,
						.to = next.start.column,
						.split = true,
					});
				}
				i = *colon;
			}
			continue;
		}
		if (unit.c == '{') {
			switch_scopes.push_back(switch_pending && i > 0 && units[i - 1].c == ')');
			switch_pending = false;
		}
		else if (unit.c == '}') {
			if (switch_scopes.empty()) {
				return {};
			}
			switch_scopes.pop_back();
		}
		else if (unit.c == ';') {
			switch_pending = false;
		}
	}

	return render_breaks(lines, breaks);
}

auto gse::format::compute_else_breaks(const std::span<const std::string> lines, const options&) -> std::vector<block_edit> {
	std::vector<std::string_view> views;
	views.reserve(lines.size());
	for (const std::string& l : lines) {
		views.emplace_back(l);
	}
	const syntax::lex_result lex = syntax::tokenize(views);
	const std::span<const syntax::token> tokens = lex.tokens;
	const std::vector<code_unit> units = flatten_units(tokens);

	std::vector<line_break> breaks;
	for (std::size_t i = 0; i + 1 < units.size(); ++i) {
		const code_unit& close = units[i];
		const code_unit& next = units[i + 1];
		if (close.c == '}' && next.c == '\0' && word_of(tokens, next) == "else" && next.start.line == close.end.line && lex.line_start_modes[close.end.line] == syntax::lex_mode::normal) {
			breaks.push_back({
				.line = close.end.line,
				.from = close.end.column,
				.to = next.start.column,
				.split = true,
			});
		}
	}

	return render_breaks(lines, breaks);
}

auto gse::format::render_breaks(const std::span<const std::string> lines, std::vector<line_break>& breaks) -> std::vector<block_edit> {
	std::ranges::sort(breaks, {}, [](const line_break& b) {
		return std::pair(b.line, b.from);
	});

	std::vector<block_edit> edits;
	for (std::size_t b = 0; b < breaks.size();) {
		const std::uint32_t line = breaks[b].line;
		const std::string_view text(lines[line]);
		const std::string indent(leading_whitespace(text));
		std::vector<std::string> out;
		std::string current;
		std::size_t position = 0;
		for (; b < breaks.size() && breaks[b].line == line; ++b) {
			current += text.substr(position, breaks[b].from - position);
			if (breaks[b].split) {
				current.erase(current.find_last_not_of(" \t") + 1);
				out.push_back(std::move(current));
				current = indent;
			}
			else {
				current += ' ';
			}
			position = breaks[b].to;
		}
		current += text.substr(position);
		out.push_back(std::move(current));
		edits.push_back({
			.first_line = line,
			.last_line = line,
			.replacement = std::move(out),
		});
	}

	std::ranges::reverse(edits);
	return edits;
}

auto gse::format::compute_enum_bodies(const std::span<const std::string> lines, const options& opts) -> std::vector<block_edit> {
	std::vector<std::string_view> views;
	views.reserve(lines.size());
	for (const std::string& l : lines) {
		views.emplace_back(l);
	}
	const syntax::lex_result lex = syntax::tokenize(views);
	const std::span<const syntax::token> tokens = lex.tokens;
	const std::vector<code_unit> units = flatten_units(tokens);

	std::vector<block_edit> edits;
	for (std::size_t i = 0; i < units.size(); ++i) {
		if (word_of(tokens, units[i]) != "enum" || (i > 0 && word_of(tokens, units[i - 1]) == "using")) {
			continue;
		}
		std::size_t open = i + 1;
		while (open < units.size() && units[open].c != '{' && units[open].c != ';' && units[open].c != '(') {
			++open;
		}
		if (open >= units.size() || units[open].c != '{') {
			continue;
		}

		std::vector<std::pair<std::size_t, std::size_t>> segments;
		std::vector<char> nest;
		std::optional<std::size_t> close;
		std::size_t first = open + 1;
		bool clean = true;
		for (std::size_t j = open + 1; j < units.size() && clean; ++j) {
			const code_unit& unit = units[j];
			if (unit.c == '\0') {
				const syntax::token_type type = tokens[unit.token].type;
				clean = !is_comment(type) && type != syntax::token_type::preprocessor;
				continue;
			}
			if (nest.empty() && unit.c == '}') {
				close = j;
				break;
			}
			if (nest.empty() && unit.c == ',') {
				segments.emplace_back(first, j);
				first = j + 1;
			}
			else if (unit.c == '(' || unit.c == '[' || unit.c == '{') {
				nest.push_back(unit.c);
			}
			else if (opens_angle(tokens, units, j)) {
				nest.push_back('<');
			}
			else if (closes_angle(units, j) && !nest.empty() && nest.back() == '<') {
				nest.pop_back();
			}
			else if (is_closer(unit.c)) {
				while (!nest.empty() && nest.back() == '<') {
					nest.pop_back();
				}
				clean = !nest.empty() && nest.back() == matching_opener(unit.c);
				if (clean) {
					nest.pop_back();
				}
			}
		}
		if (!close || !clean) {
			continue;
		}
		i = *close;
		const bool trailing = first == *close;
		if (!trailing) {
			segments.emplace_back(first, *close);
		}
		const code_unit& brace = units[open];
		const code_unit& end = units[*close];
		const bool plain = std::ranges::all_of(std::views::iota(brace.start.line, end.start.line + 1), [&](const std::uint32_t l) {
			return lex.line_start_modes[l] == syntax::lex_mode::normal;
		});
		if (segments.empty() || !plain) {
			continue;
		}

		if (brace.start.line == end.start.line) {
			const std::uint32_t line = brace.start.line;
			const std::string_view text(lines[line]);
			const std::string indent(leading_whitespace(text));
			const std::string inner = indent + make_indent(1, opts.indent_width, opts);
			std::vector<std::string> out;
			out.emplace_back(text.substr(0, brace.end.column));
			for (const auto& [from, to] : segments) {
				const std::uint32_t start = units[from].start.column;
				out.push_back(inner + std::string(text.substr(start, units[to - 1].end.column - start)) + ",");
			}
			out.push_back(indent + std::string(text.substr(end.start.column)));
			edits.push_back({
				.first_line = line,
				.last_line = line,
				.replacement = std::move(out),
			});
		}
		else if (!trailing) {
			const code_unit& last = units[segments.back().second - 1];
			std::string text = lines[last.end.line];
			text.insert(last.end.column, ",");
			edits.push_back({
				.first_line = last.end.line,
				.last_line = last.end.line,
				.replacement = { std::move(text) },
			});
		}
	}

	std::ranges::reverse(edits);
	return edits;
}

auto gse::format::classify_brace(const std::span<const syntax::token> tokens, const std::span<const code_unit> units, const std::size_t first, const std::size_t open) -> brace_kind {
	std::vector<std::size_t> top;
	std::vector<char> nest;
	for (std::size_t j = first; j < open; ++j) {
		const code_unit& unit = units[j];
		if (unit.c == '\0') {
			const syntax::token_type type = tokens[unit.token].type;
			if (type == syntax::token_type::preprocessor) {
				return brace_kind::other;
			}
			if (is_comment(type)) {
				continue;
			}
		}
		const bool closer = is_closer(unit.c) || (closes_angle(units, j) && !nest.empty() && nest.back() == '<');
		if (nest.empty() && !closer) {
			top.push_back(j);
		}
		if (unit.c == '(' || unit.c == '[' || unit.c == '{') {
			nest.push_back(unit.c);
		}
		else if (opens_angle(tokens, units, j)) {
			nest.push_back('<');
		}
		else if (closes_angle(units, j) && !nest.empty() && nest.back() == '<') {
			nest.pop_back();
		}
		else if (is_closer(unit.c)) {
			while (!nest.empty() && nest.back() == '<') {
				nest.pop_back();
			}
			if (nest.empty() || nest.back() != matching_opener(unit.c)) {
				return brace_kind::other;
			}
			nest.pop_back();
		}
		if (closer && nest.empty()) {
			top.push_back(j);
		}
	}
	if (top.empty() || !nest.empty()) {
		return brace_kind::other;
	}

	const auto single_colon = [&](const std::size_t j) {
		const bool before = j > 0 && units[j - 1].c == ':' && units[j - 1].token == units[j].token;
		const bool after = j + 1 < units.size() && units[j + 1].c == ':' && units[j + 1].token == units[j].token;
		return units[j].c == ':' && !before && !after;
	};

	std::optional<std::size_t> record;
	std::optional<std::size_t> first_close;
	std::optional<std::size_t> last_open;
	bool initializers = false;
	for (std::size_t t = 0; t < top.size(); ++t) {
		const code_unit& unit = units[top[t]];
		const std::string_view word = word_of(tokens, unit);
		if (word == "enum" || word == "using" || word == "typedef" || word == "return" || word == "concept") {
			return brace_kind::other;
		}
		if (word == "namespace") {
			return brace_kind::scope;
		}
		if (!record && (word == "struct" || word == "class" || word == "union")) {
			record = t;
		}
		if (unit.c == '=' && tokens[unit.token].text == "=" && !(t > 0 && word_of(tokens, units[top[t - 1]]) == "operator")) {
			return brace_kind::other;
		}
		if (unit.c == '(') {
			last_open = t;
		}
		if (unit.c == ')' && !first_close) {
			first_close = t;
		}
		if (first_close && single_colon(top[t])) {
			initializers = true;
		}
	}

	const code_unit& last = units[top.back()];
	if (last.c == ',' || last.c == '(' || last.c == '=') {
		return brace_kind::other;
	}

	if (record) {
		bool previous_word = false;
		for (std::size_t t = *record + 1; t < top.size(); ++t) {
			const code_unit& unit = units[top[t]];
			const std::string_view word = word_of(tokens, unit);
			if (single_colon(top[t])) {
				break;
			}
			if (!word.empty()) {
				if (previous_word && word != "final") {
					return brace_kind::other;
				}
				previous_word = true;
				continue;
			}
			if (unit.c != ':' && unit.c != '<' && unit.c != '>' && !(unit.c == '[' || unit.c == ']')) {
				return brace_kind::other;
			}
			previous_word = false;
		}
		return brace_kind::scope;
	}

	if (!last_open) {
		return brace_kind::other;
	}
	if (*last_open > 1 && word_of(tokens, units[top[*last_open - 1]]) == "requires" && word_of(tokens, units[top[*last_open - 2]]) == "requires") {
		return brace_kind::other;
	}
	if (initializers && last.c != ')' && last.c != '}') {
		return brace_kind::other;
	}
	return brace_kind::function;
}

auto gse::format::compute_body_braces(const std::span<const std::string> lines, const options&) -> std::vector<block_edit> {
	std::vector<std::string_view> views;
	views.reserve(lines.size());
	for (const std::string& l : lines) {
		views.emplace_back(l);
	}
	const syntax::lex_result lex = syntax::tokenize(views);
	const std::span<const syntax::token> tokens = lex.tokens;
	const std::vector<code_unit> units = flatten_units(tokens);

	struct frame {
		std::size_t open = 0;
		brace_kind kind = brace_kind::other;
	};

	std::vector<frame> stack;
	std::vector<std::pair<std::size_t, std::size_t>> bodies;
	std::size_t head = 0;
	for (std::size_t i = 0; i < units.size(); ++i) {
		const code_unit& unit = units[i];
		const bool declaration = stack.empty() || stack.back().kind == brace_kind::scope;
		if (unit.c == '\0') {
			if (declaration && tokens[unit.token].type == syntax::token_type::preprocessor) {
				head = i + 1;
			}
			continue;
		}
		switch (unit.c) {
			case '(':
			case '[':
				stack.push_back({ .open = i });
				break;
			case ')':
			case ']':
				if (!stack.empty()) {
					stack.pop_back();
				}
				break;
			case '{': {
				const brace_kind kind = declaration ? classify_brace(tokens, units, head, i) : brace_kind::other;
				stack.push_back({ .open = i, .kind = kind });
				if (kind == brace_kind::scope) {
					head = i + 1;
				}
				break;
			}
			case '}': {
				if (stack.empty()) {
					break;
				}
				const frame f = stack.back();
				stack.pop_back();
				if (f.kind != brace_kind::other) {
					bodies.emplace_back(f.open, i);
					head = i + 1;
				}
				break;
			}
			case ';':
				if (declaration) {
					head = i + 1;
				}
				break;
			case ':':
				if (const std::string_view word = i > 0 ? word_of(tokens, units[i - 1]) : std::string_view(); declaration && (word == "public" || word == "private" || word == "protected")) {
					head = i + 1;
				}
				break;
			default:
				break;
		}
	}
	std::ranges::sort(bodies);

	std::vector<bool> claimed(lines.size(), false);
	const auto available = [&](const std::uint32_t from, const std::uint32_t to) {
		return std::ranges::all_of(std::views::iota(from, to + 1), [&](const std::uint32_t l) {
			return !claimed[l] && lex.line_start_modes[l] == syntax::lex_mode::normal;
		});
	};
	const auto claim = [&](const std::uint32_t from, const std::uint32_t to) {
		for (std::uint32_t l = from; l <= to; ++l) {
			claimed[l] = true;
		}
	};
	const auto plain = [&](const std::size_t from, const std::size_t to) {
		return std::ranges::none_of(std::views::iota(from, to + 1), [&](const std::size_t j) {
			const syntax::token_type type = tokens[units[j].token].type;
			return units[j].c == '\0' && (is_comment(type) || type == syntax::token_type::preprocessor);
		});
	};

	std::vector<block_edit> edits;
	std::vector<line_break> breaks;
	for (const auto& [o, c] : bodies) {
		const code_unit& open = units[o];
		const code_unit& close = units[c];
		const std::string_view open_text(lines[open.start.line]);
		if (o > 0 && open.start.column == leading_whitespace(open_text).size()) {
			const code_unit& previous = units[o - 1];
			if (previous.end.line < open.start.line && plain(o - 1, o) && available(previous.end.line, open.start.line)) {
				claim(previous.end.line, open.start.line);
				edits.push_back({
					.first_line = previous.end.line,
					.last_line = open.start.line,
					.replacement = { lines[previous.end.line].substr(0, previous.end.column) + " " + std::string(open_text.substr(open.start.column)) },
				});
			}
			continue;
		}
		if (c == o + 1) {
			if ((open.end.line != close.start.line || open.end.column != close.start.column) && available(open.start.line, close.start.line)) {
				claim(open.start.line, close.start.line);
				edits.push_back({
					.first_line = open.start.line,
					.last_line = close.start.line,
					.replacement = { std::string(open_text.substr(0, open.end.column)) + lines[close.start.line].substr(close.start.column) },
				});
			}
			continue;
		}
		if (open.start.line == close.start.line && plain(o, c) && available(open.start.line, open.start.line)) {
			claim(open.start.line, open.start.line);
			breaks.push_back({
				.line = open.start.line,
				.from = open.end.column,
				.to = units[o + 1].start.column,
				.split = true,
			});
			breaks.push_back({
				.line = open.start.line,
				.from = units[c - 1].end.column,
				.to = close.start.column,
				.split = true,
			});
		}
	}

	std::ranges::move(render_breaks(lines, breaks), std::back_inserter(edits));
	std::ranges::sort(edits, std::ranges::greater{}, &block_edit::first_line);
	return edits;
}

auto gse::format::compute_blank_lines(const std::span<const std::string> lines, const options&) -> std::vector<block_edit> {
	std::vector<std::string_view> views;
	views.reserve(lines.size());
	for (const std::string& l : lines) {
		views.emplace_back(l);
	}
	const syntax::lex_result lex = syntax::tokenize(views);
	const std::span<const syntax::token> tokens = lex.tokens;

	constexpr std::size_t none = std::numeric_limits<std::size_t>::max();
	std::vector<std::size_t> first(lines.size(), none);
	std::vector<std::size_t> last(lines.size(), none);
	for (std::size_t i = 0; i < tokens.size(); ++i) {
		if (first[tokens[i].line] == none) {
			first[tokens[i].line] = i;
		}
		last[tokens[i].line] = i;
	}

	const auto is_punct = [&](const std::size_t index) {
		return index != none && tokens[index].type == syntax::token_type::punctuation;
	};
	const auto is_access_label = [&](const std::size_t line) {
		const std::size_t f = first[line];
		if (f == none || last[line] != f + 1 || !is_punct(f + 1) || tokens[f + 1].text != ":") {
			return false;
		}
		const std::string_view word = tokens[f].text;
		return word == "public" || word == "private" || word == "protected";
	};
	const auto is_blank = [&](const std::size_t line) {
		return trimmed(lines[line]).empty() && lex.line_start_modes[line] == syntax::lex_mode::normal;
	};

	std::vector<block_edit> edits;
	for (std::size_t i = 1; i < lines.size();) {
		if (!is_blank(i)) {
			++i;
			continue;
		}
		std::size_t end = i;
		while (end + 1 < lines.size() && is_blank(end + 1)) {
			++end;
		}
		const std::size_t previous = i - 1;
		const std::size_t next = end + 1;
		if (next < lines.size() && trimmed(lines[previous]).size() > 0) {
			const bool after_open = is_punct(last[previous]) && tokens[last[previous]].text.back() == '{';
			const bool before_close = is_punct(first[next]) && tokens[first[next]].text.front() == '}';
			const bool remove = after_open || before_close || is_access_label(previous);
			if (remove) {
				edits.push_back({
					.first_line = static_cast<std::uint32_t>(i),
					.last_line = static_cast<std::uint32_t>(end),
				});
			}
			else if (end > i) {
				edits.push_back({
					.first_line = static_cast<std::uint32_t>(i),
					.last_line = static_cast<std::uint32_t>(end),
					.replacement = { std::string() },
				});
			}
		}
		i = end + 1;
	}

	std::ranges::reverse(edits);
	return edits;
}

auto gse::format::format_lines(std::vector<std::string>& lines, const options& opts) -> format_result {
	format_result result;
	if (std::optional<block_edit> imports = compute_imports(lines); imports && apply_block(lines, *imports)) {
		result.blocks.push_back(std::move(*imports));
	}
	for (block_edit& block : compute_annotations(lines, opts)) {
		if (apply_block(lines, block)) {
			result.blocks.push_back(std::move(block));
		}
	}
	for (block_edit& block : compute_enum_bodies(lines, opts)) {
		if (apply_block(lines, block)) {
			result.blocks.push_back(std::move(block));
		}
	}
	for (block_edit& block : compute_initializers(lines, opts)) {
		if (apply_block(lines, block)) {
			result.blocks.push_back(std::move(block));
		}
	}
	for (block_edit& block : compute_signatures(lines, opts)) {
		if (apply_block(lines, block)) {
			result.blocks.push_back(std::move(block));
		}
	}
	for (bool changed = true; changed;) {
		changed = false;
		for (block_edit& block : compute_calls(lines, opts)) {
			if (apply_block(lines, block)) {
				result.blocks.push_back(std::move(block));
				changed = true;
			}
		}
	}
	for (block_edit& block : compute_initializers(lines, opts)) {
		if (apply_block(lines, block)) {
			result.blocks.push_back(std::move(block));
		}
	}
	for (block_edit& block : compute_templates(lines, opts)) {
		if (apply_block(lines, block)) {
			result.blocks.push_back(std::move(block));
		}
	}
	for (block_edit& block : compute_case_bodies(lines, opts)) {
		if (apply_block(lines, block)) {
			result.blocks.push_back(std::move(block));
		}
	}
	for (bool changed = true; changed;) {
		changed = false;
		for (block_edit& block : compute_body_braces(lines, opts)) {
			if (apply_block(lines, block)) {
				result.blocks.push_back(std::move(block));
				changed = true;
			}
		}
	}
	for (block_edit& block : compute_else_breaks(lines, opts)) {
		if (apply_block(lines, block)) {
			result.blocks.push_back(std::move(block));
		}
	}
	for (block_edit& block : compute_blank_lines(lines, opts)) {
		if (apply_block(lines, block)) {
			result.blocks.push_back(std::move(block));
		}
	}
	result.edits = compute(lines, opts);
	result.reindented = apply(lines, result.edits);
	return result;
}
