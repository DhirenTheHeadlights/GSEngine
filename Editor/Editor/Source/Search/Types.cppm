export module gse.ide.search:types;

import std;

import gse;

import gse.ide.analysis;

export namespace gse::ide::search {
	enum class domain {
		content,
		symbol,
		file
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
		std::string qualified;
		std::string identity;
		std::string type;
		std::string value;
	};

	struct options {
		std::uint32_t max_results = 200;
		bool include_content = true;
		bool include_symbols = true;
		bool include_files = true;
	};

	auto to_lower(std::string_view text) -> std::string;
}

auto gse::ide::search::to_lower(const std::string_view text) -> std::string {
	std::string out;
	out.reserve(text.size());
	for (const char c : text) {
		out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
	}
	return out;
}
