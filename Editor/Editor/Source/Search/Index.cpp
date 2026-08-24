module gse.ide.search:index_impl;

import std;
import gse;
import gse.win32;
import gse.ide.analysis;
import gse.ide.diagnostic;
import gse.ide.lint;

import :types;
import :index;

auto gse::ide::search::unexpected_lookup(const lookup_failure reason, std::string subject) -> std::unexpected<lookup_error> {
	return std::unexpected(lookup_error{
		.reason = reason,
		.subject = std::move(subject),
	});
}

auto gse::ide::search::is_skipped_dir(const std::string_view name) -> bool {
	return name == "out" || name == ".git" || name == ".vs" || name == ".vscode" || name == ".claude" || name == ".idea" || name == "external" || name == "vcpkg" || name == ".gcc-build" || name == ".gcc-ci-build" || name == "msys64" || name == "build" || name == "node_modules" || name == ".cache";
}

auto gse::ide::search::is_binary_ext(const std::string_view ext) -> bool {
	static constexpr std::string_view list[] = {
		".exe", ".dll", ".lib", ".obj", ".o", ".a", ".pdb", ".ilk", ".gcm", ".bmi", ".ifc",
		".png", ".jpg", ".jpeg", ".bmp", ".gif", ".ico", ".tga", ".dds", ".ktx",
		".ttf", ".otf", ".woff", ".woff2",
		".zip", ".7z", ".gz", ".tar", ".rar",
		".bin", ".dat", ".pack", ".idx", ".spv", ".wav", ".mp3", ".ogg", ".mp4", ".fbx", ".glb"
	};
	return std::ranges::find(list, ext) != std::ranges::end(list);
}

auto gse::ide::search::is_cpp_ext(const std::string_view ext) -> bool {
	static constexpr std::string_view list[] = {
		".cpp", ".cppm", ".cc", ".cxx", ".ixx", ".c",
		".h", ".hpp", ".hh", ".hxx", ".inl"
	};
	return std::ranges::find(list, ext) != std::ranges::end(list);
}

auto gse::ide::search::source_dir_origin(const std::string_view name) -> std::optional<loc_origin> {
	if (name == "Engine") {
		return loc_origin::engine;
	}
	if (name == "Editor") {
		return loc_origin::editor;
	}
	if (name == "Game" || name == "Server") {
		return loc_origin::project;
	}
	return std::nullopt;
}

auto gse::ide::search::loc_language_of(const std::string_view ext) -> std::optional<loc_language> {
	if (is_cpp_ext(ext)) {
		return loc_language::cpp;
	}
	if (ext == ".slang") {
		return loc_language::slang;
	}
	return std::nullopt;
}

auto gse::ide::search::classify_loc(const index_root& owner, const std::filesystem::path& path) -> std::optional<loc_bucket> {
	const std::optional<loc_language> language = loc_language_of(to_lower(path.extension().native_encoded_string()));
	if (!language) {
		return std::nullopt;
	}
	const std::filesystem::path relative = path.lexically_normal().lexically_relative(owner.path.lexically_normal());
	if (relative.empty()) {
		return std::nullopt;
	}
	if (owner.is_project) {
		return loc_bucket{
			.origin = loc_origin::project,
			.language = *language,
		};
	}
	const std::string first = relative.begin()->display_string();
	if (const std::optional<loc_origin> origin = source_dir_origin(first)) {
		return loc_bucket{
			.origin = *origin,
			.language = *language,
		};
	}
	if (*language != loc_language::slang || first != "Resources") {
		return std::nullopt;
	}
	const std::optional<loc_origin> shader_origin = source_dir_origin(owner.name);
	if (!shader_origin) {
		return std::nullopt;
	}
	return loc_bucket{
		.origin = *shader_origin,
		.language = *language,
	};
}

auto gse::ide::search::add_loc(loc_counts& counts, const loc_bucket bucket, const std::uint64_t lines) -> void {
	loc_group* group = &counts.project;
	if (bucket.origin == loc_origin::engine) {
		group = &counts.engine;
	}
	else if (bucket.origin == loc_origin::editor) {
		group = &counts.editor;
	}
	if (bucket.language == loc_language::cpp) {
		group->cpp += lines;
	}
	else {
		group->slang += lines;
	}
}

auto gse::ide::search::loc_total(const loc_counts& counts) -> std::uint64_t {
	return counts.engine.cpp + counts.engine.slang + counts.editor.cpp + counts.editor.slang + counts.project.cpp + counts.project.slang;
}

auto gse::ide::search::loc_index::store(const loc_counts& counts) -> void {
	engine_cpp.store(counts.engine.cpp, std::memory_order_release);
	engine_slang.store(counts.engine.slang, std::memory_order_release);
	editor_cpp.store(counts.editor.cpp, std::memory_order_release);
	editor_slang.store(counts.editor.slang, std::memory_order_release);
	project_cpp.store(counts.project.cpp, std::memory_order_release);
	project_slang.store(counts.project.slang, std::memory_order_release);
}

auto gse::ide::search::loc_index::load() const -> loc_counts {
	return {
		.engine = {
			.cpp = engine_cpp.load(std::memory_order_acquire),
			.slang = engine_slang.load(std::memory_order_acquire),
		},
		.editor = {
			.cpp = editor_cpp.load(std::memory_order_acquire),
			.slang = editor_slang.load(std::memory_order_acquire),
		},
		.project = {
			.cpp = project_cpp.load(std::memory_order_acquire),
			.slang = project_slang.load(std::memory_order_acquire),
		},
	};
}

auto gse::ide::search::compute_line_starts(const std::string_view blob) -> std::vector<std::uint32_t> {
	std::vector<std::uint32_t> starts;
	starts.push_back(0);
	for (std::size_t i = 0; i < blob.size(); ++i) {
		if (blob[i] == '\n') {
			starts.push_back(static_cast<std::uint32_t>(i + 1));
		}
	}
	return starts;
}

auto gse::ide::search::canonical_path_id(const std::filesystem::path& path) -> std::pair<std::filesystem::path, id> {
	std::error_code ec;
	std::filesystem::path canon = std::filesystem::weakly_canonical(path, ec);
	if (ec) {
		canon = path;
	}
	return { canon, generate_temp_id(canon) };
}

auto gse::ide::search::module_ident_start(const char ch) -> bool {
	return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

auto gse::ide::search::module_ident_continue(const char ch) -> bool {
	return module_ident_start(ch) || (ch >= '0' && ch <= '9');
}

auto gse::ide::search::skip_module_ws(const std::string_view text, std::size_t pos) -> std::size_t {
	while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) {
		++pos;
	}
	return pos;
}

auto gse::ide::search::consume_module_keyword(const std::string_view text, std::size_t& pos, const std::string_view keyword) -> bool {
	if (text.substr(pos, keyword.size()) != keyword) {
		return false;
	}
	const std::size_t end = pos + keyword.size();
	if (end < text.size() && module_ident_continue(text[end])) {
		return false;
	}
	pos = end;
	return true;
}

auto gse::ide::search::parse_module_name(const std::string_view text, std::size_t pos) -> std::optional<std::pair<std::string, std::uint32_t>> {
	const std::size_t start = pos;
	bool need_ident = true;
	bool saw_colon = false;
	while (pos < text.size()) {
		const char ch = text[pos];
		if (module_ident_start(ch)) {
			++pos;
			while (pos < text.size() && module_ident_continue(text[pos])) {
				++pos;
			}
			need_ident = false;
			continue;
		}
		if (ch == '.' && !need_ident) {
			++pos;
			need_ident = true;
			continue;
		}
		if (ch == ':' && !saw_colon) {
			++pos;
			saw_colon = true;
			need_ident = true;
			continue;
		}
		break;
	}
	if (pos == start || need_ident) {
		return std::nullopt;
	}
	return std::pair{ std::string(text.substr(start, pos - start)), static_cast<std::uint32_t>(start) };
}

auto gse::ide::search::line_at(const std::string_view blob, const std::span<const std::uint32_t> starts, const std::size_t line) -> std::string_view {
	const std::size_t start = starts[line];
	std::size_t end = line + 1 < starts.size() ? starts[line + 1] : blob.size();
	while (end > start && (blob[end - 1] == '\n' || blob[end - 1] == '\r')) {
		--end;
	}
	return blob.substr(start, end - start);
}

auto gse::ide::search::parse_module_declaration(const std::string_view line) -> std::optional<std::pair<std::string, std::uint32_t>> {
	std::size_t pos = skip_module_ws(line, 0);
	if (pos >= line.size() || line[pos] == '/' || line[pos] == '*') {
		return std::nullopt;
	}
	if (consume_module_keyword(line, pos, "export")) {
		pos = skip_module_ws(line, pos);
	}
	if (!consume_module_keyword(line, pos, "module")) {
		return std::nullopt;
	}
	pos = skip_module_ws(line, pos);
	if (pos >= line.size() || line[pos] == ';') {
		return std::nullopt;
	}
	std::optional<std::pair<std::string, std::uint32_t>> name = parse_module_name(line, pos);
	if (name && name->first == ":private") {
		return std::nullopt;
	}
	return name;
}

auto gse::ide::search::scan_modules(const std::filesystem::path& path, const std::string_view blob, const std::span<const std::uint32_t> starts, module_index& modules) -> void {
	const file_id fid = modules.file_for(path);
	for (std::size_t line = 0; line < starts.size(); ++line) {
		if (std::optional<std::pair<std::string, std::uint32_t>> decl = parse_module_declaration(line_at(blob, starts, line))) {
			auto [name, column] = std::move(*decl);
			modules.modules.push_back({
				.name = std::move(name),
				.file = fid,
				.line = static_cast<std::uint32_t>(line),
				.column = column,
			});
		}
	}
}

