export module gse.ide.analysis:semantic_tokens;

import gse.log;
import gse.meta;
import std;

export namespace gse::ide::analysis {
	enum class identifier_context {
		block,
		requires_expression,
		lambda_body
	};

	struct identifier_use {
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::uint32_t length = 0;
		identifier_context context = identifier_context::block;
	};

	struct semantic_tokens {
		static auto parse_identifiers(
			std::string_view text
		) -> std::vector<identifier_use>;
	};
}

auto gse::ide::analysis::semantic_tokens::parse_identifiers(const std::string_view text) -> std::vector<identifier_use> {
	std::vector<identifier_use> out;
	std::size_t position = 0;
	while (const std::optional<std::string_view> line = next_line(text, position)) {
		if (!line->starts_with("GSEIDENT\t")) {
			continue;
		}
		std::array<std::string_view, 5> fields;
		if (split_fields(*line, '\t', fields) < 5) {
			log::println(log::level::warning, log::category::general, "[semantic] GSEIDENT record is malformed: |{}|", *line);
			continue;
		}
		std::uint32_t ln = 0;
		std::uint32_t col = 0;
		std::uint32_t len = 0;
		identifier_context context = identifier_context::block;
		if (!gse::parse(fields[1], ln) || !gse::parse(fields[2], col) || !gse::parse(fields[3], len) || !enum_from_string(fields[4], context)) {
			log::println(log::level::warning, log::category::general, "[semantic] GSEIDENT record is not understood: |{}|", *line);
			continue;
		}
		out.push_back({
			.line = ln,
			.column = col,
			.length = len,
			.context = context,
		});
	}
	return out;
}
