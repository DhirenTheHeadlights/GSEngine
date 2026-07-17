export module gse.ide.format:formatter;

import std;
import gse.ide.highlight;

export namespace gse::ide::format {
	struct options {
		int indent_width = 4;
		bool indent_with_spaces = false;
	};

	struct line_edit {
		std::uint32_t line = 0;
		std::string expected;
		std::string replacement;
	};

	auto compute(std::span<const std::string> lines, const options& opts) -> std::vector<line_edit>;

	auto apply(std::vector<std::string>& lines, std::span<const line_edit> edits) -> std::size_t;
}

namespace gse::ide::format {
	struct bracket {
		char kind = '{';
		int open_indent = 0;
		int content_indent = 0;
		bool switch_body = false;
		std::uint32_t open_line = 0;
	};

	auto is_opener(char c) -> bool;
	auto is_closer(char c) -> bool;
	auto starts_statement(char c) -> bool;
	auto ends_statement(char c) -> bool;
	auto clears_continuation(char c) -> bool;
	auto punct_starts_statement(std::string_view text, char last_code) -> bool;
	auto is_conditional_directive(std::string_view text) -> bool;
	auto is_comment(syntax::token_type type) -> bool;
	auto is_label_keyword(std::string_view w) -> bool;
	auto leading_whitespace(std::string_view s) -> std::string_view;
	auto indent_of(std::string_view ws, int width) -> int;
	auto make_indent(int level, int width, const options& opts) -> std::string;
	auto leading_closer_count(std::span<const syntax::token> line_tokens) -> int;
	auto is_access_specifier(std::span<const syntax::token> line_tokens) -> bool;
}

auto gse::ide::format::is_opener(const char c) -> bool {
	return c == '(' || c == '[' || c == '{';
}

auto gse::ide::format::is_closer(const char c) -> bool {
	return c == ')' || c == ']' || c == '}';
}

auto gse::ide::format::starts_statement(const char c) -> bool {
	return c == ';' || c == '{' || c == '}' || c == ',' || c == ':' || c == '(' || c == '[' || c == ']' || c == '>';
}

auto gse::ide::format::ends_statement(const char c) -> bool {
	return c == ';' || c == '{' || c == '}' || c == '>';
}

auto gse::ide::format::clears_continuation(const char c) -> bool {
	return ends_statement(c) || c == ',' || c == '(' || c == '[';
}

auto gse::ide::format::punct_starts_statement(const std::string_view text, const char last_code) -> bool {
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

auto gse::ide::format::is_conditional_directive(const std::string_view text) -> bool {
	std::size_t i = 1;
	while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) {
		++i;
	}
	const std::string_view directive = text.substr(i);
	return directive.starts_with("if") || directive.starts_with("el");
}

auto gse::ide::format::is_comment(const syntax::token_type type) -> bool {
	return type == syntax::token_type::line_comment || type == syntax::token_type::block_comment;
}

auto gse::ide::format::is_label_keyword(const std::string_view w) -> bool {
	return w == "case" || w == "default";
}

auto gse::ide::format::leading_whitespace(const std::string_view s) -> std::string_view {
	std::size_t i = 0;
	while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
		++i;
	}
	return s.substr(0, i);
}

auto gse::ide::format::indent_of(const std::string_view ws, const int width) -> int {
	int col = 0;
	for (const char c : ws) {
		col = c == '\t' ? (col / width + 1) * width : col + 1;
	}
	return col / width;
}

auto gse::ide::format::make_indent(const int level, const int width, const options& opts) -> std::string {
	if (opts.indent_with_spaces) {
		return std::string(static_cast<std::size_t>(level) * width, ' ');
	}
	return std::string(static_cast<std::size_t>(level), '\t');
}

auto gse::ide::format::leading_closer_count(const std::span<const syntax::token> line_tokens) -> int {
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

auto gse::ide::format::is_access_specifier(const std::span<const syntax::token> line_tokens) -> bool {
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

auto gse::ide::format::compute(const std::span<const std::string> lines, const options& opts) -> std::vector<line_edit> {
	std::vector<line_edit> edits;

	std::vector<std::string_view> views;
	views.reserve(lines.size());
	for (const std::string& l : lines) {
		views.emplace_back(l);
	}

	const syntax::lex_result lex = syntax::tokenize(views);
	const int width = std::max(1, opts.indent_width);

	std::vector<bracket> stack;
	int paren_depth = 0;
	bool switch_pending = false;
	int switch_paren_base = 0;
	char last_code = ';';
	bool continuation = false;
	bool guard_next = false;
	int statement_indent = 0;
	int angle_depth = 0;
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
			if (is_conditional_directive(first.text)) {
				guard_next = true;
			}
			else {
				last_code = ';';
				continuation = false;
			}
			continue;
		}

		const bool guarded = guard_next;
		guard_next = false;

		const char trigger = last_code;
		const bool operator_start = first.type == syntax::token_type::punctuation && !punct_starts_statement(first.text, trigger);
		const bool chain_start = first.type == syntax::token_type::punctuation
			&& (first.text.front() == '.' || (first.text.size() > 1 && first.text[0] == '-' && first.text[1] == '>'));
		const bool eligible = normal_start && !continuation && !guarded && angle_depth == 0
			&& starts_statement(trigger) && !is_comment(first.type) && !operator_start;

		int effective = indent_of(ws, width);
		if (eligible) {
			int level = 0;
			if (const int closers = leading_closer_count(line_tokens); closers > 0) {
				level = stack.empty() ? 0 : stack.back().open_indent;
			}
			else if (!stack.empty()) {
				const bracket& top = stack.back();
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
				statement_indent = level;
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
				last_code = t.text.back();
				if (t.type == syntax::token_type::identifier && t.text == "switch") {
					switch_pending = true;
					switch_paren_base = paren_depth;
				}
				prev_token = &t;
				continue;
			}
			const bool after_ident = prev_token && prev_token->type == syntax::token_type::identifier && prev_token->end == t.start;
			prev_token = &t;
			for (std::size_t ci = 0; ci < t.text.size(); ++ci) {
				const char c = t.text[ci];
				if (c == '<' && ci == 0 && after_ident) {
					++angle_depth;
				}
				else if (c == '>' && angle_depth > 0 && !(ci > 0 && t.text[ci - 1] == '-')) {
					--angle_depth;
				}
				else if (c == ';' || c == '{' || c == '}') {
					angle_depth = 0;
				}
				if (is_opener(c)) {
					const bool body = c == '{' && switch_pending && paren_depth == switch_paren_base;
					if (body) {
						switch_pending = false;
					}
					if (c == '(') {
						++paren_depth;
					}
					int anchor = effective;
					if (c == '{') {
						if (cross_line_pop >= 0) {
							anchor = cross_line_pop;
						}
						else if (!eligible && operator_start && !chain_start) {
							anchor = statement_indent;
						}
					}
					stack.push_back({
						.kind = c,
						.open_indent = anchor,
						.content_indent = anchor + 1,
						.switch_body = body,
						.open_line = line,
					});
				}
				else if (is_closer(c)) {
					if (!stack.empty()) {
						if (stack.back().kind == '(') {
							--paren_depth;
						}
						if (stack.back().open_line != line) {
							cross_line_pop = stack.back().open_indent;
						}
						stack.pop_back();
					}
				}
				else if (c == ';') {
					switch_pending = false;
				}
				last_code = c;
			}
		}

		continuation = !eligible && !clears_continuation(last_code);
	}

	return edits;
}

auto gse::ide::format::apply(std::vector<std::string>& lines, const std::span<const line_edit> edits) -> std::size_t {
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
