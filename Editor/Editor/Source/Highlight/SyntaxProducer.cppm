export module gse.ide.highlight:syntax_producer;

import gse.analysis;
import gse.concurrency;
import gse.core;
import gse.graphics;
import gse.ide.diagnostic;
import gse.math;
import gse.meta;
import gse.syntax;
import std;

import :language;
import :markdown;

export namespace gse::ide {
	struct syntax_producer {
		struct semantic_data {
			gse::analysis::semantic_index index;
			document_revision revision;
		};

		struct highlight_result {
			std::vector<gui::text_span> spans;
			document_revision revision;
		};

		struct data {
			std::vector<gui::text_span> spans;
			document_revision spans_revision;
			std::shared_ptr<const semantic_data> semantic;
			task::pending<highlight_result> pending;

			auto current_spans(
				document_revision revision
			) const -> std::span<const gui::text_span>;

			auto current_semantic(
				document_revision revision
			) const -> const gse::analysis::semantic_index*;
		};

		struct semantic_input {
			std::span<const gse::analysis::semantic_token> tokens;
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
			const gse::analysis::semantic_index* index
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

	auto color_for(
		gse::analysis::semantic_kind k
	) -> vec4f;
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

auto gse::ide::producer::color_for(const gse::analysis::semantic_kind k) -> vec4f {
	const gse::analysis::semantic_kind_info info = annotation_from_enum<gse::analysis::semantic_kind_info>(k, {
		.color = 0xc2cce0,
	});
	return rgb(info.color);
}

auto gse::ide::syntax_producer::data::current_spans(const document_revision revision) const -> std::span<const gui::text_span> {
	return spans_revision == revision ? std::span<const gui::text_span>(spans) : std::span<const gui::text_span>{};
}

auto gse::ide::syntax_producer::data::current_semantic(const document_revision revision) const -> const gse::analysis::semantic_index* {
	return semantic && semantic->revision == revision ? &semantic->index : nullptr;
}

auto gse::ide::syntax_producer::rebuild(data& d, const gui::text_buffer& buffer, const document_revision revision, const document_language language) -> void {
	std::shared_ptr<const semantic_data> sem = d.semantic && d.semantic->revision == revision
		? d.semantic
		: nullptr;

	d.pending.start([snapshot = buffer.text(), sem = std::move(sem), revision, language] {
		return highlight_result{
			.spans = language == document_language::markdown
				? markdown::spans(snapshot)
				: highlight(snapshot, sem ? &sem->index : nullptr),
			.revision = revision,
		};
	}, trace_id<"highlight::spans">(), task::lane::io);
}

auto gse::ide::syntax_producer::poll(data& d, const document_revision revision) -> void {
	if (std::optional<highlight_result> finished = d.pending.take(); finished && finished->revision == revision) {
		d.spans = std::move(finished->spans);
		d.spans_revision = revision;
	}
}

auto gse::ide::syntax_producer::highlight(const std::string_view source, const gse::analysis::semantic_index* index) -> std::vector<gui::text_span> {
	const std::vector<gse::analysis::highlight_token> tokens = gse::analysis::highlight(source, index);
	std::vector<gui::text_span> spans;
	spans.reserve(tokens.size());
	for (const gse::analysis::highlight_token& t : tokens) {
		spans.push_back({
			.line = t.line,
			.start_col = t.start,
			.end_col = t.end,
			.color = std::visit([](const auto k) {
				return producer::color_for(k);
			}, t.k),
		});
	}
	return spans;
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
	d.semantic = std::make_shared<const semantic_data>(semantic_data{
		.index = gse::analysis::index_semantic({
			.tokens = input.tokens,
			.lines = input.buffer.lines,
			.path = input.source_path,
		}),
		.revision = input.revision,
	});
}

auto gse::ide::syntax_producer::clear_semantic(data& d) -> void {
	d.semantic.reset();
}
