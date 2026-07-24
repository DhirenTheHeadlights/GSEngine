export module gse.ide.analysis:semantic_tokens;

import std;
import gse;

export namespace gse::ide::analysis {
	enum class semantic_kind {
		variable,
		parameter,
		function,
		member,
		type,
		enum_member,
		name_space,
		global,
		attribute
	};

	struct semantic_token {
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::uint32_t length = 0;
		semantic_kind kind = semantic_kind::variable;
	};

	struct semantic_tokens {
		static auto kind_from(std::string_view name) -> std::optional<semantic_kind>;
		static auto parse(std::string_view text) -> std::vector<semantic_token>;
	};
}

auto gse::ide::analysis::semantic_tokens::kind_from(std::string_view name) -> std::optional<semantic_kind> {
	semantic_kind kind;
	if (gse::enum_from_string(name, kind)) {
		return kind;
	}
	return std::nullopt;
}

auto gse::ide::analysis::semantic_tokens::parse(std::string_view text) -> std::vector<semantic_token> {
	std::vector<semantic_token> out;

	auto to_u32 = [](std::string_view s) -> std::optional<std::uint32_t> {
		std::uint32_t value = 0;
		const auto result = std::from_chars(s.data(), s.data() + s.size(), value);
		if (result.ec != std::errc{}) {
			return std::nullopt;
		}
		return value;
	};

	std::size_t pos = 0;
	while (pos < text.size()) {
		const std::size_t eol = text.find('\n', pos);
		std::string_view line = text.substr(pos, (eol == std::string_view::npos ? text.size() : eol) - pos);
		pos = eol == std::string_view::npos ? text.size() : eol + 1;

		if (!line.empty() && line.back() == '\r') {
			line.remove_suffix(1);
		}

		if (!line.starts_with("GSETOK\t")) {
			continue;
		}

		std::array<std::string_view, 5> fields;
		std::size_t count = 0;
		std::size_t start = 0;
		for (std::size_t i = 0; i <= line.size() && count < fields.size(); ++i) {
			if (i == line.size() || line[i] == '\t') {
				fields[count++] = line.substr(start, i - start);
				start = i + 1;
			}
		}
		if (count < 5) {
			continue;
		}

		const std::optional<std::uint32_t> ln = to_u32(fields[1]);
		const std::optional<std::uint32_t> col = to_u32(fields[2]);
		const std::optional<std::uint32_t> len = to_u32(fields[3]);
		const std::optional<semantic_kind> kind = kind_from(fields[4]);
		if (ln && col && len && kind) {
			out.push_back({
				.line = *ln,
				.column = *col,
				.length = *len,
				.kind = *kind,
			});
		}
	}
	return out;
}