auto gse::ide::search::symbol_location(const symbol_index& index, const symbol_entry& s) -> location {
	return location{
		.path = index.path_for(s.file),
		.line = s.line,
		.column = s.column,
	};
}

auto gse::ide::search::rebuild_module_lookups(module_index& index) -> void {
	index.modules_by_name.clear();
	index.modules_by_file.clear();
	for (std::uint32_t i = 0; i < index.modules.size(); ++i) {
		index.modules_by_name[index.modules[i].name].push_back(i);
		index.modules_by_file[index.modules[i].file].push_back(i);
	}
}

auto gse::ide::search::build_symbol_lookups(symbol_index& index, std::atomic<std::size_t>* progress) -> void {
	index.symbols_by_file.clear();
	index.symbols_by_name.clear();
	index.definitions_by_identity.clear();
	index.definitions_by_qualified.clear();
	for (std::uint32_t i = 0; i < index.symbols.size(); ++i) {
		const symbol_entry& symbol = index.symbols[i];
		index.symbols_by_file[symbol.file].push_back(i);
		index.symbols_by_name[symbol.name].push_back(i);
		if (symbol.is_definition) {
			if (!symbol.identity.empty()) {
				index.definitions_by_identity[symbol.identity].push_back(i);
			}
			if (!symbol.qualified.empty()) {
				index.definitions_by_qualified[symbol.qualified].push_back(i);
			}
		}
		if (progress) {
			progress->store(static_cast<std::size_t>(i) + 1, std::memory_order_release);
		}
	}
}

auto gse::ide::search::sort_symbol_ids(const symbol_index& index, std::vector<std::uint32_t>& ids) -> void {
	std::ranges::sort(ids, [&](const std::uint32_t lhs, const std::uint32_t rhs) {
		const symbol_entry& a = index.symbols[lhs];
		const symbol_entry& b = index.symbols[rhs];
		return std::tie(a.line, a.column) < std::tie(b.line, b.column);
	});
}

auto gse::ide::search::sort_xrefs(std::vector<xref_entry>& refs) -> void {
	std::ranges::sort(refs, [](const xref_entry& a, const xref_entry& b) {
		return std::tie(a.line, a.column, a.length) < std::tie(b.line, b.column, b.length);
	});
}

auto gse::ide::search::sort_symbol_locations(symbol_index& index, std::atomic<std::size_t>* progress) -> void {
	std::size_t completed = 0;
	for (auto& ids : index.symbols_by_file | std::views::values) {
		sort_symbol_ids(index, ids);
		if (progress) {
			progress->store(++completed, std::memory_order_release);
		}
	}
	for (auto& refs : index.xrefs | std::views::values) {
		sort_xrefs(refs);
		if (progress) {
			progress->store(++completed, std::memory_order_release);
		}
	}
}

auto gse::ide::search::rebuild_symbol_lookups(symbol_index& index) -> void {
	build_symbol_lookups(index, nullptr);
	sort_symbol_locations(index, nullptr);
}

auto gse::ide::search::next_file_search_snapshot(index_state& index) -> std::shared_ptr<file_search_snapshot> {
	auto next = std::make_shared<file_search_snapshot>(*index.current_search_snapshot->files);
	index.generation.fetch_add(1, std::memory_order_acq_rel);
	next->file_generation = index.file_generation.fetch_add(1, std::memory_order_acq_rel) + 1;
	return next;
}

auto gse::ide::search::next_symbol_search_snapshot(index_state& index) -> std::shared_ptr<symbol_search_snapshot> {
	auto next = std::make_shared<symbol_search_snapshot>(*index.current_search_snapshot->symbols);
	index.generation.fetch_add(1, std::memory_order_acq_rel);
	next->symbol_generation = index.symbol_generation.fetch_add(1, std::memory_order_acq_rel) + 1;
	return next;
}

auto gse::ide::search::publish_file_search_snapshot(index_state& index) -> void {
	auto next = next_file_search_snapshot(index);
	const std::span<const file_entry> file_entries = index.files.entries.items();
	next->files = std::make_shared<const std::vector<file_entry>>(file_entries.begin(), file_entries.end());

	auto content_entries = std::make_shared<std::vector<std::shared_ptr<const content_entry>>>();
	content_entries->reserve(index.content.entries.size());
	for (const std::shared_ptr<content_entry>& entry : index.content.entries.items()) {
		content_entries->push_back(entry);
	}
	next->content = std::move(content_entries);
	auto combined = std::make_shared<search_snapshot>(*index.current_search_snapshot);
	combined->files = std::move(next);
	index.current_search_snapshot = std::move(combined);
}

auto gse::ide::search::publish_symbol_search_snapshot(index_state& index) -> void {
	auto next = next_symbol_search_snapshot(index);
	auto searchable = std::make_shared<std::vector<searchable_symbol>>();
	searchable->reserve(index.symbols.symbols.size());
	for (const symbol_entry& symbol : index.symbols.symbols) {
		searchable->push_back({
			.path = index.symbols.path_for(symbol.file),
			.name = symbol.name,
			.name_lower = symbol.name_lower,
			.kind = symbol.kind,
			.line = symbol.line,
			.column = symbol.column,
		});
	}
	next->symbols = std::move(searchable);

	auto lints = std::make_shared<lint_snapshot>();
	lints->symbol_generation = next->symbol_generation;
	auto sites = std::make_shared<std::vector<lint_site>>();
	sites->reserve(index.symbols.lints.size());
	for (const lint_entry& entry : index.symbols.lints) {
		sites->push_back({
			.path = index.symbols.path_for(entry.file),
			.rule = entry.rule,
			.edit = entry.edit,
		});
	}
	lints->sites = std::move(sites);

	auto combined = std::make_shared<search_snapshot>(*index.current_search_snapshot);
	combined->symbols = std::move(next);
	combined->lints = std::move(lints);
	index.current_search_snapshot = std::move(combined);
}

auto gse::ide::search::xref_at(const symbol_index& index, const file_id file, const std::uint32_t line, const std::uint32_t column) -> const xref_entry* {
	const auto file_it = index.xrefs.find(file);
	if (file_it == index.xrefs.end()) {
		return nullptr;
	}
	const std::span<const xref_entry> refs = file_it->second;
	auto it = std::ranges::lower_bound(refs, line, {}, &xref_entry::line);
	for (; it != refs.end() && it->line == line; ++it) {
		if (column >= it->column && column < it->column + it->length) {
			return &*it;
		}
	}
	return nullptr;
}

auto gse::ide::search::symbol_at_location(const symbol_index& index, const file_id file, const std::uint32_t line, const std::uint32_t column, const bool contains_column) -> const symbol_entry* {
	const auto file_it = index.symbols_by_file.find(file);
	if (file_it == index.symbols_by_file.end()) {
		return nullptr;
	}
	const std::span<const std::uint32_t> ids = file_it->second;
	auto it = std::ranges::lower_bound(ids, line, {}, [&](const std::uint32_t id) {
		return index.symbols[id].line;
	});
	for (; it != ids.end() && index.symbols[*it].line == line; ++it) {
		const symbol_entry& symbol = index.symbols[*it];
		if ((!contains_column && symbol.column == column) || (contains_column && column >= symbol.column && column < symbol.column + symbol.name.size())) {
			return &symbol;
		}
	}
	return nullptr;
}

auto gse::ide::search::is_lookup_declaration(const symbol_entry& symbol) -> bool {
	return analysis::is_definition_kind(symbol.kind) || symbol.kind == analysis::symbol_kind::variable || symbol.kind == analysis::symbol_kind::parameter || symbol.kind == analysis::symbol_kind::member || symbol.kind == analysis::symbol_kind::enum_member || symbol.kind == analysis::symbol_kind::name_space || symbol.kind == analysis::symbol_kind::alias;
}

auto gse::ide::search::same_symbol_candidate(const symbol_entry& lhs, const symbol_entry& rhs) -> bool {
	if (!lhs.identity.empty() && !rhs.identity.empty()) {
		return lhs.identity == rhs.identity;
	}
	return lhs.file == rhs.file && lhs.line == rhs.line && lhs.column == rhs.column;
}

auto gse::ide::search::selection_score(const symbol_entry& symbol, const std::string_view needle, const std::string_view suffix, const bool qualified, const std::optional<file_id> click_file, const symbol_selection_mode mode) -> std::optional<int> {
	if (!is_lookup_declaration(symbol)) {
		return std::nullopt;
	}
	auto prefer_definition = [&](int score) {
		if (mode == symbol_selection_mode::definition && symbol.is_definition) {
			score += symbol.kind == analysis::symbol_kind::function ? 3 : 1;
		}
		return score;
	};
	if (qualified) {
		if (symbol.qualified == needle) {
			return prefer_definition(20);
		}
		if (symbol.qualified.ends_with(suffix)) {
			return prefer_definition(10);
		}
		return std::nullopt;
	}

	int score = 10;
	if (mode == symbol_selection_mode::definition) {
		if (click_file && symbol.file == *click_file) {
			score = 30;
		}
		else if (analysis::is_type_kind(symbol.kind)) {
			score = 20;
		}
	}
	return prefer_definition(score);
}

auto gse::ide::search::select_symbol(const symbol_index& index, const std::span<const std::uint32_t> candidates, const std::string_view name, const std::string_view qualifier, const std::optional<file_id> click_file, const symbol_selection_mode mode) -> symbol_selection {
	std::string needle;
	needle.reserve(qualifier.size() + name.size());
	needle.append(qualifier);
	needle.append(name);
	std::string suffix = "::";
	suffix.append(needle);

	symbol_selection selection;
	int best_score = 0;
	for (const std::uint32_t id : candidates) {
		const symbol_entry& candidate = index.symbols[id];
		const std::optional<int> score = selection_score(candidate, needle, suffix, !qualifier.empty(), click_file, mode);
		if (!score) {
			continue;
		}
		if (*score > best_score) {
			selection.value = &candidate;
			selection.ambiguous = false;
			best_score = *score;
		}
		else if (*score == best_score && selection.value && !same_symbol_candidate(*selection.value, candidate)) {
			selection.ambiguous = true;
		}
	}
	return selection;
}

