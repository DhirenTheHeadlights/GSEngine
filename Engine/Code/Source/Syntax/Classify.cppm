export module gse.syntax:classify;

import std;

import gse.meta;

import :kinds;
import :lexer;

export namespace gse::syntax {
	struct classified_token {
		std::uint32_t line = 0;
		std::uint32_t start = 0;
		std::uint32_t end = 0;
		kind k = kind::punctuation;
	};

	auto classify_word(
		std::string_view word
	) -> std::optional<kind>;

	auto reserved_words() -> std::span<const std::string_view>;

	auto classify_token(
		const token& t
	) -> std::optional<kind>;

	auto classify(
		std::span<const token> tokens
	) -> std::vector<classified_token>;

	auto classify(
		std::string_view source
	) -> std::vector<classified_token>;
}

namespace gse::syntax {
	constexpr std::array control_words = std::to_array<std::string_view>({
		"if", "else", "for", "while", "do", "switch", "case", "default",
		"break", "continue", "return", "goto", "throw", "try", "catch",
		"co_await", "co_return", "co_yield", "new", "delete"
	});

	constexpr std::array keyword_words = std::to_array<std::string_view>({
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
		"override", "final", "typedef", "asm", "contract_assert", "pre", "post"
	});

	constexpr std::array literal_words = std::to_array<std::string_view>({
		"true", "false", "nullptr"
	});
}

auto gse::syntax::classify_word(const std::string_view word) -> std::optional<kind> {
	static const std::unordered_set<std::string_view> control(control_words.begin(), control_words.end());
	static const std::unordered_set<std::string_view> keywords(keyword_words.begin(), keyword_words.end());
	static const std::unordered_set<std::string_view> literals(literal_words.begin(), literal_words.end());
	if (literals.contains(word)) {
		return kind::literal;
	}
	if (control.contains(word)) {
		return kind::control_keyword;
	}
	if (keywords.contains(word)) {
		return kind::keyword;
	}
	return std::nullopt;
}

auto gse::syntax::reserved_words() -> std::span<const std::string_view> {
	static const std::vector<std::string_view> words = [] {
		std::vector<std::string_view> all;
		all.reserve(control_words.size() + keyword_words.size() + literal_words.size());
		all.append_range(control_words);
		all.append_range(keyword_words);
		all.append_range(literal_words);
		return all;
	}();
	return words;
}

auto gse::syntax::classify_token(const token& t) -> std::optional<kind> {
	if (!enum_has_annotation<kind>(t.type)) {
		return classify_word(t.text);
	}
	return annotation_from_enum(t.type, kind::punctuation);
}

auto gse::syntax::classify(const std::span<const token> tokens) -> std::vector<classified_token> {
	std::vector<classified_token> out;
	out.reserve(tokens.size());
	for (const token& t : tokens) {
		if (const std::optional<kind> k = classify_token(t)) {
			out.push_back({
				.line = t.line,
				.start = t.start,
				.end = t.end,
				.k = *k,
			});
		}
	}
	return out;
}

auto gse::syntax::classify(const std::string_view source) -> std::vector<classified_token> {
	const std::vector<std::string_view> lines = split_lines(source);
	const lex_result lex = tokenize(lines);
	return classify(lex.tokens);
}