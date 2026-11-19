export module gse.syntax:lexer;

import std;

import :kinds;

export namespace gse::syntax {
	enum class token_type {
		line_comment [[= kind::comment]],
		block_comment [[= kind::comment]],
		string [[= kind::string]],
		number [[= kind::number]],
		identifier,
		preprocessor [[= kind::preprocessor]],
		punctuation [[= kind::punctuation]]
	};

	struct token {
		std::uint32_t line = 0;
		std::uint32_t start = 0;
		std::uint32_t end = 0;
		token_type type = token_type::identifier;
		std::string_view text;
	};

	enum class lex_mode {
		normal,
		block_comment,
		raw_string,
		quoted_string,
		line_comment
	};

	struct lex_result {
		std::vector<token> tokens;
		std::vector<lex_mode> line_start_modes;
	};

	auto is_ident_start(
		char c
	) -> bool;

	auto is_ident_char(
		char c
	) -> bool;

	auto is_digit(
		char c
	) -> bool;

	auto is_space(
		char c
	) -> bool;

	auto is_punct(
		char c
	) -> bool;

	auto split_lines(
		std::string_view source
	) -> std::vector<std::string_view>;

	auto tokenize(
		std::span<const std::string_view> lines
	) -> lex_result;
}

namespace gse::syntax {
	struct literal_prefix {
		std::string_view text;
		bool raw = false;
	};

	struct literal_start {
		std::size_t prefix_len = 0;
		bool raw = false;
	};

	struct quoted_scan_info {
		std::string_view line;
		std::size_t start = 0;
		std::size_t end = 0;
		char quote = '\0';
		bool escaped = false;
	};

	struct quoted_scan {
		std::optional<std::size_t> close;
		bool escaped = false;
	};

	constexpr auto is_ident_start_byte(
		unsigned char c
	) -> bool;

	constexpr auto is_ident_char_byte(
		unsigned char c
	) -> bool;

	constexpr auto splice_position(
		std::string_view line
	) -> std::optional<std::size_t>;

	auto scan_quoted(
		const quoted_scan_info& info
	) -> quoted_scan;

	auto match_literal_start(
		std::string_view line,
		std::size_t index
	) -> std::optional<literal_start>;
}

constexpr auto gse::syntax::is_ident_start_byte(const unsigned char c) -> bool {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c >= 0x80;
}

constexpr auto gse::syntax::is_ident_char_byte(const unsigned char c) -> bool {
	return is_ident_start_byte(c) || (c >= '0' && c <= '9');
}

constexpr auto gse::syntax::splice_position(const std::string_view line) -> std::optional<std::size_t> {
	std::size_t end = line.size();
	if (end > 0 && line[end - 1] == '\r') {
		--end;
	}
	if (end > 0 && line[end - 1] == '\\') {
		return end - 1;
	}
	return std::nullopt;
}

static_assert(gse::syntax::is_ident_start_byte(0xc3));
static_assert(gse::syntax::splice_position("value\\") == 5);
static_assert(gse::syntax::splice_position("value\\\r") == 5);
static_assert(!gse::syntax::splice_position("value\\ "));

auto gse::syntax::is_ident_start(const char c) -> bool {
	return is_ident_start_byte(static_cast<unsigned char>(c));
}

auto gse::syntax::is_ident_char(const char c) -> bool {
	return is_ident_char_byte(static_cast<unsigned char>(c));
}

auto gse::syntax::is_digit(const char c) -> bool {
	return c >= '0' && c <= '9';
}