auto gse::ide::search::matching_definition(const symbol_index& index, const std::string_view identity, const std::string_view qualified, const file_id from_file, const std::uint32_t from_line, const std::uint32_t from_column) -> std::optional<location> {
	const std::vector<std::uint32_t>* candidates = nullptr;
	if (!identity.empty()) {
		if (const auto it = index.definitions_by_identity.find(identity); it != index.definitions_by_identity.end()) {
			candidates = &it->second;
		}
	}
	else if (!qualified.empty()) {
		if (const auto it = index.definitions_by_qualified.find(qualified); it != index.definitions_by_qualified.end()) {
			candidates = &it->second;
		}
	}
	if (!candidates) {
		return std::nullopt;
	}
	for (const std::uint32_t id : *candidates) {
		const symbol_entry& symbol = index.symbols[id];
		if (symbol.is_definition && (symbol.file != from_file || symbol.line != from_line || symbol.column != from_column)) {
			return symbol_location(index, symbol);
		}
	}
	return std::nullopt;
}

auto gse::ide::search::promoted_definition(const symbol_index& index, const file_id file, const std::uint32_t line, const std::uint32_t column, const std::string_view identity, const std::string_view qualified) -> location {
	const location target{
		.path = index.path_for(file),
		.line = line,
		.column = column,
	};
	if (const symbol_entry* symbol = symbol_at_location(index, file, line, column, false)) {
		if (!symbol->is_definition) {
			return matching_definition(index, symbol->identity.empty() ? identity : std::string_view(symbol->identity), symbol->qualified.empty() ? qualified : std::string_view(symbol->qualified), file, line, column).value_or(target);
		}
		return target;
	}
	return matching_definition(index, identity, qualified, file, line, column).value_or(target);
}

auto gse::ide::search::log_index_phase_completion(const index_state& idx) -> void {
	const gse::time_t<double> started = idx.phase_started.load(std::memory_order_relaxed);
	if (started == gse::time_t<double>{}) {
		return;
	}
	const index_phase phase = idx.phase.load(std::memory_order_acquire);
	const index_phase_info info = annotation_from_enum<index_phase_info>(phase, {});
	const gse::time_t<double> elapsed = gse::system_clock::now<gse::time_t<double>>() - started;
	const std::size_t done = idx.progress_done.load(std::memory_order_relaxed);
	const std::size_t total = idx.progress_total.load(std::memory_order_relaxed);
	log::println(
		log::level::info,
		log::category::general,
		"[symidx] Finished {} in {:.1f:ms}{}",
		info.label,
		elapsed,
		total == 0 ? "" : std::format(" ({}/{} items)", done, total));
}

auto gse::ide::search::append_entry(std::string& out, const std::string& entry) -> void {
	if (!out.empty()) {
		out += "; ";
	}
	out += entry;
}

auto gse::ide::search::xrefs_on_line(const symbol_index& index, const file_id file, const std::uint32_t line) -> std::string {
	const auto file_it = index.xrefs.find(file);
	if (file_it == index.xrefs.end()) {
		return "(file has no references)";
	}
	const std::span<const xref_entry> refs = file_it->second;
	std::string out;
	auto it = std::ranges::lower_bound(refs, line, {}, &xref_entry::line);
	for (; it != refs.end() && it->line == line; ++it) {
		append_entry(
			out,
			std::format(
				"col {} len {} '{}' -> {}:{}:{}",
				it->column + 1,
				it->length,
				it->qualified.empty() ? std::string_view("(unqualified)") : std::string_view(it->qualified),
				index.path_for(it->def_file).generic_display_string(),
				it->def_line + 1,
				it->def_column + 1));
	}
	return out.empty() ? "(none)" : out;
}

auto gse::ide::search::symbols_on_line(const symbol_index& index, const file_id file, const std::uint32_t line) -> std::string {
	const auto file_it = index.symbols_by_file.find(file);
	if (file_it == index.symbols_by_file.end()) {
		return "(file has no symbols)";
	}
	std::string out;
	for (const std::uint32_t id : file_it->second) {
		const symbol_entry& symbol = index.symbols[id];
		if (symbol.line != line) {
			continue;
		}
		append_entry(
			out,
			std::format(
				"col {} {} '{}' {}",
				symbol.column + 1,
				symbol.kind,
				symbol.qualified.empty() ? symbol.name : symbol.qualified,
				symbol.is_definition ? "definition" : "declaration"));
	}
	return out.empty() ? "(none)" : out;
}

auto gse::ide::search::params_on_line(const symbol_index& index, const file_id file, const std::uint32_t line) -> std::string {
	const auto file_it = index.params.find(file);
	if (file_it == index.params.end()) {
		return "(file has no semantic tokens)";
	}
	std::string out;
	for (const positioned_kind& param : file_it->second) {
		if (param.line != line) {
			continue;
		}
		append_entry(out, std::format("col {} len {} {}", param.column + 1, param.length, param.kind));
	}
	return out.empty() ? "(none)" : out;
}

auto gse::ide::search::begin_index_phase(index_state& idx, const index_phase phase, const std::size_t total) -> void {
	log_index_phase_completion(idx);
	idx.progress_total.store(total, std::memory_order_relaxed);
	idx.progress_done.store(0, std::memory_order_relaxed);
	idx.phase_started.store(gse::system_clock::now<gse::time_t<double>>(), std::memory_order_relaxed);
	idx.phase.store(phase, std::memory_order_release);
	const index_phase_info info = annotation_from_enum<index_phase_info>(phase, {});
	log::println(
		log::level::debug,
		log::category::general,
		"[symidx] {}{}",
		info.label,
		total == 0 ? "" : std::format(" ({} items)", total));
}

auto gse::ide::search::end_index_progress(index_state& idx) -> void {
	log_index_phase_completion(idx);
	idx.progress_total.store(0, std::memory_order_relaxed);
	idx.progress_done.store(0, std::memory_order_relaxed);
	idx.phase_started.store(gse::time_t<double>{}, std::memory_order_relaxed);
	idx.phase.store(index_phase::idle, std::memory_order_release);
}

auto gse::ide::search::run_symbol_batch(const symbol_batch_request& request, index_state& idx, const std::stop_token stop) -> std::vector<analysis::tu_symbols> {
	const auto& [list, plugin_dll, roots, workers, compile_phase] = request;
	std::vector<analysis::tu_symbols> out(list.size());
	const std::size_t n = std::min(workers, list.size());
	auto run_parallel = [&](auto&& fn) {
		std::atomic<std::size_t> next = 0;
		std::vector<std::thread> pool;
		for (std::size_t worker = 0; worker < n; ++worker) {
			pool.emplace_back([&] {
				while (true) {
					if (idx.cancel.load(std::memory_order_acquire)) {
						break;
					}
					const std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
					if (i >= list.size()) {
						break;
					}
					fn(i);
					idx.progress_done.fetch_add(1, std::memory_order_relaxed);
				}
			});
		}
		for (std::thread& thread : pool) {
			thread.join();
		}
	};

	begin_index_phase(idx, index_phase::checking_modules, list.size());
	run_parallel([&](const std::size_t i) {
		out[i].tu = list[i]->file;
		if (const std::expected<void, std::string> module_graph = analysis::validate_module_graph(*list[i]); !module_graph) {
			out[i].failure = analysis::symbol_index_failure::module_unavailable;
			out[i].failure_detail = module_graph.error();
		}
	});
	if (idx.cancel.load(std::memory_order_acquire)) {
		return out;
	}

	const analysis::symbol_request plugin_request{
		.plugin_dll = plugin_dll,
		.workspace_roots = roots,
		.module_graph_validated = true,
	};
	begin_index_phase(idx, compile_phase, list.size());
	run_parallel([&](const std::size_t i) {
		if (out[i].failure == analysis::symbol_index_failure::none) {
			out[i] = analysis::symbol_index_builder::run_one(*list[i], plugin_request, stop);
		}
	});
	return out;
}

auto gse::ide::search::is_indexed_path(const std::filesystem::path& root, const std::filesystem::path& path) -> bool {
	const std::filesystem::path relative = path.lexically_normal().lexically_relative(root.lexically_normal());
	if (relative.empty() || *relative.begin() == "..") {
		return false;
	}
	for (const std::filesystem::path& part : relative) {
		if (is_skipped_dir(to_lower(part.native_encoded_string()))) {
			return false;
		}
	}
	return true;
}

auto gse::ide::search::owning_root(const std::span<const index_root> roots, const std::filesystem::path& path) -> const index_root* {
	const auto it = std::ranges::find_if(roots, [&](const index_root& root) {
		return is_indexed_path(root.path, path);
	});
	return it != roots.end() ? &*it : nullptr;
}

auto gse::ide::search::module_index::file_for(const std::filesystem::path& path) -> file_id {
	auto [canon, canonical_id] = canonical_path_id(path);
	files.try_emplace(canonical_id, std::move(canon));
	return canonical_id;
}

gse::ide::search::module_index::~module_index() = default;

auto gse::ide::search::module_index::path_for(const file_id id) const -> std::filesystem::path {
	if (const auto it = files.find(id); it != files.end()) {
		return it->second;
	}
	return {};
}

