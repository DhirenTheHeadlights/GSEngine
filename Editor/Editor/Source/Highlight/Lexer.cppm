export module gse.ide.highlight:lexer;

import std;

export namespace gse::ide::syntax {
	enum class token_type {
		line_comment,
		block_comment,
		string,
		number,
		identifier,
		preprocessor,
		punctuation
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
		raw_string
	};

	struct lex_result {
		std::vector<token> tokens;
		std::vector<lex_mode> line_start_modes;
	};

	auto is_ident_start(char c) -> bool;
	auto is_ident_char(char c) -> bool;
	auto is_digit(char c) -> bool;
	auto is_space(char c) -> bool;
	auto is_punct(char c) -> bool;

	auto split_lines(std::string_view source) -> std::vector<std::string_view>;

	auto tokenize(std::span<const std::string_view> lines) -> lex_result;
}

namespace gse::ide::syntax {
	struct literal_prefix {
		std::string_view text;
		bool raw = false;
	};

	struct literal_start {
		std::size_t prefix_len = 0;
		bool raw = false;
	};

	auto match_literal_start(std::string_view s, std::size_t i) -> std::optional<literal_start>;
}

auto gse::ide::syntax::is_ident_start(const char c) -> bool {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

auto gse::ide::syntax::is_ident_char(const char c) -> bool {
	return is_ident_start(c) || (c >= '0' && c <= '9');
}

auto gse::ide::syntax::is_digit(const char c) -> bool {
	return c >= '0' && c <= '9';
}

auto gse::ide::syntax::is_space(const char c) -> bool {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

auto gse::ide::syntax::is_punct(const char c) -> bool {
	switch (c) {
		case '+': case '-': case '*': case '/': case '%':
		case '=': case '<': case '>': case '!': case '&':
		case '|': case '^': case '~': case '?': case ':':
		case ';': case ',': case '.': case '(': case ')':
		case '[': case ']': case '{': case '}': case '#':
			return true;
		default:
			return false;
	}
}

auto gse::ide::syntax::split_lines(const std::string_view source) -> std::vector<std::string_view> {
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

auto gse::ide::syntax::match_literal_start(const std::string_view s, const std::size_t i) -> std::optional<literal_start> {
	static constexpr std::array<literal_prefix, 10> prefixes = { {
		{ "u8R", true }, { "LR", true }, { "uR", true }, { "UR", true }, { "u8", false },
		{ "R", true }, { "L", false }, { "u", false }, { "U", false }, { "", false }
	} };
	for (const literal_prefix& pf : prefixes) {
		if (i + pf.text.size() >= s.size()) {
			continue;
		}
		if (s.substr(i, pf.text.size()) != pf.text) {
			continue;
		}
		const char q = s[i + pf.text.size()];
		if (pf.raw) {
			if (q == '"') {
				return literal_start{ pf.text.size(), true };
			}
		}
		else if (q == '"' || q == '\'') {
			return literal_start{ pf.text.size(), false };
		}
	}
	return std::nullopt;
}

auto gse::ide::syntax::tokenize(const std::span<const std::string_view> lines) -> lex_result {
	lex_result result;
	lex_mode mode = lex_mode::normal;
	std::string raw_delim;

	for (std::size_t li = 0; li < lines.size(); ++li) {
		result.line_start_modes.push_back(mode);

		const std::string_view s = lines[li];
		const auto line = static_cast<std::uint32_t>(li);
		std::size_t i = 0;

		auto push = [&](std::size_t a, std::size_t b, token_type type) {
			if (b > a) {
				result.tokens.push_back({ line, static_cast<std::uint32_t>(a), static_cast<std::uint32_t>(b), type, s.substr(a, b - a) });
			}
		};

		if (mode == lex_mode::block_comment) {
			const std::size_t close = s.find("*/");
			if (close == std::string_view::npos) {
				push(0, s.size(), token_type::block_comment);
				continue;
			}
			push(0, close + 2, token_type::block_comment);
			i = close + 2;
			mode = lex_mode::normal;
		}
		else if (mode == lex_mode::raw_string) {
			const std::string needle = ")" + raw_delim + "\"";
			const std::size_t close = s.find(needle);
			if (close == std::string_view::npos) {
				push(0, s.size(), token_type::string);
				continue;
			}
			push(0, close + needle.size(), token_type::string);
			i = close + needle.size();
			mode = lex_mode::normal;
			raw_delim.clear();
		}

		std::size_t first_code = 0;
		while (first_code < s.size() && is_space(s[first_code])) {
			++first_code;
		}

		bool include_like = false;
		if (i <= first_code && first_code < s.size() && s[first_code] == '#') {
			std::size_t j = first_code + 1;
			while (j < s.size() && is_space(s[j])) {
				++j;
			}
			const std::size_t d0 = j;
			while (j < s.size() && is_ident_char(s[j])) {
				++j;
			}
			push(first_code, j, token_type::preprocessor);
			const std::string_view directive = s.substr(d0, j - d0);
			include_like = directive == "include" || directive == "import";
			i = j;
		}

		while (i < s.size()) {
			const char c = s[i];

			if (is_space(c)) {
				++i;
				continue;
			}

			if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
				push(i, s.size(), token_type::line_comment);
				break;
			}

			if (c == '/' && i + 1 < s.size() && s[i + 1] == '*') {
				const std::size_t close = s.find("*/", i + 2);
				if (close == std::string_view::npos) {
					push(i, s.size(), token_type::block_comment);
					mode = lex_mode::block_comment;
					break;
				}
				push(i, close + 2, token_type::block_comment);
				i = close + 2;
				continue;
			}

			if (include_like && c == '<') {
				const std::size_t close = s.find('>', i + 1);
				const std::size_t end = close == std::string_view::npos ? s.size() : close + 1;
				push(i, end, token_type::string);
				include_like = false;
				i = end;
				continue;
			}

			if (const std::optional<literal_start> lit = match_literal_start(s, i)) {
				const std::size_t open = i + lit->prefix_len;
				if (lit->raw) {
					std::size_t k = open + 1;
					while (k < s.size() && s[k] != '(' && s[k] != '"' && !is_space(s[k])) {
						++k;
					}
					if (k < s.size() && s[k] == '(') {
						const std::string delim(s.substr(open + 1, k - (open + 1)));
						const std::string needle = ")" + delim + "\"";
						const std::size_t close = s.find(needle, k + 1);
						if (close == std::string_view::npos) {
							push(i, s.size(), token_type::string);
							mode = lex_mode::raw_string;
							raw_delim = delim;
							break;
						}
						push(i, close + needle.size(), token_type::string);
						i = close + needle.size();
						continue;
					}
				}
				const char quote = s[open];
				std::size_t j = open + 1;
				while (j < s.size()) {
					if (s[j] == '\\') {
						j += 2;
						continue;
					}
					if (s[j] == quote) {
						++j;
						break;
					}
					++j;
				}
				push(i, std::min(j, s.size()), token_type::string);
				i = std::min(j, s.size());
				continue;
			}

			if (is_digit(c) || (c == '.' && i + 1 < s.size() && is_digit(s[i + 1]))) {
				std::size_t j = i + 1;
				while (j < s.size()) {
					const char n = s[j];
					const bool exp_sign = (n == '+' || n == '-') && (s[j - 1] == 'e' || s[j - 1] == 'E' || s[j - 1] == 'p' || s[j - 1] == 'P');
					if (is_ident_char(n) || n == '.' || n == '\'' || exp_sign) {
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
				while (j < s.size() && is_ident_char(s[j])) {
					++j;
				}
				push(i, j, token_type::identifier);
				i = j;
				continue;
			}

			if (is_punct(c)) {
				std::size_t j = i + 1;
				while (j < s.size() && is_punct(s[j]) && !(s[j] == '/' && j + 1 < s.size() && (s[j + 1] == '/' || s[j + 1] == '*'))) {
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
