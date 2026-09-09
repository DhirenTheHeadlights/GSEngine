module gse.ide.search:agent_query_impl;

import gse;
import gse.ide.analysis;
import gse.ide.build;
import gse.ide.config;
import std;

import :agent_query;
import :index;
import :types;

auto gse::ide::search::owns_query(const build_inbox::symbol_query& query) -> bool {
	if (!query.cwd.empty() && !config::owning_worktree(query.cwd)) {
		return false;
	}
	if (query.project.empty()) {
		return true;
	}
	std::error_code ec;
	const bool same_project = std::filesystem::equivalent(query.project, config::project_root(), ec);
	return !ec && same_project;
}

auto gse::ide::search::query_file(const build_inbox::symbol_query& query) -> std::filesystem::path {
	if (query.file.is_absolute()) {
		return query.file;
	}
	for (const std::filesystem::path& base : { query.cwd, config::project_root(), config::engine_root() }) {
		if (base.empty()) {
			continue;
		}
		std::error_code ec;
		const std::filesystem::path candidate = base / query.file;
		if (std::filesystem::exists(candidate, ec)) {
			return candidate;
		}
	}
	return query.cwd / query.file;
}

auto gse::ide::search::split_qualifier(const std::string_view name) -> std::pair<std::string, std::string> {
	const std::size_t colons = name.rfind("::");
	if (colons == std::string_view::npos) {
		return { std::string(), std::string(name) };
	}
	return { std::string(name.substr(0, colons + 2)), std::string(name.substr(colons + 2)) };
}

auto gse::ide::search::is_outline_kind(const analysis::symbol_kind kind) -> bool {
	return kind != analysis::symbol_kind::parameter && kind != analysis::symbol_kind::variable;
}

auto gse::ide::search::named_sites(const index_state& index, const std::string_view name, const std::size_t limit) -> std::expected<query_sites, lookup_error> {
	const auto [qualifier, leaf] = split_qualifier(name);
	const std::string needle = qualifier + leaf;
	const std::string suffix = "::" + needle;

	std::shared_lock _(index.mutex);
	if (!index.symbols_ready.load(std::memory_order_acquire)) {
		return unexpected_lookup(lookup_failure::index_building, std::string(name));
	}
	const auto candidates = index.symbols.symbols_by_name.find(leaf);
	if (candidates == index.symbols.symbols_by_name.end()) {
		return unexpected_lookup(lookup_failure::symbol_not_found, std::string(name));
	}

	std::vector<ranked_symbol> ranked;
	for (const std::uint32_t id : candidates->second) {
		const symbol_entry& candidate = index.symbols.symbols[id];
		if (const std::optional<int> score = selection_score(candidate, needle, suffix, !qualifier.empty(), std::nullopt, symbol_selection_mode::definition)) {
			ranked.push_back({ .score = *score, .symbol = &candidate });
		}
	}
	if (ranked.empty()) {
		return unexpected_lookup(qualifier.empty() ? lookup_failure::definition_not_found : lookup_failure::qualified_symbol_not_found, std::string(name));
	}
	std::ranges::stable_sort(ranked, std::ranges::greater{}, &ranked_symbol::score);

	query_sites out;
	out.total = ranked.size();
	for (const ranked_symbol& match : ranked) {
		if (out.shown.size() >= limit) {
			break;
		}
		query_site site{
			.path = index.symbols.path_for(match.symbol->file),
			.qualified = match.symbol->qualified,
			.kind = match.symbol->kind,
			.line = match.symbol->line,
			.column = match.symbol->column,
			.is_definition = match.symbol->is_definition,
		};
		if (const xref_entry* reference = xref_at(index.symbols, match.symbol->file, match.symbol->line, match.symbol->column)) {
			site.type = reference->type;
		}
		out.shown.push_back(std::move(site));
	}
	return out;
}

auto gse::ide::search::outline_sites(const index_state& index, const std::filesystem::path& file, const std::size_t limit) -> std::expected<query_sites, lookup_error> {
	const file_id identity = canonical_path_id(file).second;

	std::shared_lock _(index.mutex);
	if (!index.symbols_ready.load(std::memory_order_acquire)) {
		return unexpected_lookup(lookup_failure::index_building, file.generic_display_string());
	}
	const auto found = index.symbols.symbols_by_file.find(identity);
	if (found == index.symbols.symbols_by_file.end()) {
		return unexpected_lookup(lookup_failure::file_not_indexed, file.generic_display_string());
	}

	query_sites out;
	for (const std::uint32_t id : found->second) {
		const symbol_entry& symbol = index.symbols.symbols[id];
		if (!is_outline_kind(symbol.kind)) {
			continue;
		}
		out.total += 1;
		if (out.shown.size() >= limit) {
			continue;
		}
		out.shown.push_back({
			.path = file,
			.qualified = symbol.qualified,
			.kind = symbol.kind,
			.line = symbol.line,
			.column = symbol.column,
			.is_definition = symbol.is_definition,
		});
	}
	return out;
}

