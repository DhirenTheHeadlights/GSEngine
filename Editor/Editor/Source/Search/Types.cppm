export module gse.ide.search:types;

import std;

import gse;

import gse.ide.analysis;
import gse.ide.navigation;

export namespace gse::ide::search {
	struct domain_info {
		int priority = 0;
	};

	enum class domain {
		content [[= domain_info{
			.priority = 2,
		}]],
		symbol [[= domain_info{
			.priority = 0,
		}]],
		file [[= domain_info{
			.priority = 1,
		}]]
	};

	using file_id = id;

	struct location {
		std::filesystem::path path;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
	};

	struct match_range {
		std::uint32_t start = 0;
		std::uint32_t length = 0;
	};

	struct result {
		domain source = domain::file;
		float score = 0.f;
		std::filesystem::path path;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::uint32_t length = 0;
		std::string display;
		std::string detail;
		std::vector<match_range> highlight;
	};

	struct file_entry {
		std::filesystem::path path;
		std::string rel;
		std::string rel_lower;
	};

	struct symbol_entry {
		std::string name;
		std::string name_lower;
		analysis::symbol_kind kind = analysis::symbol_kind::type;
		file_id file{};
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::string qualified;
		std::string identity;
		bool is_definition = true;
	};

	struct xref_entry {
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::uint32_t length = 0;
		file_id def_file{};
		std::uint32_t def_line = 0;
		std::uint32_t def_column = 0;
		id qualified{};
		id identity{};
		std::string type;
		std::string value;
	};

	struct options {
		std::uint32_t max_results = 200;
		bool include_content = true;
		bool include_symbols = true;
		bool include_files = true;
	};

	auto to_lower(
		std::string_view text
	) -> std::string;

	auto jump_target(
		const result& match
	) -> jump_to_request;
}

auto gse::ide::search::jump_target(const result& match) -> jump_to_request {
	return {
		.path = match.path,
		.line = match.line,
		.column = match.column,
		.end_line = match.line,
		.end_column = match.column + match.length,
		.highlight = match.source == domain::file ? jump_highlight::caret : jump_highlight::span,
	};
}

auto gse::ide::search::to_lower(const std::string_view text) -> std::string {
	std::string out;
	out.reserve(text.size());
	for (const char c : text) {
		out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
	}
	return out;
}