auto gse::syntax::is_space(const char c) -> bool {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

auto gse::syntax::is_punct(const char c) -> bool {
	switch (c) {
		case '+':
		case '-':
		case '*':
		case '/':
		case '%':
		case '=':
		case '<':
		case '>':
		case '!':
		case '&':
		case '|':
		case '^':
		case '~':
		case '?':
		case ':':
		case ';':
		case ',':
		case '.':
		case '(':
		case ')':
		case '[':
		case ']':
		case '{':
		case '}':
		case '#':
			return true;
		default:
			return false;
	}
}

auto gse::syntax::split_lines(const std::string_view source) -> std::vector<std::string_view> {
	std::vector<std::string_view> lines;
	std::size_t line_start = 0;
	for (std::size_t i = 0; i <= source.size(); ++i) {
		if (i == source.size() || source[i] == '\n') {
			lines.push_back(source.substr(line_start, i - line_start));
			line_start = i + 1;
		}
	}
	return lines;
}

auto gse::syntax::scan_quoted(const quoted_scan_info& info) -> quoted_scan {
	bool escaped = info.escaped;
	for (std::size_t i = info.start; i < info.end; ++i) {
		const char c = info.line[i];
		if (escaped) {
			escaped = false;
		}
		else if (c == '\\') {
			escaped = true;
		}
		else if (c == info.quote) {
			return {
				.close = i + 1,
				.escaped = false,
			};
		}
	}
	return {
		.close = std::nullopt,
		.escaped = escaped,
	};
}

auto gse::syntax::match_literal_start(const std::string_view line, const std::size_t index) -> std::optional<literal_start> {
	static constexpr std::array<literal_prefix, 10> prefixes = {
		literal_prefix{
			.text = "u8R",
			.raw = true,
		},
		literal_prefix{
			.text = "LR",
			.raw = true,
		},
		literal_prefix{
			.text = "uR",
			.raw = true,
		},
		literal_prefix{
			.text = "UR",
			.raw = true,
		},
		literal_prefix{
			.text = "u8",
			.raw = false,
		},
		literal_prefix{
			.text = "R",
			.raw = true,
		},
		literal_prefix{
			.text = "L",
			.raw = false,
		},
		literal_prefix{
			.text = "u",
			.raw = false,
		},
		literal_prefix{
			.text = "U",
			.raw = false,
		},
		literal_prefix{
			.text = "",
			.raw = false,
		},
	};
	for (const literal_prefix& prefix : prefixes) {
		if (index + prefix.text.size() >= line.size()) {
			continue;
		}
		if (line.substr(index, prefix.text.size()) != prefix.text) {
			continue;
		}
		const char quote = line[index + prefix.text.size()];
		if (prefix.raw) {
			if (quote == '"') {
				return literal_start{
					.prefix_len = prefix.text.size(),
					.raw = true,
				};
			}
		}
		else if (quote == '"' || quote == '\'') {
			return literal_start{
				.prefix_len = prefix.text.size(),
				.raw = false,
			};
		}
	}
	return std::nullopt;
}

auto gse::syntax::tokenize(const std::span<const std::string_view> lines) -> lex_result {
	lex_result result;
	lex_mode mode = lex_mode::normal;
	std::string raw_delimiter;
	char quoted_delimiter = '\0';
	bool quoted_escaped = false;

	for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
		result.line_start_modes.push_back(mode);

		const std::string_view line_text = lines[line_index];
		const auto line = static_cast<std::uint32_t>(line_index);
		const std::optional<std::size_t> splice = splice_position(line_text);
		const std::size_t logical_end = splice.value_or(line_text.size());
		std::size_t i = 0;

		auto push = [&](const std::size_t start, const std::size_t end, const token_type type) {
			if (end > start) {
				result.tokens.push_back({
					.line = line,
					.start = static_cast<std::uint32_t>(start),
					.end = static_cast<std::uint32_t>(end),
					.type = type,
					.text = line_text.substr(start, end - start),
				});
			}
		};

		if (mode == lex_mode::line_comment) {
			push(0, line_text.size(), token_type::line_comment);
			mode = splice ? lex_mode::line_comment : lex_mode::normal;
			continue;
		}
		if (mode == lex_mode::block_comment) {
			const std::size_t close = line_text.find("*/");
			if (close == std::string_view::npos) {
				push(0, line_text.size(), token_type::block_comment);
				continue;
			}
			push(0, close + 2, token_type::block_comment);
			i = close + 2;
			mode = lex_mode::normal;
		}
		else if (mode == lex_mode::raw_string) {
			const std::string needle = ")" + raw_delimiter + "\"";
			const std::size_t close = line_text.find(needle);
			if (close == std::string_view::npos) {
				push(0, line_text.size(), token_type::string);
				continue;
			}
			push(0, close + needle.size(), token_type::string);
			i = close + needle.size();
			mode = lex_mode::normal;
			raw_delimiter.clear();
		}
		else if (mode == lex_mode::quoted_string) {
			const quoted_scan scan = scan_quoted({
				.line = line_text,
				.start = 0,
				.end = logical_end,
				.quote = quoted_delimiter,
				.escaped = quoted_escaped,
			});
			if (!scan.close) {
				push(0, line_text.size(), token_type::string);
				mode = splice ? lex_mode::quoted_string : lex_mode::normal;
				quoted_escaped = splice ? scan.escaped : false;
				continue;
			}
			push(0, *scan.close, token_type::string);
			i = *scan.close;
			mode = lex_mode::normal;
			quoted_delimiter = '\0';
			quoted_escaped = false;
		}

		std::size_t first_code = 0;
		while (first_code < line_text.size() && is_space(line_text[first_code])) {
			++first_code;
		}

		bool include_like = false;
		if (i <= first_code && first_code < line_text.size() && line_text[first_code] == '#') {
			std::size_t j = first_code + 1;
			while (j < line_text.size() && is_space(line_text[j])) {
				++j;
			}
			const std::size_t directive_start = j;
			while (j < line_text.size() && is_ident_char(line_text[j])) {
				++j;
			}
			push(first_code, j, token_type::preprocessor);
			const std::string_view directive = line_text.substr(directive_start, j - directive_start);
			include_like = directive == "include" || directive == "import";
			i = j;
		}

		while (i < line_text.size()) {
			const char c = line_text[i];

			if (is_space(c)) {
				++i;
				continue;
			}

			if (c == '/' && i + 1 < line_text.size() && line_text[i + 1] == '/') {
				push(i, line_text.size(), token_type::line_comment);
				if (splice) {
					mode = lex_mode::line_comment;
				}
				break;
			}

			if (c == '/' && i + 1 < line_text.size() && line_text[i + 1] == '*') {
				const std::size_t close = line_text.find("*/", i + 2);
				if (close == std::string_view::npos) {
					push(i, line_text.size(), token_type::block_comment);
					mode = lex_mode::block_comment;
					break;
				}
				push(i, close + 2, token_type::block_comment);
				i = close + 2;
				continue;
			}

			if (include_like && c == '<') {
				const std::size_t close = line_text.find('>', i + 1);
				const std::size_t end = close == std::string_view::npos ? line_text.size() : close + 1;
				push(i, end, token_type::string);
				include_like = false;
				i = end;
				continue;
			}

			if (const std::optional<literal_start> literal = match_literal_start(line_text, i)) {
				const std::size_t open = i + literal->prefix_len;
				if (literal->raw) {
					std::size_t delimiter_end = open + 1;
					while (delimiter_end < line_text.size() && line_text[delimiter_end] != '(' && line_text[delimiter_end] != '"' && !is_space(line_text[delimiter_end])) {
						++delimiter_end;
					}
					if (delimiter_end < line_text.size() && line_text[delimiter_end] == '(') {
						const std::string delimiter(line_text.substr(open + 1, delimiter_end - (open + 1)));
						const std::string needle = ")" + delimiter + "\"";
						const std::size_t close = line_text.find(needle, delimiter_end + 1);
						if (close == std::string_view::npos) {
							push(i, line_text.size(), token_type::string);
							mode = lex_mode::raw_string;
							raw_delimiter = delimiter;
							break;
						}
						push(i, close + needle.size(), token_type::string);
						i = close + needle.size();
						continue;
					}
				}

				const char quote = line_text[open];
				const quoted_scan scan = scan_quoted({
					.line = line_text,
					.start = open + 1,
					.end = logical_end,
					.quote = quote,
				});
				if (scan.close) {
					push(i, *scan.close, token_type::string);
					i = *scan.close;
					continue;
				}
				push(i, line_text.size(), token_type::string);
				i = line_text.size();
				if (splice) {
					mode = lex_mode::quoted_string;
					quoted_delimiter = quote;
					quoted_escaped = scan.escaped;
				}
				continue;
			}

			if (is_digit(c) || (c == '.' && i + 1 < line_text.size() && is_digit(line_text[i + 1]))) {
				std::size_t j = i + 1;
				while (j < line_text.size()) {
					const char next = line_text[j];
					const bool exponent_sign = (next == '+' || next == '-') && (line_text[j - 1] == 'e' || line_text[j - 1] == 'E' || line_text[j - 1] == 'p' || line_text[j - 1] == 'P');
					if (is_ident_char(next) || next == '.' || next == '\'' || exponent_sign) {
						++j;
						continue;
					}
					break;
				}
				push(i, j, token_type::number);
				i = j;
				continue;
			}

			if (is_ident_start(c)) {
				std::size_t j = i + 1;
				while (j < line_text.size() && is_ident_char(line_text[j])) {
					++j;
				}
				push(i, j, token_type::identifier);
				i = j;
				continue;
			}

			if (is_punct(c)) {
				std::size_t j = i + 1;
				while (j < line_text.size() && is_punct(line_text[j]) && !(line_text[j] == '/' && j + 1 < line_text.size() && (line_text[j + 1] == '/' || line_text[j + 1] == '*'))) {
					++j;
				}
				push(i, j, token_type::punctuation);
				i = j;
				continue;
			}

			++i;
		}
	}

	return result;
}