auto gse::ide::search::indexed_source(const index_state& index, const std::filesystem::path& path) -> std::shared_ptr<const content_entry> {
	const file_id identity = canonical_path_id(path).second;
	std::shared_lock _(index.mutex);
	if (const std::shared_ptr<content_entry>* entry = index.content.entries.try_get(identity)) {
		return *entry;
	}
	return {};
}

auto gse::ide::search::definition_extent(const content_entry& source, const std::uint32_t first, const std::uint32_t limit) -> source_extent {
	const std::uint32_t end_of_file = static_cast<std::uint32_t>(source.line_starts.size() - 1);
	const std::uint32_t last = std::min(end_of_file, first + limit - 1);
	const std::string_view opening = line_at(source.blob, source.line_starts, first);
	const std::size_t indent = opening.find_first_not_of(" \t");
	const std::string_view prefix = opening.substr(0, indent == std::string_view::npos ? 0 : indent);
	const std::size_t open_brace = opening.find('{');

	if (open_brace == std::string_view::npos) {
		if (opening.ends_with(';')) {
			return { .last = first, .complete = true };
		}
	}
	else if (const std::size_t close_brace = opening.rfind('}'); close_brace != std::string_view::npos && close_brace > open_brace) {
		return { .last = first, .complete = true };
	}

	bool braced = open_brace != std::string_view::npos;
	for (std::uint32_t line = first + 1; line <= last; ++line) {
		const std::string_view text = line_at(source.blob, source.line_starts, line);
		if (braced) {
			if (text.starts_with(prefix) && text.substr(prefix.size()).starts_with('}')) {
				return { .last = line, .complete = true };
			}
		}
		else if (text.ends_with(';')) {
			return { .last = line, .complete = true };
		}
		braced = braced || text.contains('{');
	}
	return { .last = last, .complete = last == end_of_file };
}

auto gse::ide::search::answer_query(const index_state& index, const build_inbox::symbol_query& query) -> std::vector<std::string> {
	constexpr std::uint32_t most_sites = 200;
	constexpr std::uint32_t most_lines = 2000;

	const std::size_t site_limit = query.sites > 0 ? std::min(query.sites, most_sites) : 20;
	const std::uint32_t line_budget = query.lines > 0 ? std::min(query.lines, most_lines) : 120;
	const bool outline = query.name.empty();

	const std::expected<query_sites, lookup_error> found = outline
		? outline_sites(index, query_file(query), site_limit)
		: named_sites(index, query.name, site_limit);
	if (!found) {
		return {
			std::format("error {}", describe(found.error())),
			std::format("indexing {}", is_pending(found.error()) ? 1 : 0),
		};
	}

	std::vector<std::string> out;
	out.push_back(std::format("mode {}", outline ? "file" : "name"));
	out.push_back(std::format("sites {} {}", found->shown.size(), found->total));

	std::uint32_t remaining = outline || !query.body ? 0 : line_budget;
	for (const auto [position, site] : std::views::enumerate(found->shown)) {
		out.push_back(std::format("site {} {} {} {} {} {}", position, site.kind, site.is_definition ? "definition" : "declaration", site.line + 1, site.column + 1, site.path.generic_display_string()));
		if (!site.qualified.empty()) {
			out.push_back(std::format("qualified {} {}", position, site.qualified));
		}
		if (!site.type.empty()) {
			out.push_back(std::format("type {} {}", position, site.type));
		}
		if (remaining == 0) {
			continue;
		}
		const std::shared_ptr<const content_entry> source = indexed_source(index, site.path);
		if (!source || source->line_starts.empty() || site.line >= source->line_starts.size()) {
			continue;
		}
		const source_extent extent = definition_extent(*source, site.line, remaining);
		out.push_back(std::format("body {} {} {} {}", position, site.line + 1, extent.last + 1, extent.complete ? "complete" : "truncated"));
		for (std::uint32_t line = site.line; line <= extent.last; ++line) {
			out.push_back(std::format("| {}", line_at(source->blob, source->line_starts, line)));
		}
		remaining -= std::min(remaining, extent.last - site.line + 1);
	}
	return out;
}

auto gse::ide::search::poll_agent_queries(const index_state& index) -> void {
	for (const build_inbox::symbol_query& query : build_inbox::peek_symbol_queries()) {
		if (!owns_query(query)) {
			continue;
		}
		build_inbox::consume_symbol_query(query.id);
		build_inbox::publish({
			.id = query.id,
			.outcome = build_inbox::status::ok,
			.lines = answer_query(index, query),
		});
	}
}
