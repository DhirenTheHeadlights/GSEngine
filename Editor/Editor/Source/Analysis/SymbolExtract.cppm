export module gse.ide.analysis:symbol_extract;

import std;
import gse;

import :semantic_tokens;

export namespace gse::ide::analysis {
	struct definition_symbol_kind {};
	struct type_symbol_kind {};

	enum class symbol_kind {
		variable [[= semantic_kind::variable]],
		parameter [[= semantic_kind::parameter]],
		function [[= definition_symbol_kind{}, = semantic_kind::function]],
		member [[= semantic_kind::member]],
		type [[= definition_symbol_kind{}, = type_symbol_kind{}, = semantic_kind::type]],
		enum_member [[= semantic_kind::enum_member]],
		name_space [[= semantic_kind::name_space]],
		enumeration [[= definition_symbol_kind{}, = type_symbol_kind{}, = semantic_kind::type]],
		alias [[= semantic_kind::type]],
		concept_decl [[= definition_symbol_kind{}, = type_symbol_kind{}, = semantic_kind::type]]
	};

	[[nodiscard]] constexpr auto is_definition_kind(
		symbol_kind kind
	) -> bool;

	[[nodiscard]] constexpr auto is_type_kind(
		symbol_kind kind
	) -> bool;

	[[nodiscard]] constexpr auto to_semantic_kind(
		symbol_kind kind
	) -> semantic_kind;

	struct symbol_token {
		std::string name;
		symbol_kind kind = symbol_kind::type;
		std::string file;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::string qualified;
		std::string identity;
		bool is_definition = true;
	};

	struct symbol_ref {
		std::string file;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::uint32_t length = 0;
		std::string name;
		std::string def_file;
		std::uint32_t def_line = 0;
		std::uint32_t def_column = 0;
		std::string qualified;
		std::string identity;
		std::string type;
		std::string value;
	};

	struct qualified_use {
		std::string file;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::uint32_t end_line = 0;
		std::uint32_t end_column = 0;
	};

	struct param_token {
		std::string file;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::uint32_t length = 0;
		semantic_kind kind = semantic_kind::variable;
	};

	struct symbol_set {
		std::vector<symbol_token> symbols;
		std::vector<symbol_ref> refs;
		std::vector<qualified_use> quals;
		std::vector<qualified_use> template_args;
		std::vector<param_token> params;
		bool complete = false;
	};

	struct symbol_tokens {
		static auto kind_from(std::string_view name) -> std::optional<symbol_kind>;
		static auto parse(std::string_view text, std::string_view main_file) -> symbol_set;
	};
}

constexpr auto gse::ide::analysis::is_definition_kind(const symbol_kind kind) -> bool {
	return gse::enum_has_annotation<definition_symbol_kind>(kind);
}

constexpr auto gse::ide::analysis::is_type_kind(const symbol_kind kind) -> bool {
	return gse::enum_has_annotation<type_symbol_kind>(kind);
}

constexpr auto gse::ide::analysis::to_semantic_kind(const symbol_kind kind) -> semantic_kind {
	return gse::annotation_from_enum<semantic_kind>(kind, semantic_kind::variable);
}

namespace gse::ide::analysis {
	auto report_discarded_record(const std::string_view main_file, const std::string_view line, const std::string_view reason, const std::size_t discarded) -> void {
		if (discarded > 8) {
			return;
		}
		log::println(log::level::warning, log::category::general, "[semantic] {}: {}: |{}|", main_file, reason, line);
	}
}

auto gse::ide::analysis::symbol_tokens::kind_from(std::string_view name) -> std::optional<symbol_kind> {
	symbol_kind kind;
	if (gse::enum_from_string(name, kind)) {
		return kind;
	}
	return std::nullopt;
}