auto gse::ide::search::symbol_index::file_for(const std::filesystem::path& path) -> file_id {
	auto [canon, canonical_id] = canonical_path_id(path);
	files.try_emplace(canonical_id, std::move(canon));
	return canonical_id;
}

gse::ide::search::symbol_index::~symbol_index() = default;

auto gse::ide::search::symbol_index::path_for(const file_id id) const -> std::filesystem::path {
	if (const auto it = files.find(id); it != files.end()) {
		return it->second;
	}
	return {};
}

auto gse::ide::search::describe(const lookup_error& error) -> std::string {
	switch (error.reason) {
	case lookup_failure::index_unavailable:
		return "the symbol index is unavailable";
	case lookup_failure::file_not_indexed:
		return std::format("'{}' is not in the symbol index", error.subject);
	case lookup_failure::reference_not_found:
		return std::format("no compiler reference covers {}", error.subject);
	case lookup_failure::symbol_not_found:
		return std::format("symbol '{}' is absent from the index", error.subject);
	case lookup_failure::definition_not_found:
		return std::format("no definition was recorded for '{}'", error.subject);
	case lookup_failure::qualified_symbol_not_found:
		return std::format("no definition matched qualified symbol '{}'", error.subject);
	case lookup_failure::ambiguous_symbol:
		return std::format("unqualified symbol '{}' is ambiguous", error.subject);
	case lookup_failure::module_name_empty:
		return "the module name is empty";
	case lookup_failure::module_context_not_found:
		return std::format("relative module '{}' has no indexed owning module", error.subject);
	case lookup_failure::module_not_found:
		return std::format("module '{}' is absent from the module index", error.subject);
	case lookup_failure::index_building:
		return error.subject.empty()
			? "semantic information is still indexing"
			: std::format("semantic information for '{}' is still indexing", error.subject);
	case lookup_failure::translation_unit_failed:
		return error.detail.empty()
			? std::format("semantic compilation failed for '{}'", error.subject)
			: std::format("semantic compilation failed for '{}': {}", error.subject, error.detail);
	case lookup_failure::kind_not_recorded:
		return std::format("the compiler did not record a symbol kind for '{}'", error.subject);
	case lookup_failure::type_not_recorded:
		return std::format("the compiler did not record a resolved type for '{}'", error.subject);
	}
	return "the semantic lookup failed for an unknown reason";
}

auto gse::ide::search::is_pending(const lookup_error& error) -> bool {
	return error.reason == lookup_failure::index_building;
}

auto gse::ide::search::report_lookup_failure(const index_state& index, const lookup_probe& probe, const std::span<const std::string> causes) -> void {
	const std::uint64_t generation = index.generation.load(std::memory_order_acquire);
	const std::string path = probe.file.generic_display_string();

	std::shared_lock lock(index.mutex);
	const file_id fid = canonical_path_id(probe.file).second;
	const index_phase phase = index.phase.load(std::memory_order_acquire);
	const index_phase_info info = annotation_from_enum<index_phase_info>(phase, {});
	const auto failure = index.symbols.failures.find(fid);

	log::println(
		log::level::warning,
		log::category::general,
		"[semantic] {} miss '{}' at {}:{}:{} (byte col {}, len {})",
		probe.context.empty() ? std::string_view("lookup") : std::string_view(probe.context),
		probe.ident,
		path,
		probe.line + 1,
		probe.column + 1,
		probe.column,
		probe.length
	);
	if (!probe.row.empty()) {
		log::println(log::level::warning, log::category::general, "[semantic]   line: |{}|", probe.row);
	}
	if (causes.empty()) {
		log::println(log::level::warning, log::category::general, "[semantic]   cause: nothing was recorded, so the lookup silently produced no result");
	}
	for (const std::string& cause : causes) {
		log::println(log::level::warning, log::category::general, "[semantic]   cause: {}", cause);
	}
	log::println(
		log::level::warning,
		log::category::general,
		"[semantic]   index: gen={} phase={} ready={} building={} file={} tu={}",
		generation,
		info.label,
		index.symbols_ready.load(std::memory_order_acquire),
		index.building.load(std::memory_order_acquire),
		index.symbols.files.contains(fid) ? "indexed" : (index.pending_symbol_files.contains(fid) ? "pending" : "absent"),
		failure == index.symbols.failures.end() ? std::string_view("ok") : std::string_view(failure->second)
	);
	log::println(log::level::warning, log::category::general, "[semantic]   xrefs on line {}: {}", probe.line + 1, xrefs_on_line(index.symbols, fid, probe.line));
	log::println(log::level::warning, log::category::general, "[semantic]   syms on line {}: {}", probe.line + 1, symbols_on_line(index.symbols, fid, probe.line));
	log::println(log::level::warning, log::category::general, "[semantic]   sem tokens on line {}: {}", probe.line + 1, params_on_line(index.symbols, fid, probe.line));
}

auto gse::ide::search::index_state::definition_at(const std::filesystem::path& file, const std::uint32_t line, const std::uint32_t column) const -> std::expected<location, lookup_error> {
	std::shared_lock lock(mutex);
	const file_id fid = canonical_path_id(file).second;
	if (!symbols_ready.load(std::memory_order_acquire) || pending_symbol_files.contains(fid)) {
		return unexpected_lookup(lookup_failure::index_building, file.generic_display_string());
	}
	if (!symbols.files.contains(fid)) {
		return unexpected_lookup(lookup_failure::file_not_indexed, file.generic_display_string());
	}
	if (const auto failure = symbols.failures.find(fid); failure != symbols.failures.end()) {
		return std::unexpected(lookup_error{
			.reason = lookup_failure::translation_unit_failed,
			.subject = file.generic_display_string(),
			.detail = failure->second,
		});
	}
	if (const xref_entry* xref = xref_at(symbols, fid, line, column)) {
		location definition = promoted_definition(symbols, xref->def_file, xref->def_line, xref->def_column, xref->identity, xref->qualified);
		if (definition.path.empty()) {
			return unexpected_lookup(lookup_failure::definition_not_found, xref->qualified);
		}
		return definition;
	}
	if (const symbol_entry* symbol = symbol_at_location(symbols, fid, line, column, true)) {
		if (symbol->is_definition) {
			return symbol_location(symbols, *symbol);
		}
		if (const std::optional<location> definition = matching_definition(symbols, symbol->identity, symbol->qualified, fid, symbol->line, symbol->column)) {
			return *definition;
		}
		return unexpected_lookup(lookup_failure::definition_not_found, symbol->qualified);
	}
	return std::unexpected(lookup_error{
		.reason = lookup_failure::reference_not_found,
		.subject = std::format("{}:{}:{}", file.generic_display_string(), line + 1, column + 1),
	});
}

auto gse::ide::search::index_state::symbol_at(const std::filesystem::path& file, const std::uint32_t line, const std::uint32_t column) const -> std::expected<hover_hit, lookup_error> {
	std::shared_lock lock(mutex);
	const file_id fid = canonical_path_id(file).second;
	if (!symbols_ready.load(std::memory_order_acquire) || pending_symbol_files.contains(fid)) {
		return unexpected_lookup(lookup_failure::index_building, file.generic_display_string());
	}
	if (!symbols.files.contains(fid)) {
		return unexpected_lookup(lookup_failure::file_not_indexed, file.generic_display_string());
	}
	if (const auto failure = symbols.failures.find(fid); failure != symbols.failures.end()) {
		return std::unexpected(lookup_error{
			.reason = lookup_failure::translation_unit_failed,
			.subject = file.generic_display_string(),
			.detail = failure->second,
		});
	}
	const xref_entry* xref = xref_at(symbols, fid, line, column);
	if (!xref) {
		return std::unexpected(lookup_error{
			.reason = lookup_failure::reference_not_found,
			.subject = std::format("{}:{}:{}", file.generic_display_string(), line + 1, column + 1),
		});
	}
	const location definition = promoted_definition(symbols, xref->def_file, xref->def_line, xref->def_column, xref->identity, xref->qualified);
	if (definition.path.empty()) {
		return unexpected_lookup(lookup_failure::definition_not_found, xref->qualified);
	}
	std::string kind;
	std::vector<lookup_error> issues;
	const file_id definition_file = canonical_path_id(definition.path).second;
	if (const symbol_entry* symbol = symbol_at_location(symbols, definition_file, definition.line, definition.column, false)) {
		kind = std::format("{}", symbol->kind);
		const bool needs_type = symbol->kind == analysis::symbol_kind::variable || symbol->kind == analysis::symbol_kind::parameter || symbol->kind == analysis::symbol_kind::member;
		if (needs_type && xref->type.empty()) {
			issues.push_back({
				.reason = lookup_failure::type_not_recorded,
				.subject = xref->qualified,
			});
		}
	}
	else {
		issues.push_back({
			.reason = lookup_failure::kind_not_recorded,
			.subject = xref->qualified,
		});
	}
	return hover_hit{
		.def = definition,
		.qualified = xref->qualified,
		.kind = kind,
		.type = xref->type,
		.value = xref->value,
		.issues = std::move(issues),
	};
}

auto gse::ide::search::index_state::symbol_definition(const std::string_view name, const std::string_view qualifier, const std::filesystem::path& click_file) const -> std::expected<location, lookup_error> {
	std::shared_lock lock(mutex);
	if (!symbols_ready.load(std::memory_order_acquire)) {
		return unexpected_lookup(lookup_failure::index_building, std::string(name));
	}
	const auto candidates = symbols.symbols_by_name.find(name);
	if (candidates == symbols.symbols_by_name.end()) {
		return unexpected_lookup(lookup_failure::symbol_not_found, std::string(name));
	}
	const std::optional<file_id> click_fid = click_file.empty()
		? std::nullopt
		: std::optional{ canonical_path_id(click_file).second };
	const symbol_selection selected = select_symbol(symbols, candidates->second, name, qualifier, click_fid, symbol_selection_mode::definition);
	std::string subject;
	subject.reserve(qualifier.size() + name.size());
	subject.append(qualifier);
	subject.append(name);
	if (!selected.value) {
		return std::unexpected(lookup_error{
			.reason = qualifier.empty() ? lookup_failure::definition_not_found : lookup_failure::qualified_symbol_not_found,
			.subject = std::move(subject),
		});
	}
	if (selected.ambiguous) {
		return unexpected_lookup(lookup_failure::ambiguous_symbol, std::string(name));
	}
	return location{
		.path = symbols.path_for(selected.value->file),
		.line = selected.value->line,
		.column = selected.value->column,
	};
}

