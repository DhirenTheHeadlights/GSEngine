export module gse.ide.highlight:syntax_producer;

import std;
import gse;
import gse.ide.analysis;
import gse.ide.diagnostic;

import gse.syntax;

import :language;
import :markdown;

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
			document_revision revision,
			document_language language
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

namespace gse::ide::producer {
	auto rgb(
		std::uint32_t hex
	) -> vec4f;

	auto color_for(
		syntax::kind k
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

auto gse::ide::producer::rgb(const std::uint32_t hex) -> vec4f {
	return {
		static_cast<float>((hex >> 16) & 0xffu) / 255.f,
		static_cast<float>((hex >> 8) & 0xffu) / 255.f,
		static_cast<float>(hex & 0xffu) / 255.f,
		1.f
	};
}

auto gse::ide::producer::color_for(const syntax::kind k) -> vec4f {
	const syntax::kind_info info = annotation_from_enum<syntax::kind_info>(k, {
		.color = 0xc2cce0,
	});
	return rgb(info.color);
}

auto gse::ide::producer::color_for_semantic(const analysis::semantic_kind k) -> vec4f {
	const analysis::semantic_kind_info info = annotation_from_enum<analysis::semantic_kind_info>(k, {
		.color = 0xc2cce0,
	});
	return rgb(info.color);
}

auto gse::ide::producer::follows_member_access(const std::string_view source, const std::uint32_t column) -> bool {
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

auto gse::ide::producer::semantic_precedence(const analysis::semantic_kind k, const bool member_access) -> int {
	if (member_access && (k == analysis::semantic_kind::member || k == analysis::semantic_kind::function)) {
		return 5;
	}
	return annotation_from_enum<analysis::semantic_kind_info>(k, {}).precedence;
}

auto gse::ide::producer::log_dropped_token(const std::string_view path, const analysis::semantic_token& token, const std::string_view reason, const std::string_view row) -> void {
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

auto gse::ide::producer::drop_reason(const analysis::semantic_token& token, const gui::text_buffer& buffer, std::string_view& row) -> std::string_view {
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
	if (column > 0 && syntax::is_ident_char(row[column - 1])) {
		return "the token starts inside an identifier, so its column is misaligned";
	}
	if (column + token.length < row.size() && syntax::is_ident_char(row[column + token.length])) {
		return "the token ends inside an identifier, so its length is short";
	}
	return {};
}

auto gse::ide::producer::compute(const std::string_view source, const syntax_producer::semantic_data* sem, const syntax_producer::name_kinds* names) -> std::vector<gui::text_span> {
	std::vector<gui::text_span> spans;

	const std::vector<std::string_view> lines = syntax::split_lines(source);
	const syntax::lex_result lex = syntax::tokenize(lines);

	auto emit = [&](const syntax::token& t, const vec4f& color) {
		spans.push_back({
			.line = t.line,
			.start_col = t.start,
			.end_col = t.end,
			.color = color,
		});
	};

	for (const syntax::token& t : lex.tokens) {
		if (const std::optional<syntax::kind> k = syntax::classify_token(t)) {
			emit(t, color_for(*k));
			continue;
		}

		if (const analysis::semantic_kind* sk = sem ? sem->kind_at({
			.line = t.line,
			.column = t.start,
		}) : nullptr) {
			emit(t, color_for_semantic(*sk));
		}
		else if (names) {
			if (const auto it = names->find(t.text); it != names->end()) {
				emit(t, color_for_semantic(it->second));
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

auto gse::ide::syntax_producer::rebuild(data& d, const gui::text_buffer& buffer, const document_revision revision, const document_language language) -> void {
	auto job = std::make_shared<highlight_job>();
	job->revision = revision;

	std::string snapshot = buffer.text();

	std::shared_ptr<const semantic_data> sem = d.semantic && d.semantic->revision == revision
		? d.semantic
		: nullptr;
	d.pending = job;

	std::thread([job, snapshot = std::move(snapshot), sem, language] {
		job->spans = language == document_language::markdown
			? markdown::spans(snapshot)
			: producer::compute(snapshot, sem.get(), nullptr);
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
	return producer::compute(source, sem, names);
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
		if (const std::string_view reason = producer::drop_reason(t, input.buffer, source); !reason.empty()) {
			if (dropped < detail_limit) {
				producer::log_dropped_token(input.source_path, t, reason, source);
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
		const bool member_access = producer::follows_member_access(source, column);
		if (const auto it = sem->at.find(position); it == sem->at.end()
			|| producer::semantic_precedence(t.kind, member_access) >= producer::semantic_precedence(it->second, member_access)) {
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