auto gse::ide::analysis::symbol_tokens::parse(std::string_view text, std::string_view main_file) -> symbol_set {
	symbol_set out;

	std::size_t discarded = 0;

	std::size_t pos = 0;
	while (const std::optional<std::string_view> record = gse::next_line(text, pos)) {
		const std::string_view line = *record;

		if (line == "GSEDONE") {
			out.complete = true;
			continue;
		}

		if (line.starts_with("GSETOK")) {
			std::array<std::string_view, 5> fields;
			const std::size_t count = gse::split_fields(line, '\t', fields);
			if (count < 5) {
				++discarded;
				report_discarded_record(main_file, line, "a GSETOK record carries fewer than the 5 required fields", discarded);
				continue;
			}
			std::uint32_t ln = 0;
			std::uint32_t col = 0;
			std::uint32_t len = 0;
			const std::optional<semantic_kind> kind = semantic_tokens::kind_from(fields[4]);
			if (gse::parse(fields[1], ln) && gse::parse(fields[2], col) && gse::parse(fields[3], len) && kind) {
				out.params.push_back({
					.file = std::string(main_file),
					.line = ln,
					.column = col,
					.length = len,
					.kind = *kind,
				});
				continue;
			}
			++discarded;
			report_discarded_record(main_file, line, "a GSETOK record has a non-numeric position or an unknown kind, so the token will not be coloured", discarded);
			continue;
		}

		if (line.starts_with("GSESYM\t")) {
			std::array<std::string_view, 9> fields;
			const std::size_t count = gse::split_fields(line, '\t', fields);
			if (count < 6) {
				++discarded;
				report_discarded_record(main_file, line, "a GSESYM record carries fewer than the 6 required fields", discarded);
				continue;
			}
			const std::optional<symbol_kind> kind = kind_from(fields[2]);
			std::uint32_t ln = 0;
			std::uint32_t col = 0;
			if (!fields[1].empty() && kind && gse::parse(fields[4], ln) && gse::parse(fields[5], col)) {
				out.symbols.push_back({
					.name = std::string(fields[1]),
					.kind = *kind,
					.file = std::string(fields[3]),
					.line = ln,
					.column = col,
					.qualified = count >= 7 ? std::string(fields[6]) : std::string{},
					.identity = count >= 8 ? std::string(fields[7]) : std::string{},
					.is_definition = count >= 9 ? fields[8] == "definition" : true,
				});
				continue;
			}
			++discarded;
			report_discarded_record(main_file, line, "a GSESYM record has an empty name, an unknown kind, or a non-numeric position, so the symbol will be missing from the index", discarded);
			continue;
		}

		if (line.starts_with("GSEQUAL\t")) {
			std::array<std::string_view, 5> fields;
			const std::size_t count = gse::split_fields(line, '\t', fields);
			if (count < 5) {
				++discarded;
				report_discarded_record(main_file, line, "a GSEQUAL record carries fewer than the 5 required fields", discarded);
				continue;
			}
			std::uint32_t ln = 0;
			std::uint32_t col = 0;
			std::uint32_t len = 0;
			if (gse::parse(fields[2], ln) && gse::parse(fields[3], col) && gse::parse(fields[4], len)) {
				out.quals.push_back({
					.file = std::string(fields[1]),
					.line = ln,
					.column = col,
					.end_line = ln,
					.end_column = col + len,
				});
				continue;
			}
			++discarded;
			report_discarded_record(main_file, line, "a GSEQUAL record has a non-numeric position", discarded);
			continue;
		}

		if (line.starts_with("GSETARG\t")) {
			std::array<std::string_view, 6> fields;
			const std::size_t count = gse::split_fields(line, '\t', fields);
			if (count < 6) {
				++discarded;
				report_discarded_record(main_file, line, "a GSETARG record carries fewer than the 6 required fields", discarded);
				continue;
			}
			std::uint32_t ln = 0;
			std::uint32_t col = 0;
			std::uint32_t end_ln = 0;
			std::uint32_t end_col = 0;
			if (gse::parse(fields[2], ln) && gse::parse(fields[3], col) && gse::parse(fields[4], end_ln) && gse::parse(fields[5], end_col)) {
				out.template_args.push_back({
					.file = std::string(fields[1]),
					.line = ln,
					.column = col,
					.end_line = end_ln,
					.end_column = end_col,
				});
				continue;
			}
			++discarded;
			report_discarded_record(main_file, line, "a GSETARG record has a non-numeric position", discarded);
			continue;
		}

		if (line.starts_with("GSEREF\t")) {
			std::array<std::string_view, 13> fields;
			const std::size_t count = gse::split_fields(line, '\t', fields);
			if (count < 9) {
				++discarded;
				report_discarded_record(main_file, line, "a GSEREF record carries fewer than the 9 required fields, so this reference will not resolve", discarded);
				continue;
			}
			std::uint32_t ln = 0;
			std::uint32_t col = 0;
			std::uint32_t len = 0;
			std::uint32_t dln = 0;
			std::uint32_t dcol = 0;
			if (!fields[6].empty() && gse::parse(fields[2], ln) && gse::parse(fields[3], col) && gse::parse(fields[4], len) && gse::parse(fields[7], dln) && gse::parse(fields[8], dcol)) {
				out.refs.push_back({
					.file = std::string(fields[1]),
					.line = ln,
					.column = col,
					.length = len,
					.name = std::string(fields[5]),
					.def_file = std::string(fields[6]),
					.def_line = dln,
					.def_column = dcol,
					.qualified = count >= 10 ? std::string(fields[9]) : std::string{},
					.identity = count >= 11 ? std::string(fields[10]) : std::string{},
					.type = count >= 12 ? std::string(fields[11]) : std::string{},
					.value = count >= 13 ? std::string(fields[12]) : std::string{},
				});
				continue;
			}
			++discarded;
			report_discarded_record(main_file, line, "a GSEREF record has a non-numeric position or an empty definition file, so this reference will not resolve", discarded);
			continue;
		}
	}

	if (discarded > 8) {
		log::println(log::level::warning, log::category::general, "[semantic] {}: {} plugin records discarded, 8 listed above", main_file, discarded);
	}

	return out;
}