auto gse::ide::search::is_symbol_source(const std::filesystem::path& path) -> bool {
	return is_cpp_ext(to_lower(path.extension().native_encoded_string()));
}

auto gse::ide::search::index_state::module_definition(const std::string_view name, const std::filesystem::path& click_file) const -> std::expected<location, lookup_error> {
	if (name.empty()) {
		return unexpected_lookup(lookup_failure::module_name_empty);
	}
	std::string target(name);
	const file_id file_identity = canonical_path_id(click_file).second;
	std::shared_lock lock(mutex);
	if (!files.loaded.load(std::memory_order_acquire)) {
		return unexpected_lookup(lookup_failure::index_building, target);
	}
	if (target.starts_with(':')) {
		if (!modules.files.contains(file_identity)) {
			return unexpected_lookup(lookup_failure::module_context_not_found, target);
		}
		std::string current;
		if (const auto ids = modules.modules_by_file.find(file_identity); ids != modules.modules_by_file.end()) {
			for (const std::uint32_t id : ids->second) {
				current = modules.modules[id].name;
				if (!current.empty()) {
					break;
				}
			}
		}
		if (current.empty()) {
			return unexpected_lookup(lookup_failure::module_context_not_found, target);
		}
		if (const std::size_t colon = current.find(':'); colon != std::string::npos) {
			current.erase(colon);
		}
		target.insert(0, current);
	}

	const auto candidates = modules.modules_by_name.find(target);
	if (candidates == modules.modules_by_name.end()) {
		return unexpected_lookup(lookup_failure::module_not_found, target);
	}
	const module_entry* best = &modules.modules[candidates->second.front()];
	for (const std::uint32_t id : candidates->second) {
		const module_entry& m = modules.modules[id];
		if (modules.path_for(m.file).extension() == ".cppm") {
			best = &m;
			break;
		}
	}
	return location{
		.path = modules.path_for(best->file),
		.line = best->line,
		.column = best->column,
	};
}

auto gse::ide::search::index_state::declaration_of(const std::string_view name, const std::string_view qualifier) const -> std::expected<hover_hit, lookup_error> {
	std::shared_lock lock(mutex);
	if (!symbols_ready.load(std::memory_order_acquire)) {
		return unexpected_lookup(lookup_failure::index_building, std::string(name));
	}
	const auto candidates = symbols.symbols_by_name.find(name);
	if (candidates == symbols.symbols_by_name.end()) {
		return unexpected_lookup(lookup_failure::symbol_not_found, std::string(name));
	}
	const symbol_selection selected = select_symbol(symbols, candidates->second, name, qualifier, std::nullopt, symbol_selection_mode::declaration);
	std::string subject;
	subject.reserve(qualifier.size() + name.size());
	subject.append(qualifier);
	subject.append(name);
	if (!selected.value) {
		return std::unexpected(lookup_error{
			.reason = qualifier.empty() ? lookup_failure::definition_not_found : lookup_failure::qualified_symbol_not_found,
			.subject = std::move(subject),
		});
	}
	if (selected.ambiguous) {
		return std::unexpected(lookup_error{
			.reason = lookup_failure::ambiguous_symbol,
			.subject = std::string(name),
		});
	}
	const symbol_entry* pick = selected.value;
	std::string type;
	std::string value;
	std::vector<lookup_error> issues;
	if (const xref_entry* reference = xref_at(symbols, pick->file, pick->line, pick->column)) {
		type = reference->type;
		value = reference->value;
	}
	const bool needs_type = pick->kind == analysis::symbol_kind::variable || pick->kind == analysis::symbol_kind::parameter || pick->kind == analysis::symbol_kind::member;
	if (needs_type && type.empty()) {
		issues.push_back({
			.reason = lookup_failure::type_not_recorded,
			.subject = pick->qualified,
		});
	}
	return hover_hit{
		.def = {
			.path = symbols.path_for(pick->file),
			.line = pick->line,
			.column = pick->column,
		},
		.qualified = pick->qualified,
		.kind = std::format("{}", pick->kind),
		.type = std::move(type),
		.value = std::move(value),
		.issues = std::move(issues),
	};
}

auto gse::ide::search::index_state::semantic_kind_of(const std::string_view name) const -> std::expected<analysis::semantic_kind, lookup_error> {
	std::shared_lock lock(mutex);
	if (!symbols_ready.load(std::memory_order_acquire)) {
		return unexpected_lookup(lookup_failure::index_building, std::string(name));
	}
	const auto candidates = symbols.symbols_by_name.find(name);
	if (candidates == symbols.symbols_by_name.end()) {
		return unexpected_lookup(lookup_failure::symbol_not_found, std::string(name));
	}
	const symbol_selection selected = select_symbol(symbols, candidates->second, name, {}, std::nullopt, symbol_selection_mode::declaration);
	if (!selected.value) {
		return unexpected_lookup(lookup_failure::kind_not_recorded, std::string(name));
	}
	if (selected.ambiguous) {
		return unexpected_lookup(lookup_failure::ambiguous_symbol, std::string(name));
	}
	return analysis::to_semantic_kind(selected.value->kind);
}

auto gse::ide::search::index_state::semantic_tokens_in(const std::filesystem::path& file, const std::uint32_t line_begin, const std::uint32_t line_end) const -> std::vector<positioned_kind> {
	std::shared_lock lock(mutex);
	std::vector<positioned_kind> out;
	const file_id fid = canonical_path_id(file).second;
	if (!symbols.files.contains(fid)) {
		return out;
	}

	if (const auto pit = symbols.params.find(fid); pit != symbols.params.end() && !pit->second.empty()) {
		for (const positioned_kind& p : pit->second) {
			if (p.line < line_begin || p.line >= line_end) {
				continue;
			}
			out.push_back(p);
		}
		return out;
	}

	if (const auto sit = symbols.symbols_by_file.find(fid); sit != symbols.symbols_by_file.end()) {
		for (const std::uint32_t id : sit->second) {
			const symbol_entry& s = symbols.symbols[id];
			if (s.line < line_begin || s.line >= line_end || s.name.empty()) {
				continue;
			}
			out.push_back({
				.line = s.line,
				.column = s.column,
				.length = static_cast<std::uint32_t>(s.name.size()),
				.kind = analysis::to_semantic_kind(s.kind),
			});
		}
	}

	if (const auto xit = symbols.xrefs.find(fid); xit != symbols.xrefs.end()) {
		for (const xref_entry& x : xit->second) {
			if (x.line < line_begin || x.line >= line_end || x.length == 0) {
				continue;
			}
			const symbol_entry* target = symbol_at_location(symbols, x.def_file, x.def_line, x.def_column, false);
			if (!target && !x.identity.empty()) {
				if (const auto it = symbols.definitions_by_identity.find(x.identity); it != symbols.definitions_by_identity.end()) {
					target = &symbols.symbols[it->second.front()];
				}
			}
			if (!target && !x.qualified.empty()) {
				if (const auto it = symbols.definitions_by_qualified.find(x.qualified); it != symbols.definitions_by_qualified.end()) {
					target = &symbols.symbols[it->second.front()];
				}
			}
			if (!target) {
				continue;
			}
			out.push_back({
				.line = x.line,
				.column = x.column,
				.length = x.length,
				.kind = analysis::to_semantic_kind(target->kind),
			});
		}
	}

	return out;
}

auto gse::ide::search::build_files_and_content(index_state& idx, const std::span<const index_root> roots) -> void {
	id_mapped_collection<file_entry> files;
	id_mapped_collection<std::shared_ptr<content_entry>> content;

	const auto opts = std::filesystem::directory_options::skip_permission_denied;
	for (const auto& [root_index, root] : std::views::enumerate(roots)) {
		std::error_code ec;
		for (auto it = std::filesystem::recursive_directory_iterator(root.path, opts, ec); it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
			if (ec) {
				ec.clear();
				continue;
			}
			const std::filesystem::directory_entry& entry = *it;
			std::error_code type_ec;
			if (entry.is_directory(type_ec)) {
				if (is_skipped_dir(to_lower(entry.path().filename().native_encoded_string()))) {
					it.disable_recursion_pending();
				}
				continue;
			}
			if (!entry.is_regular_file(type_ec)) {
				continue;
			}
			const std::filesystem::path& path = entry.path();
			if (owning_root(roots.first(static_cast<std::size_t>(root_index)), path)) {
				continue;
			}
			const std::filesystem::path relative = std::filesystem::relative(path, root.path, type_ec);
			std::string rel = relative.empty() ? path.filename().generic_display_string() : (root.name + "/" + relative.generic_display_string());
			const auto [resolved, file_identity] = canonical_path_id(path);
			files.add(file_identity, {
				.path = resolved,
				.rel = rel,
				.rel_lower = to_lower(rel),
			});

			const std::string ext = to_lower(path.extension().native_encoded_string());
			if (!is_binary_ext(ext)) {
				const auto size = entry.file_size(type_ec);
				if (!type_ec && size <= 2u * 1024u * 1024u) {
					content.add(file_identity, std::make_shared<content_entry>(content_entry{
						.path = resolved,
					}));
				}
			}
		}
	}

	const std::span<std::shared_ptr<content_entry>> content_entries = content.items();

	task::coarse_parallel(content_entries.size(), 8, [&](std::size_t i) {
		content_entry& entry = *content_entries[i];
		std::ifstream in(entry.path, std::ios::binary);
		if (!in) {
			return;
		}
		std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		entry.line_starts = compute_line_starts(data);
		entry.blob = std::move(data);
	});

	loc_counts loc;
	module_index module_defs;
	for (const std::shared_ptr<content_entry>& entry_ptr : content_entries) {
		const content_entry& entry = *entry_ptr;
		if (entry.line_starts.empty()) {
			continue;
		}
		if (is_symbol_source(entry.path)) {
			scan_modules(entry.path, entry.blob, entry.line_starts, module_defs);
		}
		const index_root* owner = owning_root(roots, entry.path);
		if (!owner) {
			continue;
		}
		if (const std::optional<loc_bucket> bucket = classify_loc(*owner, entry.path)) {
			add_loc(loc, *bucket, entry.line_starts.size() - 1);
		}
	}
	rebuild_module_lookups(module_defs);
	{
		std::lock_guard lock(idx.build_mutex);
		idx.completed_files.emplace();
		idx.completed_files->modules = std::move(module_defs);
		idx.completed_files->files = std::move(files);
		idx.completed_files->content = std::move(content);
		idx.completed_files->loc = loc;
	}
}

