export module gse.ide.search:agent_query;

import std;
import gse;

import gse.ide.analysis;
import gse.ide.build;
import gse.ide.config;

import :index;
import :types;

export namespace gse::ide::search {
	auto poll_agent_queries(
		const index_state& index
	) -> void;
}

namespace gse::ide::search {
	struct query_site {
		std::filesystem::path path;
		std::string qualified;
		std::string type;
		analysis::symbol_kind kind = analysis::symbol_kind::type;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		bool is_definition = true;
	};

	struct query_sites {
		std::vector<query_site> shown;
		std::size_t total = 0;
	};

	struct ranked_symbol {
		int score = 0;
		const symbol_entry* symbol = nullptr;
	};

	struct source_extent {
		std::uint32_t last = 0;
		bool complete = false;
	};

	auto owns_query(
		const build_inbox::symbol_query& query
	) -> bool;

	auto query_file(
		const build_inbox::symbol_query& query
	) -> std::filesystem::path;

	auto split_qualifier(
		std::string_view name
	) -> std::pair<std::string, std::string>;

	auto is_outline_kind(
		analysis::symbol_kind kind
	) -> bool;

	auto named_sites(
		const index_state& index,
		std::string_view name,
		std::size_t limit
	) -> std::expected<query_sites, lookup_error>;

	auto outline_sites(
		const index_state& index,
		const std::filesystem::path& file,
		std::size_t limit
	) -> std::expected<query_sites, lookup_error>;

	auto indexed_source(
		const index_state& index,
		const std::filesystem::path& path
	) -> std::shared_ptr<const content_entry>;

	auto definition_extent(
		const content_entry& source,
		std::uint32_t first,
		std::uint32_t limit
	) -> source_extent;

	auto answer_query(
		const index_state& index,
		const build_inbox::symbol_query& query
	) -> std::vector<std::string>;
}
