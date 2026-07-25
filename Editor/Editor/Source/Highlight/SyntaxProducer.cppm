export module gse.ide.highlight:syntax_producer;

import std;
import gse;
import gse.ide.analysis;

import :lexer;

export namespace gse::ide {
	struct syntax_producer {
		struct semantic_data {
			std::unordered_map<std::uint64_t, analysis::semantic_kind> at;
		};

		using name_kinds = std::unordered_map<std::string_view, analysis::semantic_kind>;

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
			const gse::gui::text_buffer& buffer
		) -> void;

		static auto poll(
			data& d
		) -> void;

		static auto highlight(
			std::string_view source,
			const semantic_data* sem,
			const name_kinds* names
		) -> std::vector<gse::gui::text_span>;

		static auto identifier_names(
			std::string_view source
		) -> std::vector<std::string_view>;

		static auto is_language_word(
			std::string_view word
		) -> bool;
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
			case analysis::semantic_kind::attribute: return rgb(0x2a7195);
		}
		return rgb(0xc2cce0);
	}

	auto follows_member_access(const std::string_view source, const std::uint32_t column) -> bool {
		std::size_t pos = std::min<std::size_t>(column, source.size());
		while (pos > 0 && std::isspace(static_cast<unsigned char>(source[pos - 1]))) {
			--pos;
		}
		if (pos > 0 && source[pos - 1] == '.') {
			return true;
		}
		if (pos == 0 || source[pos - 1] != '>') {
			return false;
		}
		--pos;
		while (pos > 0 && std::isspace(static_cast<unsigned char>(source[pos - 1]))) {
			--pos;
		}
		return pos > 0 && source[pos - 1] == '-';
	}

	auto semantic_precedence(const analysis::semantic_kind k, const bool member_access) -> int {
		if (member_access && (k == analysis::semantic_kind::member || k == analysis::semantic_kind::function)) {
			return 5;
		}
		switch (k) {
			case analysis::semantic_kind::type:
			case analysis::semantic_kind::name_space:
				return 4;
			case analysis::semantic_kind::function:
			case analysis::semantic_kind::member:
			case analysis::semantic_kind::enum_member:
			case analysis::semantic_kind::global:
			case analysis::semantic_kind::attribute:
				return 3;
			case analysis::semantic_kind::parameter:
				return 2;
			case analysis::semantic_kind::variable:
				return 1;
		}
		return 0;
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
			"override", "final", "typedef", "asm", "contract_assert", "pre", "post"
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

	auto semantic_at(const syntax_producer::semantic_data* sem, std::uint32_t line, std::uint32_t col) -> const analysis::semantic_kind* {
		if (!sem) {
			return nullptr;
		}
		const std::uint64_t key = (static_cast<std::uint64_t>(line) << 32) | col;
		const auto it = sem->at.find(key);
		return it == sem->at.end() ? nullptr : &it->second;
	}

	auto compute(
		std::string_view source,
		const syntax_producer::semantic_data* sem,
		const syntax_producer::name_kinds* names
	) -> std::vector<gse::gui::text_span>;
}

auto gse::ide::syntax::compute(const std::string_view source, const syntax_producer::semantic_data* sem, const syntax_producer::name_kinds* names) -> std::vector<gse::gui::text_span> {
	std::vector<gse::gui::text_span> spans;

	const std::vector<std::string_view> lines = split_lines(source);
	const lex_result lex = tokenize(lines);

	auto emit = [&](const token& t, kind k) {
		spans.push_back({ t.line, t.start, t.end, color_for(k) });
	};

	for (const token& t : lex.tokens) {
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
				if (const std::optional<kind> wk = classify_word(t.text)) {
					emit(t, *wk);
				}
				else if (const analysis::semantic_kind* sk = semantic_at(sem, t.line, t.start)) {
					spans.push_back({ t.line, t.start, t.end, color_for_semantic(*sk) });
				}
				else if (names) {
					if (const auto it = names->find(t.text); it != names->end()) {
						spans.push_back({ t.line, t.start, t.end, color_for_semantic(it->second) });
					}
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
		job->spans = gse::ide::syntax::compute(snapshot, sem.get(), nullptr);
		job->done.store(true, std::memory_order_release);
	}).detach();
}

auto gse::ide::syntax_producer::poll(data& d) -> void {
	if (d.pending && d.pending->done.load(std::memory_order_acquire)) {
		d.spans = std::move(d.pending->spans);
		d.pending.reset();
	}
}

auto gse::ide::syntax_producer::highlight(const std::string_view source, const semantic_data* sem, const name_kinds* names) -> std::vector<gse::gui::text_span> {
	return gse::ide::syntax::compute(source, sem, names);
}

auto gse::ide::syntax_producer::identifier_names(const std::string_view source) -> std::vector<std::string_view> {
	const std::vector<std::string_view> lines = syntax::split_lines(source);
	const syntax::lex_result lex = syntax::tokenize(lines);
	std::vector<std::string_view> names;
	std::unordered_set<std::string_view> seen;
	for (const syntax::token& t : lex.tokens) {
		if (t.type != syntax::token_type::identifier || syntax::classify_word(t.text)) {
			continue;
		}
		if (seen.insert(t.text).second) {
			names.push_back(t.text);
		}
	}
	return names;
}

auto gse::ide::syntax_producer::is_language_word(const std::string_view word) -> bool {
	return syntax::classify_word(word).has_value();
}

auto gse::ide::syntax_producer::set_semantic(data& d, std::span<const analysis::semantic_token> tokens, const gse::gui::text_buffer& buffer) -> void {
	auto sem = std::make_shared<semantic_data>();

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
		const bool member_access = syntax::follows_member_access(source, col);
		if (const auto it = sem->at.find(key); it == sem->at.end()
			|| syntax::semantic_precedence(t.kind, member_access) >= syntax::semantic_precedence(it->second, member_access)) {
			sem->at[key] = t.kind;
		}
	}

	d.semantic = std::move(sem);
}