auto gse::ide::search::update_file(index_state& idx, const std::filesystem::path& path) -> void {
	const index_root* owner = owning_root(idx.roots, path);
	if (!owner) {
		return;
	}

	std::error_code type_ec;
	const bool regular = std::filesystem::is_regular_file(path, type_ec);
	const auto [resolved, file_identity] = canonical_path_id(path);

	file_entry new_file;
	auto new_content = std::make_shared<content_entry>(content_entry{
		.path = resolved,
	});
	bool has_content = false;
	if (regular) {
		std::filesystem::path relative = resolved.lexically_relative(owner->path);
		std::string relative_name = relative.empty() ? resolved.filename().generic_display_string() : (owner->name + "/" + relative.generic_display_string());
		new_file = {
			.path = resolved,
			.rel = relative_name,
			.rel_lower = to_lower(relative_name),
		};
		const std::string extension = to_lower(resolved.extension().native_encoded_string());
		std::error_code size_ec;
		const std::uintmax_t size = std::filesystem::file_size(resolved, size_ec);
		if (!is_binary_ext(extension) && !size_ec && size <= 2u * 1024u * 1024u) {
			std::ifstream in(resolved, std::ios::binary);
			if (in) {
				new_content->blob.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
				new_content->line_starts = compute_line_starts(new_content->blob);
				has_content = true;
			}
		}
	}

	std::unique_lock lock(idx.mutex);
	if (regular && is_symbol_source(resolved)) {
		idx.pending_symbol_files.insert(file_identity);
	}
	else {
		idx.pending_symbol_files.erase(file_identity);
	}
	if (regular) {
		if (file_entry* indexed_file = idx.files.entries.try_get(file_identity)) {
			*indexed_file = std::move(new_file);
		}
		else {
			idx.files.entries.add(file_identity, std::move(new_file));
		}
	}
	else {
		idx.files.entries.remove(file_identity);
	}

	idx.content.entries.remove(file_identity);
	std::shared_ptr<content_entry> indexed_content;
	if (has_content) {
		indexed_content = std::move(new_content);
		idx.content.entries.add(file_identity, indexed_content);
	}

	if (idx.modules.files.contains(file_identity)) {
		std::erase_if(idx.modules.modules, [file_identity](const module_entry& module) {
			return module.file == file_identity;
		});
		if (!regular) {
			idx.modules.files.erase(file_identity);
		}
	}
	if (indexed_content && is_symbol_source(resolved)) {
		scan_modules(resolved, indexed_content->blob, indexed_content->line_starts, idx.modules);
	}
	rebuild_module_lookups(idx.modules);

	bool symbols_changed = false;
	idx.symbol_overlays.erase(file_identity);
	if (!regular) {
		if (idx.symbols.files.contains(file_identity)) {
			std::erase_if(idx.symbols.symbols, [file_identity](const symbol_entry& symbol) {
				return symbol.file == file_identity;
			});
			idx.symbols.xrefs.erase(file_identity);
			for (auto& refs : idx.symbols.xrefs | std::views::values) {
				std::erase_if(refs, [file_identity](const xref_entry& ref) {
					return ref.def_file == file_identity;
				});
			}
			rebuild_symbol_lookups(idx.symbols);
			idx.symbols.files.erase(file_identity);
			idx.symbol_count.store(idx.symbols.symbols.size(), std::memory_order_release);
			symbols_changed = true;
		}
	}

	loc_counts loc;
	for (const std::shared_ptr<content_entry>& entry_ptr : idx.content.entries.items()) {
		const content_entry& entry = *entry_ptr;
		if (entry.line_starts.empty()) {
			continue;
		}
		const index_root* owner = owning_root(idx.roots, entry.path);
		if (!owner) {
			continue;
		}
		if (const std::optional<loc_bucket> bucket = classify_loc(*owner, entry.path)) {
			add_loc(loc, *bucket, entry.line_starts.size() - 1);
		}
	}
	idx.loc.store(loc);
	publish_file_search_snapshot(idx);
	if (symbols_changed) {
		publish_symbol_search_snapshot(idx);
	}
}

auto gse::ide::search::intern_cached(symbol_index& symbols, interned_file_cache& cache, const std::string& raw) -> file_id {
	if (const auto it = cache.find(raw); it != cache.end()) {
		return it->second;
	}
	auto [canonical, path_identity] = canonical_path_id(raw);
	symbols.files.try_emplace(path_identity, canonical);
	cache.emplace(raw, path_identity);
	cache.try_emplace(canonical.generic_native_encoded_string(), path_identity);
	return path_identity;
}

auto gse::ide::search::make_xref_entry(const file_id definition_file, analysis::symbol_ref ref) -> xref_entry {
	return {
		.line = ref.line > 0 ? ref.line - 1 : 0,
		.column = ref.column > 0 ? ref.column - 1 : 0,
		.length = ref.length,
		.def_file = definition_file,
		.def_line = ref.def_line > 0 ? ref.def_line - 1 : 0,
		.def_column = ref.def_column > 0 ? ref.def_column - 1 : 0,
		.qualified = std::move(ref.qualified),
		.identity = std::move(ref.identity),
		.type = std::move(ref.type),
		.value = std::move(ref.value),
	};
}

auto gse::ide::search::indexed_cached(const std::span<const index_root> roots, const std::string& raw, indexed_path_cache& cache) -> bool {
	if (const auto it = cache.find(raw); it != cache.end()) {
		return it->second;
	}
	const bool indexed = owning_root(roots, raw) != nullptr;
	cache.emplace(raw, indexed);
	return indexed;
}

auto gse::ide::search::file_fingerprint(const std::filesystem::path& file, file_fingerprint_cache& cache) -> std::uint64_t {
	const std::string raw = file.generic_native_encoded_string();
	if (const auto it = cache.find(raw); it != cache.end()) {
		return it->second;
	}
	auto [canonical, file_identity] = canonical_path_id(file);
	const std::string canonical_key = canonical.generic_native_encoded_string();
	if (const auto it = cache.find(canonical_key); it != cache.end()) {
		const std::uint64_t fingerprint = it->second;
		cache.emplace(raw, fingerprint);
		return fingerprint;
	}
	std::uint64_t fingerprint = file_identity.number();
	std::error_code time_ec;
	const std::filesystem::file_time_type modified = std::filesystem::last_write_time(canonical, time_ec);
	fingerprint = hash_combine(fingerprint, time_ec ? 0ull : static_cast<std::uint64_t>(modified.time_since_epoch().count()));
	std::error_code size_ec;
	const std::uintmax_t size = std::filesystem::file_size(canonical, size_ec);
	fingerprint = hash_combine(fingerprint, size_ec ? 0ull : static_cast<std::uint64_t>(size));
	cache.emplace(raw, fingerprint);
	cache.try_emplace(canonical_key, fingerprint);
	return fingerprint;
}

auto gse::ide::search::tu_fingerprint(const analysis::compilation_entry& entry, const std::filesystem::path& plugin, const std::span<const std::filesystem::path> dependencies, file_fingerprint_cache& file_fingerprints) -> std::uint64_t {
	std::uint64_t fingerprint = stable_id("tu_symbol_cache");
	fingerprint = hash_combine(fingerprint, tu_cache_version);
	fingerprint = hash_combine(fingerprint, entry.command.fingerprint);
	fingerprint = hash_combine(fingerprint, file_fingerprint(plugin, file_fingerprints));
	fingerprint = hash_combine(fingerprint, dependencies.size());
	for (const std::filesystem::path& dependency : dependencies) {
		fingerprint = hash_combine(fingerprint, file_fingerprint(dependency, file_fingerprints));
	}
	return fingerprint;
}

auto gse::ide::search::tu_cache_path(const index_state& index, const analysis::compilation_entry& entry) -> std::filesystem::path {
	const std::uint64_t id = hash_combine(canonical_path_id(entry.file).second.number(), entry.command.fingerprint);
	return gse::config::cache_dir() / "symbols" / std::format("{:016x}.bin", id);
}

