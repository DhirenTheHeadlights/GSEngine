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

auto gse::ide::search::describe_role(const site_role role) -> std::string_view {
	switch (role) {
		case site_role::definition:
			return "definition";
		case site_role::declaration:
			return "declaration";
		case site_role::reference:
			return "reference";
	}
	return "declaration";
}

auto gse::ide::search::query_mode(const build_inbox::symbol_query& query) -> std::string_view {
	if (query.name.empty()) {
		return "file";
	}
	return query.refs ? "refs" : "name";
}

auto gse::ide::search::ranked_matches(const symbol_index& symbols, const std::string_view name) -> std::expected<std::vector<ranked_symbol>, lookup_failure> {
	const auto [qualifier, leaf] = split_qualifier(name);
	const std::string needle = qualifier + leaf;
	const std::string suffix = "::" + needle;

	const auto candidates = symbols.symbols_by_name.find(leaf);
	if (candidates == symbols.symbols_by_name.end()) {
		return std::unexpected(lookup_failure::symbol_not_found);
	}

	std::vector<ranked_symbol> ranked;
	for (const std::uint32_t id : candidates->second) {
		const symbol_entry& candidate = symbols.symbols[id];
		if (const std::optional<int> score = selection_score(candidate, needle, suffix, !qualifier.empty(), std::nullopt, symbol_selection_mode::definition)) {
			ranked.push_back({ .score = *score, .symbol = &candidate });
		}
	}
	if (ranked.empty()) {
		return std::unexpected(qualifier.empty() ? lookup_failure::definition_not_found : lookup_failure::qualified_symbol_not_found);
	}
	std::ranges::stable_sort(ranked, std::ranges::greater{}, &ranked_symbol::score);
	return ranked;
}

auto gse::ide::search::matching_anchor(const std::span<const ranked_symbol> anchors, const file_id file, const std::uint32_t line, const std::uint32_t column) -> const symbol_entry* {
	for (const ranked_symbol& anchor : anchors) {
		if (anchor.symbol->file == file && anchor.symbol->line == line && anchor.symbol->column == column) {
			return anchor.symbol;
		}
	}
	return nullptr;
}

auto gse::ide::search::named_sites(const index_state& index, const std::string_view name, const std::size_t limit) -> std::expected<query_sites, lookup_error> {
	std::shared_lock _(index.mutex);
	if (!index.symbols_ready.load(std::memory_order_acquire)) {
		return unexpected_lookup(lookup_failure::index_building, std::string(name));
	}
	const std::expected<std::vector<ranked_symbol>, lookup_failure> ranked = ranked_matches(index.symbols, name);
	if (!ranked) {
		return unexpected_lookup(ranked.error(), std::string(name));
	}

	query_sites out;
	out.total = ranked->size();
	for (const ranked_symbol& match : *ranked) {
		if (out.shown.size() >= limit) {
			break;
		}
		query_site site{
			.path = index.symbols.path_for(match.symbol->file),
			.qualified = match.symbol->qualified,
			.kind = match.symbol->kind,
			.line = match.symbol->line,
			.column = match.symbol->column,
			.role = match.symbol->is_definition ? site_role::definition : site_role::declaration,
		};
		if (const xref_entry* reference = xref_at(index.symbols, match.symbol->file, match.symbol->line, match.symbol->column)) {
			site.type = reference->type;
		}
		out.shown.push_back(std::move(site));
	}
	return out;
}

auto gse::ide::search::reference_sites(const index_state& index, const std::string_view name, const std::size_t limit) -> std::expected<query_sites, lookup_error> {
	std::shared_lock _(index.mutex);
	if (!index.symbols_ready.load(std::memory_order_acquire)) {
		return unexpected_lookup(lookup_failure::index_building, std::string(name));
	}
	const std::expected<std::vector<ranked_symbol>, lookup_failure> anchors = ranked_matches(index.symbols, name);
	if (!anchors) {
		return unexpected_lookup(anchors.error(), std::string(name));
	}

	std::vector<query_site> found;
	for (const auto& [file, refs] : index.symbols.xrefs) {
		for (const xref_entry& ref : refs) {
			const symbol_entry* target = matching_anchor(*anchors, ref.def_file, ref.def_line, ref.def_column);
			if (!target || matching_anchor(*anchors, file, ref.line, ref.column)) {
				continue;
			}
			found.push_back({
				.path = index.symbols.path_for(file),
				.qualified = target->qualified,
				.type = ref.type,
				.kind = target->kind,
				.line = ref.line,
				.column = ref.column,
				.role = site_role::reference,
			});
		}
	}
	std::ranges::sort(found, [](const query_site& lhs, const query_site& rhs) {
		return std::tie(lhs.path, lhs.line, lhs.column) < std::tie(rhs.path, rhs.line, rhs.column);
	});
	const auto repeated = std::ranges::unique(found, [](const query_site& lhs, const query_site& rhs) {
		return lhs.path == rhs.path && lhs.line == rhs.line && lhs.column == rhs.column;
	});
	found.erase(repeated.begin(), repeated.end());

	query_sites out;
	out.total = found.size();
	for (query_site& site : found) {
		if (out.shown.size() >= limit) {
			break;
		}
		out.shown.push_back(std::move(site));
	}
	return out;
}

auto gse::ide::search::resolve_sites(const index_state& index, const build_inbox::symbol_query& query, const std::size_t limit) -> std::expected<query_sites, lookup_error> {
	if (query.name.empty()) {
		return outline_sites(index, query_file(query), limit);
	}
	if (query.refs) {
		return reference_sites(index, query.name, limit);
	}
	return named_sites(index, query.name, limit);
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
			.role = symbol.is_definition ? site_role::definition : site_role::declaration,
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

	const std::expected<query_sites, lookup_error> found = resolve_sites(index, query, site_limit);
	if (!found) {
		return {
			std::format("error {}", describe(found.error())),
			std::format("indexing {}", is_pending(found.error()) ? 1 : 0),
		};
	}

	std::vector<std::string> out;
	out.push_back(std::format("mode {}", query_mode(query)));
	out.push_back(std::format("sites {} {}", found->shown.size(), found->total));

	std::uint32_t remaining = outline || !query.body ? 0 : line_budget;
	for (const auto [position, site] : std::views::enumerate(found->shown)) {
		out.push_back(std::format("site {} {} {} {} {} {}", position, site.kind, describe_role(site.role), site.line + 1, site.column + 1, site.path.generic_display_string()));
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
		const source_extent extent = query.refs
			? source_extent{ .last = site.line, .complete = true }
			: definition_extent(*source, site.line, remaining);
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
