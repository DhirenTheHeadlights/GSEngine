export module gse.analysis:highlight;

import std;

import gse.log;
import gse.meta;
import gse.syntax;

import :semantic;

export namespace gse::analysis {
	struct position {
		std::uint32_t line = 0;
		std::uint32_t column = 0;

		friend auto operator<=>(
			const position&,
			const position&
		) = default;
	};

	struct semantic_index {
		std::unordered_map<position, semantic_kind> at;

		auto kind_at(
			position p
		) const -> const semantic_kind*;
	};

	struct highlight_token {
		std::uint32_t line = 0;
		std::uint32_t start = 0;
		std::uint32_t end = 0;
		std::variant<syntax::kind, semantic_kind> k;
	};

	struct semantic_source {
		std::span<const semantic_token> tokens;
		std::span<const std::string> lines;
		std::string_view path;
	};

	auto index_semantic(
		const semantic_source& source
	) -> semantic_index;

	auto highlight(
		std::string_view source,
		const semantic_index* index
	) -> std::vector<highlight_token>;
}

namespace gse::analysis {
	auto follows_member_access(
		std::string_view row,
		std::uint32_t column
	) -> bool;

	auto semantic_precedence(
		semantic_kind k,
		bool member_access
	) -> int;

	auto log_dropped_token(
		std::string_view path,
		const semantic_token& token,
		std::string_view reason,
		std::string_view row
	) -> void;

	auto drop_reason(
		const semantic_token& token,
		std::span<const std::string> lines,
		std::string_view& row
	) -> std::string_view;
}

auto gse::analysis::semantic_index::kind_at(const position p) const -> const semantic_kind* {
	const auto it = at.find(p);
	return it == at.end() ? nullptr : &it->second;
}

auto gse::analysis::follows_member_access(const std::string_view row, const std::uint32_t column) -> bool {
	std::size_t pos = std::min<std::size_t>(column, row.size());
	while (pos > 0 && std::isspace(static_cast<unsigned char>(row[pos - 1]))) {
		--pos;
	}
	if (pos > 0 && row[pos - 1] == '.') {
		return true;
	}
	if (pos == 0 || row[pos - 1] != '>') {
		return false;
	}
	--pos;
	while (pos > 0 && std::isspace(static_cast<unsigned char>(row[pos - 1]))) {
		--pos;
	}
	return pos > 0 && row[pos - 1] == '-';
}

auto gse::analysis::semantic_precedence(const semantic_kind k, const bool member_access) -> int {
	if (member_access && (k == semantic_kind::member || k == semantic_kind::function)) {
		return 5;
	}
	return annotation_from_enum<semantic_kind_info>(k, {}).precedence;
}

auto gse::analysis::log_dropped_token(const std::string_view path, const semantic_token& token, const std::string_view reason, const std::string_view row) -> void {
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

auto gse::analysis::drop_reason(const semantic_token& token, const std::span<const std::string> lines, std::string_view& row) -> std::string_view {
	if (token.line == 0 || token.column == 0 || token.length == 0) {
		return "the compiler emitted a zero line, column, or length";
	}
	const std::uint32_t line = token.line - 1;
	if (line >= lines.size()) {
		return "the line is past the end of the buffer, so the index is stale against the open document";
	}
	row = lines[line];
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

auto gse::analysis::index_semantic(const semantic_source& source) -> semantic_index {
	semantic_index index;

	constexpr std::size_t detail_limit = 12;
	std::size_t dropped = 0;

	for (const semantic_token& t : source.tokens) {
		std::string_view row;
		if (const std::string_view reason = drop_reason(t, source.lines, row); !reason.empty()) {
			if (dropped < detail_limit) {
				log_dropped_token(source.path, t, reason, row);
			}
			++dropped;
			continue;
		}

		const position at = {
			.line = t.line - 1,
			.column = t.column - 1,
		};
		const bool member_access = follows_member_access(row, at.column);
		if (const auto it = index.at.find(at); it == index.at.end() || semantic_precedence(t.kind, member_access) >= semantic_precedence(it->second, member_access)) {
			index.at[at] = t.kind;
		}
	}

	if (dropped > detail_limit) {
		log::println(
			log::level::warning,
			log::category::general,
			"[semantic] {} of {} highlight tokens dropped for {}, {} listed above",
			dropped,
			source.tokens.size(),
			source.path,
			detail_limit
		);
	}

	return index;
}

auto gse::analysis::highlight(const std::string_view source, const semantic_index* index) -> std::vector<highlight_token> {
	std::vector<highlight_token> out;

	const std::vector<std::string_view> lines = syntax::split_lines(source);
	const syntax::lex_result lex = syntax::tokenize(lines);

	for (const syntax::token& t : lex.tokens) {
		if (const std::optional<syntax::kind> k = syntax::classify_token(t)) {
			out.push_back({
				.line = t.line,
				.start = t.start,
				.end = t.end,
				.k = *k,
			});
			continue;
		}

		if (const semantic_kind* sk = index ? index->kind_at({
			.line = t.line,
			.column = t.start,
		}) : nullptr) {
			out.push_back({
				.line = t.line,
				.start = t.start,
				.end = t.end,
				.k = *sk,
			});
		}
	}

	return out;
}
