export module gse.ide.highlight:syntax_producer;

import std;
import gse;
import gse.ide.analysis;

export namespace gse::ide {
	struct syntax_producer {
		struct semantic_data {
			std::unordered_map<std::uint64_t, analysis::semantic_kind> at;
			std::unordered_set<std::string, gse::transparent_hash, gse::transparent_equal> types;
			std::unordered_set<std::string, gse::transparent_hash, gse::transparent_equal> enums;
			std::unordered_set<std::string, gse::transparent_hash, gse::transparent_equal> template_params;
			std::unordered_map<std::string, analysis::semantic_kind, gse::transparent_hash, gse::transparent_equal> names;
		};

		struct highlight_job {
			std::atomic<bool> done = false;
			std::vector<gse::gui::text_span> spans;
		};

		struct data {
			std::vector<gse::gui::text_span> spans;
			std::shared_ptr<const semantic_data> semantic;
			std::shared_ptr<highlight_job> pending;
		};

		static auto rebuild(
			data& d,
			const gse::gui::text_buffer& buffer
		) -> void;

		static auto set_semantic(
			data& d,
			std::span<const analysis::semantic_token> tokens,
			std::span<const std::string> type_names,
			std::span<const std::string> template_params,
			const gse::gui::text_buffer& buffer
		) -> void;

		static auto poll(
			data& d
		) -> void;

		static auto highlight(
			std::string_view source,
			const semantic_data* sem
		) -> std::vector<gse::gui::text_span>;
	};
}

namespace gse::ide::syntax {
	enum class kind {
		comment,
		keyword,
		control_keyword,
		literal,
		number,
		string,
		preprocessor,
		function_call,
		type,
		builtin_type,
		member,
		namespace_ref,
		punctuation
	};

	auto rgb(std::uint32_t hex) -> gse::vec4f {
		return {
			static_cast<float>((hex >> 16) & 0xffu) / 255.f,
			static_cast<float>((hex >> 8) & 0xffu) / 255.f,
			static_cast<float>(hex & 0xffu) / 255.f,
			1.f
		};
	}

	auto color_for(kind k) -> gse::vec4f {
		switch (k) {
			case kind::comment: return rgb(0x4b5b7e);
			case kind::keyword: return rgb(0x2a7195);
			case kind::control_keyword: return rgb(0xd57192);
			case kind::literal: return rgb(0x9586df);
			case kind::number: return rgb(0x9586df);
			case kind::string: return rgb(0x9586df);
			case kind::preprocessor: return rgb(0x2b3959);
			case kind::function_call: return rgb(0xd57192);
			case kind::type: return rgb(0x2aa1d9);
			case kind::builtin_type: return rgb(0x2a7195);
			case kind::member: return rgb(0xd6794d);
			case kind::namespace_ref: return rgb(0x9ca5b8);
			case kind::punctuation: return rgb(0x9ca5b8);
		}
		return rgb(0xc2cce0);
	}

	auto color_for_semantic(analysis::semantic_kind k) -> gse::vec4f {
		switch (k) {
			case analysis::semantic_kind::variable: return rgb(0x9ca5b8);
			case analysis::semantic_kind::parameter: return rgb(0x4b5b7e);
			case analysis::semantic_kind::function: return rgb(0xd57192);
			case analysis::semantic_kind::member: return rgb(0xd6794d);
			case analysis::semantic_kind::type: return rgb(0x2aa1d9);
			case analysis::semantic_kind::enum_member: return rgb(0x7aa549);
			case analysis::semantic_kind::name_space: return rgb(0x9ca5b8);
			case analysis::semantic_kind::global: return rgb(0xb4911b);
		}
		return rgb(0xc2cce0);
	}

	auto scopeable_semantic(analysis::semantic_kind k) -> bool {
		return k == analysis::semantic_kind::type || k == analysis::semantic_kind::name_space;
	}

