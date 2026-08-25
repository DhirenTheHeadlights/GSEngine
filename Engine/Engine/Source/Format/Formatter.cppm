export module gse.format:formatter;

import std;
import gse.syntax;

export namespace gse::format {
	struct options {
		int indent_width = 4;
		bool indent_with_spaces = false;
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
		const bool operator_start = first.type == syntax::token_type::punctuation && !punct_starts_statement(first.text, trigger);
		const bool chain_start = first.type == syntax::token_type::punctuation
			&& (first.text.front() == '.' || (first.text.size() > 1 && first.text[0] == '-' && first.text[1] == '>'));
		const bool eligible = normal_start && !state.continuation && state.angle_depth == 0
			&& starts_statement(trigger) && !is_comment(first.type) && !operator_start;

		int effective = indent_of(ws, width);
		if (eligible) {
			int level = 0;
			if (const int closers = leading_closer_count(line_tokens); closers > 0) {
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
				}
				else if (c == '>' && state.angle_depth > 0 && !(ci > 0 && t.text[ci - 1] == '-')) {
					--state.angle_depth;
				}
				else if (c == ';' || c == '{' || c == '}') {
					state.angle_depth = 0;
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