auto gse::ide::search::save_tu_cache(const std::filesystem::path& path, const analysis::compilation_entry& entry, const std::filesystem::path& plugin, const analysis::tu_symbols& symbols, file_fingerprint_cache& file_fingerprints) -> std::expected<void, std::string> {
	std::vector<std::string> dependencies;
	dependencies.reserve(symbols.dependencies.size());
	for (const std::filesystem::path& dependency : symbols.dependencies) {
		dependencies.push_back(dependency.generic_native_encoded_string());
	}
	const std::uint64_t fingerprint = tu_fingerprint(entry, plugin, symbols.dependencies, file_fingerprints);

	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	if (ec) {
		return std::unexpected(std::format("could not create '{}': {}", path.parent_path().generic_display_string(), ec.message()));
	}
	std::filesystem::path temporary = path;
	temporary += std::format(".{}.tmp", win32::GetCurrentProcessId());
	const auto remove_temporary = make_scope_exit([&] {
		std::error_code remove_ec;
		std::filesystem::remove(temporary, remove_ec);
	});
	std::ofstream out(temporary, std::ios::binary);
	if (!out) {
		return std::unexpected(std::format("could not open temporary cache '{}'", temporary.generic_display_string()));
	}
	binary_writer writer(out, tu_cache_magic, tu_cache_version);
	writer & fingerprint & dependencies & symbols.set;
	out.close();
	if (!out) {
		return std::unexpected(std::format("could not finish writing temporary cache '{}'", temporary.generic_display_string()));
	}
	if (!win32::MoveFileExW(temporary.c_str(), path.c_str(), win32::movefile_replace_existing)) {
		return std::unexpected(std::format("could not replace cache '{}' (Windows error {})", path.generic_display_string(), win32::GetLastError()));
	}
	return {};
}

auto gse::ide::search::load_tu_cache(const std::filesystem::path& path, const analysis::compilation_entry& entry, const std::filesystem::path& plugin, analysis::tu_symbols& out, file_fingerprint_cache& file_fingerprints) -> bool {
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return false;
	}
	binary_reader reader(in);
	std::uint32_t magic = 0;
	std::uint32_t version = 0;
	std::uint32_t epoch = 0;
	reader & magic & version & epoch;
	if (magic != tu_cache_magic || version != tu_cache_version || epoch != archive_format_epoch) {
		return false;
	}
	std::uint64_t fingerprint = 0;
	std::vector<std::string> dependency_strings;
	reader & fingerprint & dependency_strings;
	if (!in) {
		return false;
	}
	std::vector<std::filesystem::path> dependencies;
	dependencies.reserve(dependency_strings.size());
	for (const std::string& dependency : dependency_strings) {
		dependencies.emplace_back(dependency);
	}
	const std::uint64_t expected = tu_fingerprint(entry, plugin, dependencies, file_fingerprints);
	if (expected != fingerprint) {
		return false;
	}
	out.tu = entry.file;
	out.dependencies = std::move(dependencies);
	reader & out.set;
	return static_cast<bool>(in) && out.set.complete;
}

auto gse::ide::search::symbol_worker_loop(const std::stop_token stop, index_state* idx) -> void {
	while (true) {
		{
			std::unique_lock lock(idx->build_mutex);
			while (!idx->build_requested && !idx->build_stop) {
				idx->build_cv.wait(lock);
			}
			if (idx->build_stop) {
				return;
			}
			idx->build_requested = false;
		}
		build_symbols(*idx, stop);
	}
}

gse::ide::search::file_build_result::~file_build_result() = default;

gse::ide::search::index_state::index_state() : current_search_snapshot(std::make_shared<search_snapshot>(search_snapshot{
		.files = std::make_shared<const file_search_snapshot>(file_search_snapshot{
			.files = std::make_shared<const std::vector<file_entry>>(),
			.content = std::make_shared<const std::vector<std::shared_ptr<const content_entry>>>(),
		}),
		.symbols = std::make_shared<const symbol_search_snapshot>(symbol_search_snapshot{
			.symbols = std::make_shared<const std::vector<searchable_symbol>>(),
		}),
		.lints = std::make_shared<const lint_snapshot>(lint_snapshot{
			.sites = std::make_shared<const std::vector<lint_site>>(),
		}),
	})) {}

gse::ide::search::index_state::~index_state() {
	cancel.store(true, std::memory_order_release);
	{
		std::lock_guard lock(build_mutex);
		build_stop = true;
	}
	build_cv.notify_all();
}

auto gse::ide::search::index_state::query_snapshot() const -> std::shared_ptr<const search_snapshot> {
	std::shared_lock lock(mutex);
	return current_search_snapshot;
}

auto gse::ide::search::start_symbol_worker(index_state& idx) -> void {
	idx.build_worker = std::jthread(symbol_worker_loop, &idx);
}

auto gse::ide::search::request_symbol_build(index_state& idx) -> void {
	{
		std::lock_guard lock(idx.build_mutex);
		idx.build_requested = true;
	}
	idx.build_cv.notify_one();
}

auto gse::ide::search::build_symbols(index_state& idx, std::stop_token stop) -> void {
	if (idx.compile_commands.empty() || idx.plugin_dll.empty()) {
		idx.symbols_ready.store(true, std::memory_order_release);
		return;
	}
	bool expected = false;
	if (!idx.building.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
		return;
	}
	begin_index_phase(idx, index_phase::loading_database, idx.compile_commands.size());

	std::vector<std::pair<std::filesystem::path, std::shared_ptr<const analysis::compilation_database>>> databases;
	databases.reserve(idx.compile_commands.size());
	for (const std::filesystem::path& path : idx.compile_commands) {
		if (std::shared_ptr<const analysis::compilation_database> database = analysis::load_compilation_database(path)) {
			databases.emplace_back(path, std::move(database));
		}
		else {
			log::println(log::level::warning, log::category::general, "[symidx] failed to load '{}'", path.generic_display_string());
		}
		idx.progress_done.fetch_add(1, std::memory_order_relaxed);
	}
	if (databases.empty()) {
		idx.symbols_ready.store(true, std::memory_order_release);
		end_index_progress(idx);
		idx.building.store(false, std::memory_order_release);
		return;
	}

	std::size_t entry_count = 0;
	for (const auto& [path, database] : databases) {
		entry_count += database->entries.size();
	}

	std::vector<const analysis::compilation_entry*> tus;
	tus.reserve(entry_count);
	std::unordered_set<std::uint64_t> seen_tus;
	seen_tus.reserve(entry_count);
	begin_index_phase(idx, index_phase::discovering_translation_units, entry_count);
	for (const auto& [path, database] : databases) {
		for (const analysis::compilation_entry& entry : database->entries.items()) {
			const index_root* owner = owning_root(idx.roots, entry.file);
			if (owner && owner->compile_commands == path && seen_tus.insert(canonical_path_id(entry.file).second.number()).second) {
				tus.push_back(&entry);
			}
			idx.progress_done.fetch_add(1, std::memory_order_relaxed);
		}
	}

	file_fingerprint_cache file_fingerprints;
	file_fingerprints.reserve(1024);
	std::vector<analysis::tu_symbols> collected;
	collected.reserve(tus.size());
	std::vector<const analysis::compilation_entry*> pending;
	pending.reserve(tus.size());
	std::size_t cached = 0;
	std::size_t failed = 0;
	begin_index_phase(idx, index_phase::validating_cache, tus.size());
	for (const analysis::compilation_entry* entry : tus) {
		analysis::tu_symbols symbols;
		if (load_tu_cache(tu_cache_path(idx, *entry), *entry, idx.plugin_dll, symbols, file_fingerprints)) {
			collected.push_back(std::move(symbols));
			++cached;
		}
		else {
			pending.push_back(entry);
		}
		idx.progress_done.fetch_add(1, std::memory_order_relaxed);
	}

	const std::size_t round0 = std::max<std::size_t>(2, task::thread_count() / 2);
	std::size_t compiled = 0;
	std::vector<std::filesystem::path> root_paths;
	root_paths.reserve(idx.roots.size());
	std::ranges::copy(idx.roots | std::views::transform(&index_root::path), std::back_inserter(root_paths));
	std::vector<std::tuple<std::filesystem::path, analysis::symbol_index_failure, std::string>> build_failures;
	for (int round = 0; round < 2 && !pending.empty(); ++round) {
		std::vector<analysis::tu_symbols> results = run_symbol_batch(
			{
				.entries = pending,
				.plugin_dll = idx.plugin_dll,
				.roots = root_paths,
				.workers = round == 0 ? round0 : 1,
				.phase = round == 0 ? index_phase::compiling : index_phase::retrying,
			},
			idx,
			stop
		);
		if (idx.cancel.load(std::memory_order_acquire)) {
			end_index_progress(idx);
			idx.building.store(false, std::memory_order_release);
			return;
		}
		begin_index_phase(idx, index_phase::saving_results, results.size());
		std::vector<const analysis::compilation_entry*> next_round;
		for (std::size_t i = 0; i < results.size(); ++i) {
			analysis::tu_symbols& result = results[i];
			if (result.failure == analysis::symbol_index_failure::none && result.set.complete) {
				if (const std::expected<void, std::string> saved = save_tu_cache(tu_cache_path(idx, *pending[i]), *pending[i], idx.plugin_dll, result, file_fingerprints); !saved) {
					log::println(log::level::warning, log::category::general, "[symidx] cache write failed for '{}': {}", pending[i]->file.generic_display_string(), saved.error());
				}
				collected.push_back(std::move(result));
				++compiled;
			}
			else if (round == 0 && result.retryable()) {
				next_round.push_back(pending[i]);
			}
			else {
				const std::string reason = result.failure_detail.empty()
					? std::string(analysis::describe(result.failure))
					: result.failure_detail;
				build_failures.emplace_back(pending[i]->file, result.failure, reason);
				if (result.failure != analysis::symbol_index_failure::module_unavailable) {
					log::println(log::level::warning, log::category::general, "[symidx] semantic compile failed for '{}': {}", pending[i]->file.generic_display_string(), reason);
				}
				++failed;
			}
			idx.progress_done.fetch_add(1, std::memory_order_relaxed);
		}
		pending = std::move(next_round);
	}

	const auto module_failure = std::ranges::find_if(build_failures, [](const auto& failure) {
		return std::get<1>(failure) == analysis::symbol_index_failure::module_unavailable;
	});
	if (module_failure != build_failures.end()) {
		const std::size_t count = static_cast<std::size_t>(std::ranges::count_if(build_failures, [](const auto& failure) {
			return std::get<1>(failure) == analysis::symbol_index_failure::module_unavailable;
		}));
		std::string detail = std::format(
			"{} translation unit{} could not load the current compiled-module graph; first failure: {}",
			count,
			count == 1 ? "" : "s",
			std::get<2>(*module_failure)
		);
		if (detail != idx.reported_partial_index) {
			log::println(log::level::info, log::category::general, "[symidx] publishing partial index: {}", detail);
			idx.reported_partial_index = std::move(detail);
		}
	}
	else {
		idx.reported_partial_index.clear();
	}

	symbol_index local;
	std::size_t symbol_capacity = 0;
	for (const analysis::tu_symbols& tu : collected) {
		symbol_capacity += tu.set.symbols.size();
	}
	local.symbols.reserve(symbol_capacity);
	interned_file_cache raw_file_ids;
	raw_file_ids.reserve(1024);
	indexed_path_cache indexed_paths;
	indexed_paths.reserve(1024);
	local.xrefs.reserve(1024);
	local.params.reserve(1024);
	std::size_t reference_count = 0;
	std::size_t semantic_token_count = 0;
	begin_index_phase(idx, index_phase::merging_records, collected.size());
	for (analysis::tu_symbols& tu : collected) {
		for (analysis::symbol_token& symbol : tu.set.symbols) {
			if (!indexed_cached(idx.roots, symbol.file, indexed_paths)) {
				continue;
			}
			const file_id file = intern_cached(local, raw_file_ids, symbol.file);
			std::string name_lower = to_lower(symbol.name);
			local.symbols.push_back({
				.name = std::move(symbol.name),
				.name_lower = std::move(name_lower),
				.kind = symbol.kind,
				.file = file,
				.line = symbol.line > 0 ? symbol.line - 1 : 0,
				.column = symbol.column > 0 ? symbol.column - 1 : 0,
				.qualified = std::move(symbol.qualified),
				.identity = std::move(symbol.identity),
				.is_definition = symbol.is_definition,
			});
		}
		for (analysis::symbol_ref& ref : tu.set.refs) {
			if (!indexed_cached(idx.roots, ref.file, indexed_paths)) {
				continue;
			}
			const file_id file = intern_cached(local, raw_file_ids, ref.file);
			const file_id definition_file = intern_cached(local, raw_file_ids, ref.def_file);
			local.xrefs[file].push_back(make_xref_entry(definition_file, std::move(ref)));
			++reference_count;
		}
		for (const analysis::param_token& param : tu.set.params) {
			if (!indexed_cached(idx.roots, param.file, indexed_paths)) {
				continue;
			}
			const file_id file = intern_cached(local, raw_file_ids, param.file);
			local.params[file].push_back({
				.line = param.line > 0 ? param.line - 1 : 0,
				.column = param.column > 0 ? param.column - 1 : 0,
				.length = param.length,
				.kind = param.kind,
			});
			++semantic_token_count;
		}
		for (lint_finding& finding : lint::findings({
			.quals = tu.set.quals,
			.template_args = tu.set.template_args,
			.unused_locals = tu.set.unused_locals,
		})) {
			if (!indexed_cached(idx.roots, finding.file, indexed_paths)) {
				continue;
			}
			local.lints.push_back({
				.file = intern_cached(local, raw_file_ids, finding.file),
				.rule = finding.rule,
				.edit = std::move(finding.edit),
			});
		}
		idx.progress_done.fetch_add(1, std::memory_order_relaxed);
	}
	begin_index_phase(idx, index_phase::building_lookups, local.symbols.size());
	build_symbol_lookups(local, &idx.progress_done);
	begin_index_phase(idx, index_phase::sorting_locations, local.symbols_by_file.size() + local.xrefs.size());
	sort_symbol_locations(local, &idx.progress_done);
	for (auto& [path, failure, reason] : build_failures) {
		const file_id file = local.file_for(path);
		local.failures.emplace(file, std::move(reason));
	}

	log::println(
		log::level::info,
		log::category::general,
		"[symidx] {} TUs, {} cached, {} compiled, {} failed, {} symbols, {} references, {} semantic tokens, {} files, {} indexed paths",
		tus.size(),
		cached,
		compiled,
		failed,
		local.symbols.size(),
		reference_count,
		semantic_token_count,
		local.files.size(),
		indexed_paths.size()
	);

	{
		std::lock_guard lock(idx.build_mutex);
		idx.completed_symbols.emplace(std::move(local));
	}
	begin_index_phase(idx, index_phase::publishing, 1);
}

