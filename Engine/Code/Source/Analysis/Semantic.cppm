export module gse.analysis:semantic;

import std;

import gse.log;
import gse.meta;

export namespace gse::analysis {
	struct semantic_kind_info {
		std::uint32_t color = 0;
		int precedence = 0;
	};

	enum class semantic_kind {
		variable [[= semantic_kind_info{
			.color = 0x9ca5b8,
			.precedence = 1,
		}]],
		parameter [[= semantic_kind_info{
			.color = 0x4b5b7e,
			.precedence = 2,
		}]],
		function [[= semantic_kind_info{
			.color = 0xd57192,
			.precedence = 3,
		}]],
		member [[= semantic_kind_info{
			.color = 0xd6794d,
			.precedence = 3,
		}]],
		type [[= semantic_kind_info{
			.color = 0x2aa1d9,
			.precedence = 4,
		}]],
		enum_member [[= semantic_kind_info{
			.color = 0x7aa549,
			.precedence = 3,
		}]],
		name_space [[= semantic_kind_info{
			.color = 0x9ca5b8,
			.precedence = 4,
		}]],
		global [[= semantic_kind_info{
			.color = 0xb4911b,
			.precedence = 3,
		}]],
		attribute [[= semantic_kind_info{
			.color = 0x2a7195,
			.precedence = 3,
		}]]
	};

	struct semantic_token {
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::uint32_t length = 0;
		semantic_kind kind = semantic_kind::variable;
	};

	auto semantic_kind_from(
		std::string_view name
	) -> std::optional<semantic_kind>;

	auto parse_semantic_tokens(
		std::string_view text
	) -> std::vector<semantic_token>;
}

auto gse::analysis::semantic_kind_from(const std::string_view name) -> std::optional<semantic_kind> {
	semantic_kind kind;
	if (enum_from_string(name, kind)) {
		return kind;
	}
	return std::nullopt;
}

auto gse::analysis::parse_semantic_tokens(const std::string_view text) -> std::vector<semantic_token> {
	std::vector<semantic_token> out;

	std::size_t position = 0;
	while (const std::optional<std::string_view> line = next_line(text, position)) {
		if (!line->starts_with("GSETOK\t")) {
			continue;
		}

		std::array<std::string_view, 5> fields;
		const std::size_t count = split_fields(*line, '\t', fields);
		if (count < 5) {
			log::println(log::level::warning, log::category::general, "[semantic] GSETOK record has {} fields, expected 5: |{}|", count, *line);
			continue;
		}

		std::uint32_t ln = 0;
		std::uint32_t col = 0;
		std::uint32_t len = 0;
		const bool has_line = gse::parse(fields[1], ln);
		const bool has_column = gse::parse(fields[2], col);
		const bool has_length = gse::parse(fields[3], len);
		const std::optional<semantic_kind> kind = semantic_kind_from(fields[4]);
		if (has_line && has_column && has_length && kind) {
			out.push_back({
				.line = ln,
				.column = col,
				.length = len,
				.kind = *kind,
			});
			continue;
		}
		log::println(
			log::level::warning,
			log::category::general,
			"[semantic] GSETOK record discarded, its {} field is not understood: |{}|",
			!has_line ? "line" : (!has_column ? "column" : (!has_length ? "length" : "kind")),
			*line
		);
	}
	return out;
}
