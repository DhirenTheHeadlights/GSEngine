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
	};

	struct qualified_use {
		std::string file;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::uint32_t end_line = 0;
		std::uint32_t end_column = 0;
	};

	struct channel_use {
		bool produce = false;
		std::string system;
		std::string message;
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
		std::vector<channel_use> channels;
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

auto gse::ide::analysis::symbol_tokens::kind_from(std::string_view name) -> std::optional<symbol_kind> {
	symbol_kind kind;
	if (gse::enum_from_string(name, kind)) {
		return kind;
	}
	return std::nullopt;
}

auto gse::ide::analysis::symbol_tokens::parse(std::string_view text, std::string_view main_file) -> symbol_set {
	symbol_set out;

	auto to_u32 = [](std::string_view s) -> std::optional<std::uint32_t> {
		std::uint32_t value = 0;
		const auto result = std::from_chars(s.data(), s.data() + s.size(), value);
		if (result.ec != std::errc{}) {
			return std::nullopt;
		}
		return value;
	};

	auto split = [](std::string_view line, std::span<std::string_view> fields) -> std::size_t {
		std::size_t count = 0;
		std::size_t start = 0;
		for (std::size_t i = 0; i <= line.size() && count < fields.size(); ++i) {
			if (i == line.size() || line[i] == '\t') {
				fields[count++] = line.substr(start, i - start);
				start = i + 1;
			}
		}
		return count;
	};

	std::size_t pos = 0;
	while (pos < text.size()) {
		const std::size_t eol = text.find('\n', pos);
		std::string_view line = text.substr(pos, (eol == std::string_view::npos ? text.size() : eol) - pos);
		pos = eol == std::string_view::npos ? text.size() : eol + 1;

		if (!line.empty() && line.back() == '\r') {
			line.remove_suffix(1);
		}

		if (line == "GSEDONE") {
			out.complete = true;
			continue;
		}

		if (line.starts_with("GSETOK")) {
			std::array<std::string_view, 5> fields;
			const std::size_t count = split(line, fields);
			if (count < 5) {
				continue;
			}
			const std::optional<std::uint32_t> ln = to_u32(fields[1]);
			const std::optional<std::uint32_t> col = to_u32(fields[2]);
			const std::optional<std::uint32_t> len = to_u32(fields[3]);
			const std::optional<semantic_kind> kind = semantic_tokens::kind_from(fields[4]);
			if (ln && col && len && kind) {
				out.params.push_back({
					.file = std::string(main_file),
					.line = *ln,
					.column = *col,
					.length = *len,
					.kind = *kind,
				});
			}
			continue;
		}

		if (line.starts_with("GSESYM\t")) {
			std::array<std::string_view, 9> fields;
			const std::size_t count = split(line, fields);
			if (count < 6) {
				continue;
			}
			const std::optional<symbol_kind> kind = kind_from(fields[2]);
			const std::optional<std::uint32_t> ln = to_u32(fields[4]);
			const std::optional<std::uint32_t> col = to_u32(fields[5]);
			if (!fields[1].empty() && kind && ln && col) {
				out.symbols.push_back({
					.name = std::string(fields[1]),
					.kind = *kind,
					.file = std::string(fields[3]),
					.line = *ln,
					.column = *col,
					.qualified = count >= 7 ? std::string(fields[6]) : std::string{},
					.identity = count >= 8 ? std::string(fields[7]) : std::string{},
					.is_definition = count >= 9 ? fields[8] == "definition" : true,
				});
			}
			continue;
		}

		if (line.starts_with("GSEQUAL\t")) {
			std::array<std::string_view, 5> fields;
			const std::size_t count = split(line, fields);
			if (count < 5) {
				continue;
			}
			const std::optional<std::uint32_t> ln = to_u32(fields[2]);
			const std::optional<std::uint32_t> col = to_u32(fields[3]);
			const std::optional<std::uint32_t> len = to_u32(fields[4]);
			if (ln && col && len) {
				out.quals.push_back({
					.file = std::string(fields[1]),
					.line = *ln,
					.column = *col,
					.end_line = *ln,
					.end_column = *col + *len,
				});
			}
			continue;
		}

		if (line.starts_with("GSETARG\t")) {
			std::array<std::string_view, 6> fields;
			const std::size_t count = split(line, fields);
			if (count < 6) {
				continue;
			}
			const std::optional<std::uint32_t> ln = to_u32(fields[2]);
			const std::optional<std::uint32_t> col = to_u32(fields[3]);
			const std::optional<std::uint32_t> end_ln = to_u32(fields[4]);
			const std::optional<std::uint32_t> end_col = to_u32(fields[5]);
			if (ln && col && end_ln && end_col) {
				out.template_args.push_back({
					.file = std::string(fields[1]),
					.line = *ln,
					.column = *col,
					.end_line = *end_ln,
					.end_column = *end_col,
				});
			}
			continue;
		}

		if (line.starts_with("GSEREF\t")) {
			std::array<std::string_view, 12> fields;
			const std::size_t count = split(line, fields);
			if (count < 9) {
				continue;
			}
			const std::optional<std::uint32_t> ln = to_u32(fields[2]);
			const std::optional<std::uint32_t> col = to_u32(fields[3]);
			const std::optional<std::uint32_t> len = to_u32(fields[4]);
			const std::optional<std::uint32_t> dln = to_u32(fields[7]);
			const std::optional<std::uint32_t> dcol = to_u32(fields[8]);
			if (ln && col && len && dln && dcol && !fields[6].empty()) {
				out.refs.push_back({
					.file = std::string(fields[1]),
					.line = *ln,
					.column = *col,
					.length = *len,
					.name = std::string(fields[5]),
					.def_file = std::string(fields[6]),
					.def_line = *dln,
					.def_column = *dcol,
					.qualified = count >= 10 ? std::string(fields[9]) : std::string{},
					.identity = count >= 11 ? std::string(fields[10]) : std::string{},
					.type = count >= 12 ? std::string(fields[11]) : std::string{},
				});
			}
			continue;
		}

		if (line.starts_with("GSECHAN\t")) {
			std::array<std::string_view, 4> fields;
			const std::size_t count = split(line, fields);
			if (count >= 4 && !fields[2].empty() && !fields[3].empty()) {
				out.channels.push_back({
					.produce = fields[1] == "produce",
					.system = std::string(fields[2]),
					.message = std::string(fields[3]),
				});
			}
			continue;
		}
	}
	return out;
}