auto gse::ide::search::apply_symbol_overlay(symbol_index& index, const symbol_overlay& overlay, const bool rebuild_lookups) -> void {
	const file_id fid = index.file_for(overlay.path);
	interned_file_cache local_fid;
	local_fid.reserve(64);
	index.failures.erase(fid);

	std::erase_if(index.symbols, [fid](const symbol_entry& entry) {
		return entry.file == fid;
	});
	for (const analysis::symbol_token& symbol : overlay.symbols) {
		if (intern_cached(index, local_fid, symbol.file) != fid) {
			continue;
		}
		index.symbols.push_back({
			.name = symbol.name,
			.name_lower = to_lower(symbol.name),
			.kind = symbol.kind,
			.file = fid,
			.line = symbol.line > 0 ? symbol.line - 1 : 0,
			.column = symbol.column > 0 ? symbol.column - 1 : 0,
			.qualified = symbol.qualified,
			.identity = symbol.identity,
			.is_definition = symbol.is_definition,
		});
	}

	std::vector<xref_entry>& file_xrefs = index.xrefs[fid];
	file_xrefs.clear();
	for (const analysis::symbol_ref& ref : overlay.refs) {
		if (intern_cached(index, local_fid, ref.file) != fid) {
			continue;
		}
		file_xrefs.push_back(make_xref_entry(intern_cached(index, local_fid, ref.def_file), ref));
	}

	std::vector<positioned_kind>& file_params = index.params[fid];
	file_params.clear();
	for (const analysis::param_token& param : overlay.params) {
		if (intern_cached(index, local_fid, param.file) != fid) {
			continue;
		}
		file_params.push_back({
			.line = param.line > 0 ? param.line - 1 : 0,
			.column = param.column > 0 ? param.column - 1 : 0,
			.length = param.length,
			.kind = param.kind,
		});
	}
	if (rebuild_lookups) {
		build_symbol_lookups(index, nullptr);
		for (auto& ids : index.symbols_by_file | std::views::values) {
			sort_symbol_ids(index, ids);
		}
		sort_xrefs(file_xrefs);
	}
}

auto gse::ide::search::index_state::merge_file_symbols(const std::filesystem::path& file, const std::span<const analysis::symbol_token> syms, const std::span<const analysis::symbol_ref> refs, const std::span<const analysis::param_token> params) -> void {
	std::unique_lock lock(mutex);
	const auto [canonical, fid] = canonical_path_id(file);
	symbol_overlay& overlay = symbol_overlays[fid];
	overlay.path = canonical;
	overlay.symbols.assign(syms.begin(), syms.end());
	overlay.refs.assign(refs.begin(), refs.end());
	overlay.params.assign(params.begin(), params.end());
	apply_symbol_overlay(symbols, overlay, true);
	pending_symbol_files.erase(fid);
	symbol_count.store(symbols.symbols.size(), std::memory_order_release);
	publish_symbol_search_snapshot(*this);
}

auto gse::ide::search::publish_file_build(index_state& idx) -> void {
	std::optional<file_build_result> completed;
	{
		std::lock_guard lock(idx.build_mutex);
		if (!idx.completed_files) {
			return;
		}
		completed.emplace(std::move(*idx.completed_files));
		idx.completed_files.reset();
	}

	{
		std::unique_lock lock(idx.mutex);
		idx.modules = std::move(completed->modules);
		idx.files.entries = std::move(completed->files);
		idx.content.entries = std::move(completed->content);
		idx.loc.store(completed->loc);
		publish_file_search_snapshot(idx);
	}
	idx.files.loaded.store(true, std::memory_order_release);
	idx.content.loaded.store(true, std::memory_order_release);
}

auto gse::ide::search::publish_symbol_build(index_state& idx) -> void {
	std::optional<symbol_index> completed;
	{
		std::lock_guard lock(idx.build_mutex);
		if (idx.completed_symbols) {
			completed.emplace(std::move(*idx.completed_symbols));
			idx.completed_symbols.reset();
		}
		else {
			return;
		}
	}

	{
		std::unique_lock lock(idx.mutex);
		if (!idx.symbol_overlays.empty()) {
			for (const symbol_overlay& overlay : idx.symbol_overlays | std::views::values) {
				apply_symbol_overlay(*completed, overlay, false);
			}
			rebuild_symbol_lookups(*completed);
		}
		idx.symbols = std::move(*completed);
		idx.pending_symbol_files.clear();
		idx.symbol_count.store(idx.symbols.symbols.size(), std::memory_order_release);
		publish_symbol_search_snapshot(idx);
	}
	idx.symbols_ready.store(true, std::memory_order_release);
	idx.progress_done.store(1, std::memory_order_release);
	end_index_progress(idx);
	idx.building.store(false, std::memory_order_release);
}
