export module gse.ide.highlight:syntax_producer;

import std;
import gse;
import gse.ide.analysis;
import gse.ide.diagnostic;

import :lexer;

export namespace gse::ide {
	struct syntax_producer {
		struct semantic_data {
			std::unordered_map<gui::buffer_position, analysis::semantic_kind> at;
			document_revision revision;

			auto kind_at(
				gui::buffer_position position
			) const -> const analysis::semantic_kind*;
		};

		using name_kinds = std::unordered_map<std::string_view, analysis::semantic_kind>;

		struct highlight_job {
			std::atomic<bool> done = false;
			std::vector<gui::text_span> spans;
			document_revision revision;
		};

		struct data {
			std::vector<gui::text_span> spans;
			document_revision spans_revision;
			std::shared_ptr<const semantic_data> semantic;
			std::shared_ptr<highlight_job> pending;

			auto current_spans(
				document_revision revision
			) const -> std::span<const gui::text_span>;

			auto current_semantic(
				document_revision revision
			) const -> const semantic_data*;
		};

		struct semantic_input {
			std::span<const analysis::semantic_token> tokens;
			const gui::text_buffer& buffer;
			document_revision revision;
			std::string_view source_path;
		};

		static auto rebuild(
			data& d,
			const gui::text_buffer& buffer,
			document_revision revision
		) -> void;

		static auto set_semantic(
			data& d,
			const semantic_input& input
		) -> void;

		static auto clear_semantic(
			data& d
		) -> void;

		static auto poll(
			data& d,
			document_revision revision
		) -> void;

		static auto highlight(
			std::string_view source,
			const semantic_data* sem,
			const name_kinds* names
		) -> std::vector<gui::text_span>;

		static auto identifier_names(
			std::string_view source
		) -> std::vector<std::string_view>;

		static auto is_language_word(
			std::string_view word
		) -> bool;
	};
}

namespace gse::ide::syntax {
	struct kind_info {
		std::uint32_t color = 0;
	};

	enum class kind {
		comment [[= kind_info{ .color = 0x4b5b7e }]],
		keyword [[= kind_info{ .color = 0x2a7195 }]],
		control_keyword [[= kind_info{ .color = 0xd57192 }]],
		literal [[= kind_info{ .color = 0x9586df }]],
		number [[= kind_info{ .color = 0x9586df }]],
		string [[= kind_info{ .color = 0x9586df }]],
		preprocessor [[= kind_info{ .color = 0x2b3959 }]],
		punctuation [[= kind_info{ .color = 0x9ca5b8 }]]
	};

	auto rgb(
		std::uint32_t hex
	) -> vec4f;

	auto color_for(
		kind k
	) -> vec4f;

	auto color_for_semantic(
		analysis::semantic_kind k
	) -> vec4f;

	auto follows_member_access(
		std::string_view source,
		std::uint32_t column
	) -> bool;

	auto semantic_precedence(
		analysis::semantic_kind k,
		bool member_access
	) -> int;

	auto classify_word(
		std::string_view word
	) -> std::optional<kind>;

	auto log_dropped_token(
		std::string_view path,
		const analysis::semantic_token& token,
		std::string_view reason,
		std::string_view row
	) -> void;

	auto drop_reason(
		const analysis::semantic_token& token,
		const gui::text_buffer& buffer,
		std::string_view& row
	) -> std::string_view;

	auto compute(
		std::string_view source,
		const syntax_producer::semantic_data* sem,
		const syntax_producer::name_kinds* names
	) -> std::vector<gui::text_span>;
}

auto gse::ide::syntax::rgb(const std::uint32_t hex) -> vec4f {
	return {
		static_cast<float>((hex >> 16) & 0xffu) / 255.f,
		static_cast<float>((hex >> 8) & 0xffu) / 255.f,
		static_cast<float>(hex & 0xffu) / 255.f,
		1.f
	};
}

auto gse::ide::syntax::color_for(const kind k) -> vec4f {
	const kind_info info = annotation_from_enum<kind_info>(k, {
		.color = 0xc2cce0,
	});
	return rgb(info.color);
}

auto gse::ide::syntax::color_for_semantic(const analysis::semantic_kind k) -> vec4f {
	const analysis::semantic_kind_info info = annotation_from_enum<analysis::semantic_kind_info>(k, {
		.color = 0xc2cce0,
	});
	return rgb(info.color);
}