	auto semantic_precedence(analysis::semantic_kind k) -> int {
		switch (k) {
			case analysis::semantic_kind::type:
			case analysis::semantic_kind::name_space:
				return 4;
			case analysis::semantic_kind::function:
			case analysis::semantic_kind::member:
			case analysis::semantic_kind::enum_member:
			case analysis::semantic_kind::global:
				return 3;
			case analysis::semantic_kind::parameter:
				return 2;
			case analysis::semantic_kind::variable:
				return 1;
		}
		return 0;
	}

	auto is_ident_start(char c) -> bool {
		return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
	}

	auto is_ident_char(char c) -> bool {
		return is_ident_start(c) || (c >= '0' && c <= '9');
	}

	auto is_digit(char c) -> bool {
		return c >= '0' && c <= '9';
	}

	auto is_space(char c) -> bool {
		return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
	}

	auto is_punct(char c) -> bool {
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

	auto classify_word(std::string_view w) -> std::optional<kind> {
		static const std::unordered_set<std::string_view> control = {
			"if", "else", "for", "while", "do", "switch", "case", "default",
			"break", "continue", "return", "goto", "throw", "try", "catch",
			"co_await", "co_return", "co_yield", "new", "delete"
		};
		static const std::unordered_set<std::string_view> keywords = {
			"int", "char", "bool", "float", "double", "void", "auto", "short",
			"long", "signed", "unsigned", "wchar_t", "char8_t", "char16_t",
			"char32_t", "const", "constexpr", "consteval", "constinit",
			"volatile", "mutable", "static", "extern", "register",
			"thread_local", "inline", "virtual", "explicit", "friend",
			"typename", "template", "class", "struct", "union", "enum",
			"namespace", "using", "public", "private", "protected", "this",
			"operator", "sizeof", "alignof", "alignas", "decltype", "noexcept",
			"static_cast", "dynamic_cast", "reinterpret_cast", "const_cast",
			"static_assert", "concept", "requires", "module", "import",
			"export", "typeid", "and", "or", "not", "xor", "bitand", "bitor",
			"compl", "and_eq", "or_eq", "xor_eq", "not_eq",
			"override", "final", "typedef", "asm"
		};
		static const std::unordered_set<std::string_view> literals = {
			"true", "false", "nullptr"
		};
		if (literals.contains(w)) {
			return kind::literal;
		}
		if (control.contains(w)) {
			return kind::control_keyword;
		}
		if (keywords.contains(w)) {
			return kind::keyword;
		}
		return std::nullopt;
	}

	consteval auto collect_engine_enum_members() -> std::vector<const char*> {
		std::vector<const char*> out;
		std::vector<std::meta::info> worklist;
		worklist.push_back(^^gse);

		for (std::size_t i = 0; i < worklist.size(); ++i) {
			for (const auto m : std::meta::members_of(worklist[i], std::meta::access_context::unchecked())) {
				if (std::meta::is_type(m) && std::meta::is_enum_type(m)) {
					for (const auto e : std::meta::enumerators_of(m)) {
						if (std::meta::has_identifier(e)) {
							out.push_back(std::define_static_string(std::meta::identifier_of(e)));
						}
					}
				}
				else if (std::meta::is_namespace(m) && std::meta::has_identifier(m)) {
					worklist.push_back(m);
				}
			}
		}
		return out;
	}

	auto is_engine_enum_member(std::string_view w) -> bool {
		static constexpr auto names = std::define_static_array(collect_engine_enum_members());
		static const std::unordered_set<std::string_view> set(names.begin(), names.end());
		return set.contains(w);
	}

	consteval auto collect_engine_globals() -> std::vector<const char*> {
		std::vector<const char*> out;
		std::vector<std::meta::info> worklist;
		worklist.push_back(^^gse);

		for (std::size_t i = 0; i < worklist.size(); ++i) {
			for (const auto m : std::meta::members_of(worklist[i], std::meta::access_context::unchecked())) {
				if (std::meta::is_variable(m) && std::meta::has_identifier(m)) {
					out.push_back(std::define_static_string(std::meta::identifier_of(m)));
				}
				else if (std::meta::is_namespace(m) && std::meta::has_identifier(m)) {
					worklist.push_back(m);
				}
			}
		}
		return out;
	}

	auto is_engine_global(std::string_view w) -> bool {
		static constexpr auto names = std::define_static_array(collect_engine_globals());
		static const std::unordered_set<std::string_view> set(names.begin(), names.end());
		return set.contains(w);
	}

	consteval auto collect_engine_types() -> std::vector<const char*> {
		std::vector<const char*> out;
		std::vector<std::meta::info> worklist;
		worklist.push_back(^^gse);

		for (std::size_t i = 0; i < worklist.size(); ++i) {
			for (const auto m : std::meta::members_of(worklist[i], std::meta::access_context::unchecked())) {
				const bool type_like = std::meta::is_type(m) || std::meta::is_concept(m)
					|| (std::meta::is_template(m) && !std::meta::is_function_template(m));
				if (type_like && std::meta::has_identifier(m)) {
					out.push_back(std::define_static_string(std::meta::identifier_of(m)));
				}
				else if (std::meta::is_namespace(m) && std::meta::has_identifier(m)) {
					worklist.push_back(m);
				}
			}
		}
		return out;
	}

	auto is_engine_type(std::string_view w) -> bool {
		static constexpr auto names = std::define_static_array(collect_engine_types());
		static const std::unordered_set<std::string_view> set(names.begin(), names.end());
		return set.contains(w);
	}

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

	struct literal_prefix {
		std::string_view text;
		bool raw = false;
	};

	struct literal_start {
		std::size_t prefix_len = 0;
		bool raw = false;
	};

	auto match_literal_start(std::string_view s, std::size_t i) -> std::optional<literal_start> {
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

	auto split_lines(std::string_view source) -> std::vector<std::string_view> {
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

	auto is_significant(const token& t) -> bool {
		return t.type != token_type::line_comment && t.type != token_type::block_comment;
	}

	auto prev_significant_index(std::span<const token> tokens, std::size_t idx) -> std::size_t {
		for (std::size_t k = idx; k-- > 0;) {
			if (is_significant(tokens[k])) {
				return k;
			}
		}
		return tokens.size();
	}

	auto next_significant_index(std::span<const token> tokens, std::size_t idx) -> std::size_t {
		for (std::size_t k = idx + 1; k < tokens.size(); ++k) {
			if (is_significant(tokens[k])) {
				return k;
			}
		}
		return tokens.size();
	}

	auto semantic_at(const syntax_producer::semantic_data* sem, std::uint32_t line, std::uint32_t col) -> const analysis::semantic_kind* {
		if (!sem) {
			return nullptr;
		}
		const std::uint64_t key = (static_cast<std::uint64_t>(line) << 32) | col;
		const auto it = sem->at.find(key);
		return it == sem->at.end() ? nullptr : &it->second;
	}

	auto name_at(const syntax_producer::semantic_data* sem, std::string_view w) -> const analysis::semantic_kind* {
		if (!sem) {
			return nullptr;
		}
		const auto it = sem->names.find(w);
		return it == sem->names.end() ? nullptr : &it->second;
	}

	auto tokenize(std::span<const std::string_view> lines) -> std::vector<token>;

	auto collect_module_lines(std::span<const std::string_view> lines) -> std::vector<char>;

	auto classify_identifier(
		std::span<const token> tokens,
		std::size_t idx,
		const syntax_producer::semantic_data* sem,
		bool module_directive
	) -> std::optional<gse::vec4f>;

	auto compute(
		std::string_view source,
		const syntax_producer::semantic_data* sem
	) -> std::vector<gse::gui::text_span>;
}

auto gse::ide::syntax::tokenize(std::span<const std::string_view> lines) -> std::vector<token> {
	std::vector<token> out;
	lex_mode mode = lex_mode::normal;
	std::string raw_delim;

	for (std::size_t li = 0; li < lines.size(); ++li) {
		const std::string_view s = lines[li];
		const auto line = static_cast<std::uint32_t>(li);
		std::size_t i = 0;

		auto push = [&](std::size_t a, std::size_t b, token_type type) {
			if (b > a) {
				out.push_back({ line, static_cast<std::uint32_t>(a), static_cast<std::uint32_t>(b), type, s.substr(a, b - a) });
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

	return out;
}

auto gse::ide::syntax::collect_module_lines(std::span<const std::string_view> lines) -> std::vector<char> {
	std::vector<char> out(lines.size(), 0);

	for (std::size_t li = 0; li < lines.size(); ++li) {
		const std::string_view s = lines[li];
		std::size_t p = 0;
		while (p < s.size() && is_space(s[p])) {
			++p;
		}
		auto scan = [&]() -> std::string_view {
			const std::size_t w0 = p;
			while (p < s.size() && is_ident_char(s[p])) {
				++p;
			}
			return s.substr(w0, p - w0);
		};
		std::string_view head = scan();
		if (head == "export") {
			while (p < s.size() && is_space(s[p])) {
				++p;
			}
			head = scan();
		}
		out[li] = (head == "import" || head == "module") ? 1 : 0;
	}

	return out;
}

auto gse::ide::syntax::classify_identifier(std::span<const token> tokens, std::size_t idx, const syntax_producer::semantic_data* sem, bool module_directive) -> std::optional<gse::vec4f> {
	const token& t = tokens[idx];
	const std::string_view word = t.text;

	if (const std::optional<kind> wk = classify_word(word)) {
		return color_for(*wk);
	}
	if (module_directive) {
		return std::nullopt;
	}

	const std::size_t pi = prev_significant_index(tokens, idx);
	const std::size_t ni = next_significant_index(tokens, idx);
	const token* prev = pi < tokens.size() ? &tokens[pi] : nullptr;
	const token* next = ni < tokens.size() ? &tokens[ni] : nullptr;
	const bool colon_qualified = prev && prev->type == token_type::punctuation && prev->text == "::" && prev->end == t.start;
	const bool member = prev && prev->type == token_type::punctuation && (prev->text == "." || prev->text == "->") && prev->end == t.start;
	const bool call = next && next->type == token_type::punctuation && next->text.starts_with("(");
	const bool template_head = next && next->type == token_type::punctuation && next->start == t.end && next->text == "<";
	const bool scope = next && next->type == token_type::punctuation && next->text.starts_with("::");

	if (member) {
		return color_for((call || template_head) ? kind::function_call : kind::member);
	}
	if (colon_qualified && sem && sem->types.contains(word)) {
		return color_for(kind::type);
	}
	if (colon_qualified && is_engine_type(word)) {
		return color_for(kind::type);
	}
	if (const analysis::semantic_kind* sk = semantic_at(sem, t.line, t.start); sk && (!scope || scopeable_semantic(*sk))) {
		return color_for_semantic(*sk);
	}
	if (const analysis::semantic_kind* nk = name_at(sem, word); nk && (!scope || scopeable_semantic(*nk))) {
		return color_for_semantic(*nk);
	}
	if (sem && sem->enums.contains(word)) {
		return color_for_semantic(analysis::semantic_kind::enum_member);
	}
	if (sem && sem->types.contains(word)) {
		return color_for(kind::type);
	}
	if (colon_qualified && is_engine_enum_member(word)) {
		return color_for_semantic(analysis::semantic_kind::enum_member);
	}
	if (colon_qualified && !scope && is_engine_global(word)) {
		return color_for_semantic(analysis::semantic_kind::global);
	}
	if (call || template_head) {
		return color_for(kind::function_call);
	}
	if (scope) {
		return color_for(kind::namespace_ref);
	}
	return std::nullopt;
}

auto gse::ide::syntax::compute(const std::string_view source, const syntax_producer::semantic_data* sem) -> std::vector<gse::gui::text_span> {
	std::vector<gse::gui::text_span> spans;

	const std::vector<std::string_view> lines = split_lines(source);
	const std::vector<token> tokens = tokenize(lines);
	const std::vector<char> module_lines = collect_module_lines(lines);

	auto emit = [&](const token& t, kind k) {
		spans.push_back({ t.line, t.start, t.end, color_for(k) });
	};
	auto emit_color = [&](const token& t, gse::vec4f color) {
		spans.push_back({ t.line, t.start, t.end, color });
	};

	for (std::size_t idx = 0; idx < tokens.size(); ++idx) {
		const token& t = tokens[idx];
		switch (t.type) {
			case token_type::line_comment:
			case token_type::block_comment:
				emit(t, kind::comment);
				break;
			case token_type::string:
				emit(t, kind::string);
				break;
			case token_type::number:
				emit(t, kind::number);
				break;
			case token_type::preprocessor:
				emit(t, kind::preprocessor);
				break;
			case token_type::punctuation:
				emit(t, kind::punctuation);
				break;
			case token_type::identifier: {
				const bool module_directive = t.line < module_lines.size() && module_lines[t.line] != 0;
				if (const std::optional<gse::vec4f> color = classify_identifier(tokens, idx, sem, module_directive)) {
					emit_color(t, *color);
				}
				break;
			}
		}
	}

	return spans;
}

auto gse::ide::syntax_producer::rebuild(data& d, const gse::gui::text_buffer& buffer) -> void {
	auto job = std::make_shared<highlight_job>();

	std::size_t total = 0;
	for (std::size_t li = 0; li < buffer.line_count(); ++li) {
		total += buffer.line(li).size() + 1;
	}
	std::string snapshot;
	snapshot.reserve(total);
	for (std::size_t li = 0; li < buffer.line_count(); ++li) {
		if (li != 0) {
			snapshot += '\n';
		}
		snapshot += buffer.line(li);
	}

	std::shared_ptr<const semantic_data> sem = d.semantic;
	d.pending = job;

	std::thread([job, snapshot = std::move(snapshot), sem] {
		job->spans = gse::ide::syntax::compute(snapshot, sem.get());
		job->done.store(true, std::memory_order_release);
	}).detach();
}

auto gse::ide::syntax_producer::poll(data& d) -> void {
	if (d.pending && d.pending->done.load(std::memory_order_acquire)) {
		d.spans = std::move(d.pending->spans);
		d.pending.reset();
	}
}

auto gse::ide::syntax_producer::highlight(const std::string_view source, const semantic_data* sem) -> std::vector<gse::gui::text_span> {
	return gse::ide::syntax::compute(source, sem);
}

auto gse::ide::syntax_producer::set_semantic(data& d, std::span<const analysis::semantic_token> tokens, std::span<const std::string> type_names, std::span<const std::string> template_params, const gse::gui::text_buffer& buffer) -> void {
	auto sem = std::make_shared<semantic_data>();

	for (const std::string& name : type_names) {
		sem->types.emplace(name);
	}

	for (const std::string& name : template_params) {
		sem->template_params.emplace(name);
		sem->types.emplace(name);
	}

	for (const analysis::semantic_token& t : tokens) {
		if (t.line == 0 || t.column == 0 || t.length == 0) {
			continue;
		}

		const std::uint32_t line = t.line - 1;
		const std::uint32_t col = t.column - 1;

		if (line >= buffer.line_count()) {
			continue;
		}
		const std::string_view source = buffer.line(line);
		if (static_cast<std::size_t>(col) + t.length > source.size()) {
			continue;
		}
		if (col > 0 && syntax::is_ident_char(source[col - 1])) {
			continue;
		}
		if (static_cast<std::size_t>(col) + t.length < source.size() && syntax::is_ident_char(source[col + t.length])) {
			continue;
		}

		const std::uint64_t key = (static_cast<std::uint64_t>(line) << 32) | col;
		if (const auto it = sem->at.find(key); it == sem->at.end() || syntax::semantic_precedence(t.kind) >= syntax::semantic_precedence(it->second)) {
			sem->at[key] = t.kind;
		}

		const std::string_view name = source.substr(col, t.length);
		switch (t.kind) {
			case analysis::semantic_kind::type:
				sem->types.emplace(name);
				break;
			case analysis::semantic_kind::enum_member:
				sem->enums.emplace(name);
				break;
			case analysis::semantic_kind::variable:
			case analysis::semantic_kind::parameter:
			case analysis::semantic_kind::global:
				sem->names.emplace(name, t.kind);
				break;
			default:
				break;
		}
	}

	d.semantic = std::move(sem);
}