auto gse::ide::syntax::follows_member_access(const std::string_view source, const std::uint32_t column) -> bool {
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

auto gse::ide::syntax::semantic_precedence(const analysis::semantic_kind k, const bool member_access) -> int {
	if (member_access && (k == analysis::semantic_kind::member || k == analysis::semantic_kind::function)) {
		return 5;
	}
	return annotation_from_enum<analysis::semantic_kind_info>(k, {}).precedence;
}

auto gse::ide::syntax::classify_word(const std::string_view word) -> std::optional<kind> {
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

auto gse::ide::syntax::log_dropped_token(const std::string_view path, const analysis::semantic_token& token, const std::string_view reason, const std::string_view row) -> void {
	log::println(
		log::level::warning,
		log::category::general,
		"[semantic] highlight token dropped at {}:{}:{} len {} kind {}: {}",
		path,
		token.line,
		token.column,
		token.length,
		token.kind,
		reason
	);
	if (!row.empty()) {
		log::println(log::level::warning, log::category::general, "[semantic]   line: |{}|", row);
	}
}

auto gse::ide::syntax::drop_reason(const analysis::semantic_token& token, const gui::text_buffer& buffer, std::string_view& row) -> std::string_view {
	if (token.line == 0 || token.column == 0 || token.length == 0) {
		return "the compiler emitted a zero line, column, or length";
	}
	const std::uint32_t line = token.line - 1;
	if (line >= buffer.line_count()) {
		return "the line is past the end of the buffer, so the index is stale against the open document";
	}
	row = buffer.line(line);
	const std::size_t column = token.column - 1;
	if (column + token.length > row.size()) {
		return "the token extends past the end of the line, so the columns disagree with the document";
	}
	if (column > 0 && is_ident_char(row[column - 1])) {
		return "the token starts inside an identifier, so its column is misaligned";
	}
	if (column + token.length < row.size() && is_ident_char(row[column + token.length])) {
		return "the token ends inside an identifier, so its length is short";
	}
	return {};
}

auto gse::ide::syntax::compute(const std::string_view source, const syntax_producer::semantic_data* sem, const syntax_producer::name_kinds* names) -> std::vector<gui::text_span> {
	std::vector<gui::text_span> spans;

	const std::vector<std::string_view> lines = split_lines(source);
	const lex_result lex = tokenize(lines);

	auto emit = [&](const token& t, kind k) {
		spans.push_back({
			.line = t.line,
			.start_col = t.start,
			.end_col = t.end,
			.color = color_for(k),
		});
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
				else if (const analysis::semantic_kind* sk = sem ? sem->kind_at({
					.line = t.line,
					.column = t.start,
				}) : nullptr) {
					spans.push_back({
						.line = t.line,
						.start_col = t.start,
						.end_col = t.end,
						.color = color_for_semantic(*sk),
					});
				}
				else if (names) {
					if (const auto it = names->find(t.text); it != names->end()) {
						spans.push_back({
							.line = t.line,
							.start_col = t.start,
							.end_col = t.end,
							.color = color_for_semantic(it->second),
						});
					}
				}
				break;
			}
		}
	}

	return spans;
}

auto gse::ide::syntax_producer::semantic_data::kind_at(const gui::buffer_position position) const -> const analysis::semantic_kind* {
	const auto it = at.find(position);
	return it == at.end() ? nullptr : &it->second;
}

auto gse::ide::syntax_producer::data::current_spans(const document_revision revision) const -> std::span<const gui::text_span> {
	return spans_revision == revision ? std::span<const gui::text_span>(spans) : std::span<const gui::text_span>{};
}

auto gse::ide::syntax_producer::data::current_semantic(const document_revision revision) const -> const semantic_data* {
	return semantic && semantic->revision == revision ? semantic.get() : nullptr;
}

auto gse::ide::syntax_producer::rebuild(data& d, const gui::text_buffer& buffer, const document_revision revision) -> void {
	auto job = std::make_shared<highlight_job>();
	job->revision = revision;

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

	std::shared_ptr<const semantic_data> sem = d.semantic && d.semantic->revision == revision
		? d.semantic
		: nullptr;
	d.pending = job;

	std::thread([job, snapshot = std::move(snapshot), sem] {
		job->spans = syntax::compute(snapshot, sem.get(), nullptr);
		job->done.store(true, std::memory_order_release);
	}).detach();
}

auto gse::ide::syntax_producer::poll(data& d, const document_revision revision) -> void {
	if (d.pending && d.pending->done.load(std::memory_order_acquire)) {
		if (d.pending->revision == revision) {
			d.spans = std::move(d.pending->spans);
			d.spans_revision = revision;
		}
		d.pending.reset();
	}
}

auto gse::ide::syntax_producer::highlight(const std::string_view source, const semantic_data* sem, const name_kinds* names) -> std::vector<gui::text_span> {
	return syntax::compute(source, sem, names);
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

auto gse::ide::syntax_producer::set_semantic(data& d, const semantic_input& input) -> void {
	auto sem = std::make_shared<semantic_data>();
	sem->revision = input.revision;

	constexpr std::size_t detail_limit = 12;
	std::size_t dropped = 0;

	for (const analysis::semantic_token& t : input.tokens) {
		std::string_view source;
		if (const std::string_view reason = syntax::drop_reason(t, input.buffer, source); !reason.empty()) {
			if (dropped < detail_limit) {
				syntax::log_dropped_token(input.source_path, t, reason, source);
			}
			++dropped;
			continue;
		}

		const std::uint32_t line = t.line - 1;
		const std::uint32_t column = t.column - 1;
		const gui::buffer_position position = {
			.line = line,
			.column = column,
		};
		const bool member_access = syntax::follows_member_access(source, column);
		if (const auto it = sem->at.find(position); it == sem->at.end()
			|| syntax::semantic_precedence(t.kind, member_access) >= syntax::semantic_precedence(it->second, member_access)) {
			sem->at[position] = t.kind;
		}
	}

	if (dropped > detail_limit) {
		log::println(
			log::level::warning,
			log::category::general,
			"[semantic] {} of {} highlight tokens dropped for {}, {} listed above",
			dropped,
			input.tokens.size(),
			input.source_path,
			detail_limit
		);
	}

	d.semantic = std::move(sem);
}

auto gse::ide::syntax_producer::clear_semantic(data& d) -> void {
	d.semantic.reset();
}
