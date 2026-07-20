export module gse.ide.search:index;

import std;
import gse;
import gse.ide.analysis;

import :types;

export namespace gse::ide::search {
	struct file_index {
		gse::id_mapped_collection<file_entry> entries;
		std::atomic<bool> loaded = false;
	};

	struct content_entry {
		std::filesystem::path path;
		std::string blob;
		std::vector<std::uint32_t> line_starts;
	};

	struct content_index {
		gse::id_mapped_collection<content_entry> entries;
		std::atomic<bool> loaded = false;
	};

	struct module_entry {
		std::string name;
		file_id file = 0;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
	};

	struct module_index {
		module_index() = default;
		module_index(const module_index&) = delete;
		auto operator=(const module_index&) -> module_index& = delete;
		module_index(module_index&& other) noexcept;
		auto operator=(module_index&& other) noexcept -> module_index&;

		std::vector<module_entry> modules;
		std::vector<std::filesystem::path> files;
		std::unordered_map<gse::id, file_id> file_ids;
		std::unordered_map<std::string, std::vector<std::uint32_t>, gse::transparent_hash, gse::transparent_equal> modules_by_name;
		std::unordered_map<file_id, std::vector<std::uint32_t>> modules_by_file;

		auto file_for(const std::filesystem::path& path) -> file_id;
		auto path_for(file_id id) const -> std::filesystem::path;
		auto transfer_from(module_index& other) -> void;
	};

	struct symbol_index {
		symbol_index() = default;
		symbol_index(const symbol_index&) = delete;
		auto operator=(const symbol_index&) -> symbol_index& = delete;
		symbol_index(symbol_index&& other) noexcept;
		auto operator=(symbol_index&& other) noexcept -> symbol_index&;

		std::vector<symbol_entry> symbols;
		std::unordered_map<file_id, std::vector<xref_entry>> xrefs;
		std::vector<std::filesystem::path> files;
		std::unordered_map<gse::id, file_id> file_ids;
		std::vector<analysis::channel_use> channels;
		std::unordered_map<file_id, std::vector<std::uint32_t>> symbols_by_file;
		std::unordered_map<std::string, std::vector<std::uint32_t>, gse::transparent_hash, gse::transparent_equal> symbols_by_name;
		std::unordered_map<std::string, std::vector<std::uint32_t>, gse::transparent_hash, gse::transparent_equal> definitions_by_identity;
		std::unordered_map<std::string, std::vector<std::uint32_t>, gse::transparent_hash, gse::transparent_equal> definitions_by_qualified;

		auto file_for(const std::filesystem::path& path) -> file_id;
		auto path_for(file_id id) const -> std::filesystem::path;
		auto transfer_from(symbol_index& other) -> void;
	};

	struct hover_hit {
		location def;
		std::string qualified;
		std::string kind;
	};

	enum class index_phase : std::uint8_t {
		idle,
		scanning,
		compiling,
		retrying,
		aggregating,
	};

	struct index_state {
		symbol_index symbols;
		module_index modules;
		file_index files;
		content_index content;
		std::filesystem::path compile_commands;
		std::filesystem::path plugin_dll;
		std::filesystem::path workspace_root;
		std::atomic<bool> symbols_ready = false;
		std::atomic<bool> building = false;
		std::atomic<std::uint64_t> cpp_loc = 0;
		std::atomic<std::size_t> tus_total = 0;
		std::atomic<std::size_t> tus_done = 0;
		std::atomic<std::size_t> symbol_count = 0;
		std::atomic<index_phase> phase = index_phase::idle;
		std::atomic<bool> cancel = false;
		mutable std::shared_mutex mutex;
		std::mutex build_mutex;
		std::condition_variable build_cv;
		bool build_requested = false;
		bool build_stop = false;
		std::jthread build_worker;

		~index_state();

		auto definition_at(const std::filesystem::path& file, std::uint32_t line, std::uint32_t column) const -> std::optional<location>;
		auto symbol_at(const std::filesystem::path& file, std::uint32_t line, std::uint32_t column) const -> std::optional<hover_hit>;
		auto symbol_definition(std::string_view name, std::string_view qualifier, const std::filesystem::path& click_file) const -> std::optional<location>;
		auto module_definition(std::string_view name, const std::filesystem::path& click_file) const -> std::optional<location>;
		auto declaration_of(std::string_view name, std::string_view qualifier) const -> std::optional<hover_hit>;
		auto semantic_kind_of(std::string_view name) const -> std::optional<analysis::semantic_kind>;
		auto channel_links() const -> std::vector<analysis::channel_use>;
		auto merge_file_symbols(const std::filesystem::path& file, std::span<const analysis::symbol_token> syms, std::span<const analysis::symbol_ref> refs) -> void;
	};

	struct index_merge_request {
		std::filesystem::path path;
		std::shared_ptr<analysis::diagnostics_check> check;
	};

	struct index_file_update_request {
		std::filesystem::path path;
	};

	auto build_files_and_content(index_state& idx, const std::filesystem::path& root) -> void;
	auto build_symbols(index_state& idx) -> void;
	auto start_symbol_worker(index_state& idx) -> void;
	auto request_symbol_build(index_state& idx) -> void;
	auto update_file(index_state& idx, const std::filesystem::path& path) -> void;
	auto is_indexed_path(const std::filesystem::path& root, const std::filesystem::path& path) -> bool;
}

namespace gse::ide::search {
	auto is_skipped_dir(std::string_view name) -> bool {
		return name == "out" || name == ".git" || name == ".vs" || name == ".vscode" || name == ".claude"
			|| name == "external" || name == "vcpkg" || name == ".gcc-ci-build"
			|| name == "build" || name == "node_modules" || name == ".cache";
	}

	auto is_binary_ext(std::string_view ext) -> bool {
		static constexpr std::string_view list[] = {
			".exe", ".dll", ".lib", ".obj", ".o", ".a", ".pdb", ".ilk", ".gcm", ".bmi", ".ifc",
			".png", ".jpg", ".jpeg", ".bmp", ".gif", ".ico", ".tga", ".dds", ".ktx",
			".ttf", ".otf", ".woff", ".woff2",
			".zip", ".7z", ".gz", ".tar", ".rar",
			".bin", ".dat", ".pack", ".idx", ".spv", ".wav", ".mp3", ".ogg", ".mp4", ".fbx", ".glb"
		};
		return std::ranges::find(list, ext) != std::ranges::end(list);
	}

	auto is_cpp_ext(std::string_view ext) -> bool {
		static constexpr std::string_view list[] = {
			".cpp", ".cppm", ".cc", ".cxx", ".ixx", ".c",
			".h", ".hpp", ".hh", ".hxx", ".inl"
		};
		return std::ranges::find(list, ext) != std::ranges::end(list);
	}

	auto is_counted_source_dir(std::string_view name) -> bool {
		return name == "Editor" || name == "Engine" || name == "Game" || name == "Server";
	}

	auto compute_line_starts(std::string_view blob) -> std::vector<std::uint32_t> {
		std::vector<std::uint32_t> starts;
		starts.push_back(0);
		for (std::size_t i = 0; i < blob.size(); ++i) {
			if (blob[i] == '\n') {
				starts.push_back(static_cast<std::uint32_t>(i + 1));
			}
		}
		return starts;
	}

	auto canonical_path_id(const std::filesystem::path& path) -> std::pair<std::filesystem::path, gse::id> {
		std::error_code ec;
		std::filesystem::path canon = std::filesystem::weakly_canonical(path, ec);
		if (ec) {
			canon = path;
		}
		return { canon, gse::generate_temp_id(canon) };
	}

	auto module_ident_start(const char ch) -> bool {
		return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
	}

	auto module_ident_continue(const char ch) -> bool {
		return module_ident_start(ch) || (ch >= '0' && ch <= '9');
	}

	auto skip_module_ws(std::string_view text, std::size_t pos) -> std::size_t {
		while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) {
			++pos;
		}
		return pos;
	}

	auto consume_module_keyword(std::string_view text, std::size_t& pos, std::string_view keyword) -> bool {
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

	auto parse_module_name(std::string_view text, std::size_t pos) -> std::optional<std::pair<std::string, std::uint32_t>> {
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

	auto line_at(std::string_view blob, const std::vector<std::uint32_t>& starts, std::size_t line) -> std::string_view {
		const std::size_t start = starts[line];
		std::size_t end = line + 1 < starts.size() ? starts[line + 1] : blob.size();
		while (end > start && (blob[end - 1] == '\n' || blob[end - 1] == '\r')) {
			--end;
		}
		return blob.substr(start, end - start);
	}

	auto parse_module_declaration(std::string_view line) -> std::optional<std::pair<std::string, std::uint32_t>> {
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

	auto scan_modules(const std::filesystem::path& path, std::string_view blob, const std::vector<std::uint32_t>& starts, module_index& modules) -> void {
		if (starts.empty()) {
			return;
		}
		const file_id fid = modules.file_for(path);
		for (std::size_t line = 0; line < starts.size(); ++line) {
			if (const std::optional<std::pair<std::string, std::uint32_t>> decl = parse_module_declaration(line_at(blob, starts, line))) {
				modules.modules.push_back({
					.name = decl->first,
					.file = fid,
					.line = static_cast<std::uint32_t>(line),
					.column = decl->second,
				});
			}
		}
	}

	auto symbol_location(const symbol_index& index, const symbol_entry& s) -> location {
		return location{ .path = index.path_for(s.file), .line = s.line, .column = s.column };
	}

	auto rebuild_module_lookups(module_index& index) -> void {
		index.modules_by_name.clear();
		index.modules_by_file.clear();
		for (std::uint32_t i = 0; i < index.modules.size(); ++i) {
			index.modules_by_name[index.modules[i].name].push_back(i);
			index.modules_by_file[index.modules[i].file].push_back(i);
		}
	}

	auto rebuild_symbol_lookups(symbol_index& index) -> void {
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
		}
		for (auto& ids : index.symbols_by_file | std::views::values) {
			std::ranges::sort(ids, [&](const std::uint32_t lhs, const std::uint32_t rhs) {
				const symbol_entry& a = index.symbols[lhs];
				const symbol_entry& b = index.symbols[rhs];
				return std::tie(a.line, a.column) < std::tie(b.line, b.column);
			});
		}
		for (auto& refs : index.xrefs | std::views::values) {
			std::ranges::sort(refs, [](const xref_entry& a, const xref_entry& b) {
				return std::tie(a.line, a.column, a.length) < std::tie(b.line, b.column, b.length);
			});
		}
	}

	auto xref_at(const symbol_index& index, const file_id file, const std::uint32_t line, const std::uint32_t column) -> const xref_entry* {
		const auto file_it = index.xrefs.find(file);
		if (file_it == index.xrefs.end()) {
			return nullptr;
		}
		const std::vector<xref_entry>& refs = file_it->second;
		auto it = std::ranges::lower_bound(refs, line, {}, &xref_entry::line);
		for (; it != refs.end() && it->line == line; ++it) {
			if (column >= it->column && column < it->column + it->length) {
				return &*it;
			}
		}
		return nullptr;
	}

	auto symbol_at_location(const symbol_index& index, const file_id file, const std::uint32_t line, const std::uint32_t column, const bool contains_column) -> const symbol_entry* {
		const auto file_it = index.symbols_by_file.find(file);
		if (file_it == index.symbols_by_file.end()) {
			return nullptr;
		}
		const std::vector<std::uint32_t>& ids = file_it->second;
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

	auto matching_function_definition(const symbol_index& index, std::string_view identity, std::string_view qualified, const file_id from_file, const std::uint32_t from_line, const std::uint32_t from_column) -> std::optional<location> {
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
			if (symbol.kind == analysis::symbol_kind::function && symbol.is_definition && (symbol.file != from_file || symbol.line != from_line || symbol.column != from_column)) {
				return symbol_location(index, symbol);
			}
		}
		return std::nullopt;
	}

	auto promoted_definition(const symbol_index& index, const file_id file, const std::uint32_t line, const std::uint32_t column, std::string_view identity, std::string_view qualified) -> location {
		const location target{ .path = index.path_for(file), .line = line, .column = column };
		if (const symbol_entry* symbol = symbol_at_location(index, file, line, column, false)) {
			if (symbol->kind == analysis::symbol_kind::function && !symbol->is_definition) {
				return matching_function_definition(index, symbol->identity.empty() ? identity : std::string_view(symbol->identity), symbol->qualified.empty() ? qualified : std::string_view(symbol->qualified), file, line, column).value_or(target);
			}
			return target;
		}
		return matching_function_definition(index, identity, qualified, file, line, column).value_or(target);
	}

	auto run_symbol_batch(std::span<const analysis::compilation_entry* const> list, const std::filesystem::path& plugin_dll, const std::filesystem::path& root, std::size_t workers, std::atomic<std::size_t>* progress, const std::atomic<bool>* cancel) -> std::vector<analysis::tu_symbols> {
		std::vector<analysis::tu_symbols> out(list.size());
		std::atomic<std::size_t> next = 0;
		std::vector<std::thread> pool;
		const std::size_t n = std::min(workers, list.size());
		for (std::size_t w = 0; w < n; ++w) {
			pool.emplace_back([&out, &next, list, &plugin_dll, &root, w, progress, cancel] {
				while (true) {
					if (cancel && cancel->load(std::memory_order_acquire)) {
						break;
					}
					const std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
					if (i >= list.size()) {
						break;
					}
					out[i] = analysis::symbol_index_builder::run_one(*list[i], plugin_dll, root, static_cast<std::uint32_t>(w));
					if (progress) {
						progress->fetch_add(1, std::memory_order_relaxed);
					}
				}
			});
		}
		for (std::thread& t : pool) {
			t.join();
		}
		return out;
	}
}

auto gse::ide::search::is_indexed_path(const std::filesystem::path& root, const std::filesystem::path& path) -> bool {
	const std::filesystem::path relative = path.lexically_normal().lexically_relative(root.lexically_normal());
	if (relative.empty() || (!relative.empty() && *relative.begin() == "..")) {
		return false;
	}
	for (const std::filesystem::path& part : relative) {
		if (is_skipped_dir(to_lower(part.native_encoded_string()))) {
			return false;
		}
	}
	return true;
}

gse::ide::search::module_index::module_index(module_index&& other) noexcept {
	transfer_from(other);
}

auto gse::ide::search::module_index::operator=(module_index&& other) noexcept -> module_index& {
	if (this != &other) {
		transfer_from(other);
	}
	return *this;
}

auto gse::ide::search::module_index::transfer_from(module_index& other) -> void {
	modules.clear();
	files.clear();
	file_ids.clear();
	modules_by_name.clear();
	modules_by_file.clear();
	modules.swap(other.modules);
	files.swap(other.files);
	file_ids.swap(other.file_ids);
	modules_by_name.swap(other.modules_by_name);
	modules_by_file.swap(other.modules_by_file);
}

gse::ide::search::symbol_index::symbol_index(symbol_index&& other) noexcept {
	transfer_from(other);
}

auto gse::ide::search::symbol_index::operator=(symbol_index&& other) noexcept -> symbol_index& {
	if (this != &other) {
		transfer_from(other);
	}
	return *this;
}

auto gse::ide::search::symbol_index::transfer_from(symbol_index& other) -> void {
	symbols.clear();
	xrefs.clear();
	files.clear();
	file_ids.clear();
	channels.clear();
	symbols_by_file.clear();
	symbols_by_name.clear();
	definitions_by_identity.clear();
	definitions_by_qualified.clear();
	symbols.swap(other.symbols);
	xrefs.swap(other.xrefs);
	files.swap(other.files);
	file_ids.swap(other.file_ids);
	channels.swap(other.channels);
	symbols_by_file.swap(other.symbols_by_file);
	symbols_by_name.swap(other.symbols_by_name);
	definitions_by_identity.swap(other.definitions_by_identity);
	definitions_by_qualified.swap(other.definitions_by_qualified);
}

auto gse::ide::search::module_index::file_for(const std::filesystem::path& path) -> file_id {
	auto [canon, canonical_id] = canonical_path_id(path);
	if (const auto it = file_ids.find(canonical_id); it != file_ids.end()) {
		return it->second;
	}
	const file_id id = static_cast<file_id>(files.size());
	files.push_back(std::move(canon));
	file_ids.emplace(canonical_id, id);
	return id;
}

auto gse::ide::search::module_index::path_for(file_id id) const -> std::filesystem::path {
	if (id < files.size()) {
		return files[id];
	}
	return {};
}

auto gse::ide::search::symbol_index::file_for(const std::filesystem::path& path) -> file_id {
	auto [canon, canonical_id] = canonical_path_id(path);
	if (const auto it = file_ids.find(canonical_id); it != file_ids.end()) {
		return it->second;
	}
	const file_id id = static_cast<file_id>(files.size());
	files.push_back(std::move(canon));
	file_ids.emplace(canonical_id, id);
	return id;
}

auto gse::ide::search::symbol_index::path_for(file_id id) const -> std::filesystem::path {
	if (id < files.size()) {
		return files[id];
	}
	return {};
}

auto gse::ide::search::index_state::definition_at(const std::filesystem::path& file, std::uint32_t line, std::uint32_t column) const -> std::optional<location> {
	std::shared_lock lock(mutex);
	const auto id_it = symbols.file_ids.find(gse::generate_temp_id(file));
	if (id_it == symbols.file_ids.end()) {
		return std::nullopt;
	}
	const file_id file_id = id_it->second;
	if (const xref_entry* xref = xref_at(symbols, file_id, line, column)) {
		return promoted_definition(symbols, xref->def_file, xref->def_line, xref->def_column, xref->identity, xref->qualified);
	}
	if (const symbol_entry* symbol = symbol_at_location(symbols, file_id, line, column, true)) {
		if (symbol->kind == analysis::symbol_kind::function && !symbol->is_definition) {
			return matching_function_definition(symbols, symbol->identity, symbol->qualified, file_id, symbol->line, symbol->column);
		}
	}
	return std::nullopt;
}

auto gse::ide::search::index_state::symbol_at(const std::filesystem::path& file, std::uint32_t line, std::uint32_t column) const -> std::optional<hover_hit> {
	std::shared_lock lock(mutex);
	const auto id_it = symbols.file_ids.find(gse::generate_temp_id(file));
	if (id_it == symbols.file_ids.end()) {
		return std::nullopt;
	}
	const xref_entry* xref = xref_at(symbols, id_it->second, line, column);
	if (!xref) {
		return std::nullopt;
	}
	std::string kind;
	if (const symbol_entry* symbol = symbol_at_location(symbols, xref->def_file, xref->def_line, xref->def_column, false)) {
		kind = std::format("{}", symbol->kind);
	}
	return hover_hit{
		.def = promoted_definition(symbols, xref->def_file, xref->def_line, xref->def_column, xref->identity, xref->qualified),
		.qualified = xref->qualified,
		.kind = kind,
	};
}

auto gse::ide::search::index_state::symbol_definition(std::string_view name, std::string_view qualifier, const std::filesystem::path& click_file) const -> std::optional<location> {
	std::string needle;
	needle.reserve(qualifier.size() + name.size());
	needle.append(qualifier);
	needle.append(name);
	std::string suffix = "::";
	suffix.append(needle);
	std::shared_lock lock(mutex);
	file_id click_fid = static_cast<file_id>(-1);
	if (const auto it = symbols.file_ids.find(gse::generate_temp_id(click_file)); it != symbols.file_ids.end()) {
		click_fid = it->second;
	}
	const auto candidates = symbols.symbols_by_name.find(name);
	if (candidates == symbols.symbols_by_name.end()) {
		return std::nullopt;
	}
	const symbol_entry* best = nullptr;
	int best_score = 0;
	for (const std::uint32_t id : candidates->second) {
		const symbol_entry& s = symbols.symbols[id];
		if (!analysis::is_definition_kind(s.kind)) {
			continue;
		}
		int score = 1;
		if (s.qualified == needle) {
			score = 5;
		}
		else if (!qualifier.empty() && s.qualified.ends_with(suffix)) {
			score = 4;
		}
		else if (s.file == click_fid) {
			score = 3;
		}
		else if (analysis::is_type_kind(s.kind)) {
			score = 2;
		}
		if (s.kind == analysis::symbol_kind::function && s.is_definition) {
			score += 3;
		}
		if (score > best_score) {
			best_score = score;
			best = &s;
		}
	}
	if (!best) {
		return std::nullopt;
	}
	if (!qualifier.empty() && best_score < 4) {
		return std::nullopt;
	}
	if (qualifier.empty() && best_score <= 1) {
		return std::nullopt;
	}
	return location{ .path = symbols.path_for(best->file), .line = best->line, .column = best->column };
}

auto gse::ide::search::index_state::module_definition(std::string_view name, const std::filesystem::path& click_file) const -> std::optional<location> {
	if (name.empty()) {
		return std::nullopt;
	}
	std::string target(name);
	const gse::id file_identity = gse::generate_temp_id(click_file);
	std::shared_lock lock(mutex);
	if (target.starts_with(':')) {
		const auto file_it = modules.file_ids.find(file_identity);
		if (file_it == modules.file_ids.end()) {
			return std::nullopt;
		}
		std::string current;
		if (const auto ids = modules.modules_by_file.find(file_it->second); ids != modules.modules_by_file.end()) {
			for (const std::uint32_t id : ids->second) {
				current = modules.modules[id].name;
				if (!current.empty()) {
					break;
				}
			}
		}
		if (current.empty()) {
			return std::nullopt;
		}
		if (const std::size_t colon = current.find(':'); colon != std::string::npos) {
			current.erase(colon);
		}
		target.insert(0, current);
	}

	const auto candidates = modules.modules_by_name.find(target);
	if (candidates == modules.modules_by_name.end()) {
		return std::nullopt;
	}
	const module_entry* best = nullptr;
	for (const std::uint32_t id : candidates->second) {
		const module_entry& m = modules.modules[id];
		if (!best || modules.path_for(m.file).extension() == ".cppm") {
			best = &m;
		}
	}
	if (!best) {
		return std::nullopt;
	}
	return location{ .path = modules.path_for(best->file), .line = best->line, .column = best->column };
}

auto gse::ide::search::index_state::declaration_of(std::string_view name, std::string_view qualifier) const -> std::optional<hover_hit> {
	std::string needle;
	needle.append(qualifier);
	needle.append(name);
	std::string suffix = "::";
	suffix.append(needle);
	std::shared_lock lock(mutex);
	const auto candidates = symbols.symbols_by_name.find(name);
	if (candidates == symbols.symbols_by_name.end()) {
		return std::nullopt;
	}
	const symbol_entry* best = nullptr;
	const symbol_entry* fallback = nullptr;
	int best_score = 0;
	for (const std::uint32_t id : candidates->second) {
		const symbol_entry& s = symbols.symbols[id];
		if (!analysis::is_definition_kind(s.kind)) {
			continue;
		}
		if (!fallback || s.qualified.size() < fallback->qualified.size()) {
			fallback = &s;
		}
		int score = 0;
		if (s.qualified == needle) {
			score = 3;
		}
		else if (!qualifier.empty() && s.qualified.ends_with(suffix)) {
			score = 2;
		}
		if (score > best_score) {
			best_score = score;
			best = &s;
		}
	}
	const symbol_entry* pick = best ? best : fallback;
	if (!pick) {
		return std::nullopt;
	}
	return hover_hit{ .def = location{ .path = symbols.path_for(pick->file), .line = pick->line, .column = pick->column }, .qualified = pick->qualified, .kind = std::string(gse::enum_to_string(pick->kind)) };
}

auto gse::ide::search::index_state::semantic_kind_of(const std::string_view name) const -> std::optional<analysis::semantic_kind> {
	std::shared_lock lock(mutex);
	const auto candidates = symbols.symbols_by_name.find(name);
	if (candidates == symbols.symbols_by_name.end()) {
		return std::nullopt;
	}
	const symbol_entry* pick = nullptr;
	for (const std::uint32_t id : candidates->second) {
		const symbol_entry& s = symbols.symbols[id];
		if (analysis::is_definition_kind(s.kind)) {
			pick = &s;
			break;
		}
		if (!pick) {
			pick = &s;
		}
	}
	if (!pick) {
		return std::nullopt;
	}
	switch (pick->kind) {
		case analysis::symbol_kind::variable: return analysis::semantic_kind::variable;
		case analysis::symbol_kind::parameter: return analysis::semantic_kind::parameter;
		case analysis::symbol_kind::function: return analysis::semantic_kind::function;
		case analysis::symbol_kind::member: return analysis::semantic_kind::member;
		case analysis::symbol_kind::type: return analysis::semantic_kind::type;
		case analysis::symbol_kind::enum_member: return analysis::semantic_kind::enum_member;
		case analysis::symbol_kind::name_space: return analysis::semantic_kind::name_space;
		case analysis::symbol_kind::enumeration: return analysis::semantic_kind::type;
		case analysis::symbol_kind::alias: return analysis::semantic_kind::type;
		case analysis::symbol_kind::concept_decl: return analysis::semantic_kind::type;
	}
	return std::nullopt;
}

auto gse::ide::search::index_state::channel_links() const -> std::vector<analysis::channel_use> {
	std::shared_lock lock(mutex);
	return symbols.channels;
}

auto gse::ide::search::build_files_and_content(index_state& idx, const std::filesystem::path& root) -> void {
	gse::id_mapped_collection<file_entry> files;
	gse::id_mapped_collection<content_entry> content;

	std::error_code ec;
	const auto opts = std::filesystem::directory_options::skip_permission_denied;
	for (auto it = std::filesystem::recursive_directory_iterator(root, opts, ec); it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
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
		std::string rel = std::filesystem::relative(path, root, type_ec).generic_display_string();
		if (rel.empty()) {
			rel = path.filename().display_string();
		}
		const gse::id file_identity = gse::generate_temp_id(path);
		files.add(file_identity, {
			.path = path,
			.rel = rel,
			.rel_lower = to_lower(rel),
		});

		const std::string ext = to_lower(path.extension().native_encoded_string());
		if (!is_binary_ext(ext)) {
			const auto size = entry.file_size(type_ec);
			if (!type_ec && size <= 2u * 1024u * 1024u) {
				content.add(file_identity, {
					.path = path,
				});
			}
		}
	}

	const std::span<content_entry> content_entries = content.items();

	gse::task::coarse_parallel(content_entries.size(), 8, [&](std::size_t i) {
		content_entry& entry = content_entries[i];
		std::ifstream in(entry.path, std::ios::binary);
		if (!in) {
			return;
		}
		std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		entry.line_starts = compute_line_starts(data);
		entry.blob = std::move(data);
	});

	std::uint64_t loc = 0;
	module_index module_defs;
	for (const content_entry& entry : content_entries) {
		if (entry.line_starts.empty() || !is_cpp_ext(to_lower(entry.path.extension().native_encoded_string()))) {
			continue;
		}
		scan_modules(entry.path, entry.blob, entry.line_starts, module_defs);
		std::error_code rel_ec;
		const std::filesystem::path rel = std::filesystem::relative(entry.path, root, rel_ec);
		if (rel_ec || rel.empty() || !is_counted_source_dir(rel.begin()->display_string())) {
			continue;
		}
		loc += entry.line_starts.size() - 1;
	}
	rebuild_module_lookups(module_defs);
	idx.cpp_loc.store(loc, std::memory_order_release);

	{
		std::unique_lock lock(idx.mutex);
		idx.modules = std::move(module_defs);
		idx.files.entries = std::move(files);
		idx.content.entries = std::move(content);
	}
	idx.files.loaded.store(true, std::memory_order_release);
	idx.content.loaded.store(true, std::memory_order_release);
}

auto gse::ide::search::update_file(index_state& idx, const std::filesystem::path& path) -> void {
	if (!is_indexed_path(idx.workspace_root, path)) {
		return;
	}

	std::error_code type_ec;
	const bool regular = std::filesystem::is_regular_file(path, type_ec);
	std::filesystem::path resolved = path.lexically_normal();
	if (regular) {
		std::error_code canonical_ec;
		const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, canonical_ec);
		if (!canonical_ec) {
			resolved = canonical;
		}
	}
	const gse::id file_identity = gse::generate_temp_id(resolved);

	file_entry new_file;
	content_entry new_content{
		.path = resolved,
	};
	bool has_content = false;
	if (regular) {
		std::filesystem::path relative = resolved.lexically_relative(idx.workspace_root);
		std::string relative_name = relative.empty() ? resolved.filename().display_string() : relative.generic_display_string();
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
				new_content.blob.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
				new_content.line_starts = compute_line_starts(new_content.blob);
				has_content = true;
			}
		}
	}

	std::unique_lock lock(idx.mutex);
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
	content_entry* indexed_content = nullptr;
	if (has_content) {
		indexed_content = idx.content.entries.add(file_identity, std::move(new_content));
	}

	if (const auto file = idx.modules.file_ids.find(file_identity); file != idx.modules.file_ids.end()) {
		const file_id id = file->second;
		std::erase_if(idx.modules.modules, [id](const module_entry& module) {
			return module.file == id;
		});
	}
	if (regular && indexed_content && is_cpp_ext(to_lower(resolved.extension().native_encoded_string()))) {
		scan_modules(resolved, indexed_content->blob, indexed_content->line_starts, idx.modules);
	}
	rebuild_module_lookups(idx.modules);

	if (!regular) {
		if (const auto file = idx.symbols.file_ids.find(file_identity); file != idx.symbols.file_ids.end()) {
			const file_id id = file->second;
			std::erase_if(idx.symbols.symbols, [id](const symbol_entry& symbol) {
				return symbol.file == id;
			});
			idx.symbols.xrefs.erase(id);
			for (auto& refs : idx.symbols.xrefs | std::views::values) {
				std::erase_if(refs, [id](const xref_entry& ref) {
					return ref.def_file == id;
				});
			}
			rebuild_symbol_lookups(idx.symbols);
			idx.symbol_count.store(idx.symbols.symbols.size(), std::memory_order_release);
		}
	}

	std::uint64_t loc = 0;
	for (const content_entry& entry : idx.content.entries.items()) {
		if (!is_cpp_ext(to_lower(entry.path.extension().native_encoded_string()))) {
			continue;
		}
		const std::filesystem::path relative = entry.path.lexically_relative(idx.workspace_root);
		if (!relative.empty() && is_counted_source_dir(relative.begin()->display_string()) && !entry.line_starts.empty()) {
			loc += entry.line_starts.size() - 1;
		}
	}
	idx.cpp_loc.store(loc, std::memory_order_release);
}

namespace gse::ide::search {
	constexpr std::uint32_t tu_cache_magic = 0x47535455;
	constexpr std::uint32_t tu_cache_version = 2;

	auto intern_cached(symbol_index& symbols, std::unordered_map<gse::id, file_id>& cache, const std::string& raw) -> file_id {
		const gse::id path_identity = gse::generate_temp_id(raw);
		if (const auto it = cache.find(path_identity); it != cache.end()) {
			return it->second;
		}
		const file_id file = symbols.file_for(raw);
		cache.emplace(path_identity, file);
		return file;
	}

	auto file_fingerprint(const std::filesystem::path& file, std::unordered_map<gse::id, std::uint64_t>& cache) -> std::uint64_t {
		const gse::id file_identity = gse::generate_temp_id(file);
		if (const auto it = cache.find(file_identity); it != cache.end()) {
			return it->second;
		}
		std::uint64_t fingerprint = file_identity.number();
		std::error_code time_ec;
		const std::filesystem::file_time_type modified = std::filesystem::last_write_time(file, time_ec);
		fingerprint = hash_combine(fingerprint, time_ec ? 0ull : static_cast<std::uint64_t>(modified.time_since_epoch().count()));
		std::error_code size_ec;
		const std::uintmax_t size = std::filesystem::file_size(file, size_ec);
		fingerprint = hash_combine(fingerprint, size_ec ? 0ull : static_cast<std::uint64_t>(size));
		cache.emplace(file_identity, fingerprint);
		return fingerprint;
	}

	auto tu_fingerprint(const analysis::compilation_entry& entry, const std::filesystem::path& plugin, std::span<const std::filesystem::path> dependencies, std::unordered_map<gse::id, std::uint64_t>& file_fingerprints) -> std::uint64_t {
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

	auto tu_cache_path(const index_state& index, const analysis::compilation_entry& entry) -> std::filesystem::path {
		const std::uint64_t id = hash_combine(gse::generate_temp_id(entry.file).number(), entry.command.fingerprint);
		return index.compile_commands.parent_path() / "gseditor_symbols" / std::format("{:016x}.bin", id);
	}

	auto save_tu_cache(const std::filesystem::path& path, const analysis::compilation_entry& entry, const std::filesystem::path& plugin, const analysis::tu_symbols& symbols, std::unordered_map<gse::id, std::uint64_t>& file_fingerprints, bool failed) -> void {
		std::vector<std::string> dependencies;
		dependencies.reserve(symbols.dependencies.size());
		for (const std::filesystem::path& dependency : symbols.dependencies) {
			dependencies.push_back(dependency.generic_native_encoded_string());
		}
		const std::uint64_t fingerprint = tu_fingerprint(entry, plugin, symbols.dependencies, file_fingerprints);

		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);
		std::filesystem::path temporary = path;
		temporary += ".tmp";
		std::ofstream out(temporary, std::ios::binary);
		if (!out) {
			return;
		}
		gse::binary_writer writer(out, tu_cache_magic, tu_cache_version);
		writer & fingerprint & dependencies & failed & symbols.set;
		out.close();
		std::filesystem::remove(path, ec);
		std::filesystem::rename(temporary, path, ec);
	}

	auto load_tu_cache(const std::filesystem::path& path, const analysis::compilation_entry& entry, const std::filesystem::path& plugin, analysis::tu_symbols& out, std::unordered_map<gse::id, std::uint64_t>& file_fingerprints, bool& failed) -> bool {
		std::ifstream in(path, std::ios::binary);
		if (!in) {
			return false;
		}
		gse::binary_reader reader(in);
		std::uint32_t magic = 0;
		std::uint32_t version = 0;
		reader & magic & version;
		if (magic != tu_cache_magic || version != tu_cache_version) {
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
		if (tu_fingerprint(entry, plugin, dependencies, file_fingerprints) != fingerprint) {
			return false;
		}
		out.tu = entry.file;
		out.dependencies = std::move(dependencies);
		reader & failed & out.set;
		return static_cast<bool>(in) && (failed || out.set.complete);
	}

	auto symbol_worker_loop(index_state* idx) -> void {
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
			build_symbols(*idx);
		}
	}
}

gse::ide::search::index_state::~index_state() {
	cancel.store(true, std::memory_order_release);
	{
		std::lock_guard lock(build_mutex);
		build_stop = true;
	}
	build_cv.notify_all();
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

auto gse::ide::search::build_symbols(index_state& idx) -> void {
	if (idx.compile_commands.empty() || idx.plugin_dll.empty()) {
		idx.symbols_ready.store(true, std::memory_order_release);
		return;
	}
	bool expected = false;
	if (!idx.building.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
		return;
	}
	idx.phase.store(index_phase::scanning, std::memory_order_release);

	const std::shared_ptr<const analysis::compilation_database> database = analysis::load_compilation_database(idx.compile_commands);
	if (!database) {
		idx.symbols_ready.store(true, std::memory_order_release);
		idx.phase.store(index_phase::idle, std::memory_order_release);
		idx.building.store(false, std::memory_order_release);
		return;
	}

	std::vector<const analysis::compilation_entry*> tus;
	tus.reserve(database->entries.size());
	for (const analysis::compilation_entry& entry : database->entries.items()) {
		if (is_indexed_path(idx.workspace_root, entry.file)) {
			tus.push_back(&entry);
		}
	}

	std::unordered_map<gse::id, std::uint64_t> file_fingerprints;
	std::vector<analysis::tu_symbols> collected;
	collected.reserve(tus.size());
	std::vector<const analysis::compilation_entry*> pending;
	pending.reserve(tus.size());
	std::size_t cached = 0;
	std::size_t failed = 0;
	std::size_t failed_cached = 0;
	for (const analysis::compilation_entry* entry : tus) {
		analysis::tu_symbols symbols;
		bool cache_failed = false;
		if (load_tu_cache(tu_cache_path(idx, *entry), *entry, idx.plugin_dll, symbols, file_fingerprints, cache_failed)) {
			if (cache_failed) {
				++failed;
				++failed_cached;
			}
			else {
				collected.push_back(std::move(symbols));
				++cached;
			}
		}
		else {
			pending.push_back(entry);
		}
	}

	const std::size_t round0 = std::max<std::size_t>(2, gse::task::thread_count() / 2);
	std::size_t compiled = 0;
	for (int round = 0; round < 2 && !pending.empty(); ++round) {
		idx.phase.store(round == 0 ? index_phase::compiling : index_phase::retrying, std::memory_order_release);
		idx.tus_total.store(pending.size(), std::memory_order_release);
		idx.tus_done.store(0, std::memory_order_release);
		std::vector<analysis::tu_symbols> results = run_symbol_batch(pending, idx.plugin_dll, idx.workspace_root, round == 0 ? round0 : 1, &idx.tus_done, &idx.cancel);
		if (idx.cancel.load(std::memory_order_acquire)) {
			idx.phase.store(index_phase::idle, std::memory_order_release);
			idx.building.store(false, std::memory_order_release);
			return;
		}
		std::vector<const analysis::compilation_entry*> next_round;
		for (std::size_t i = 0; i < results.size(); ++i) {
			analysis::tu_symbols& result = results[i];
			if (result.failure == analysis::symbol_index_failure::none && result.set.complete) {
				save_tu_cache(tu_cache_path(idx, *pending[i]), *pending[i], idx.plugin_dll, result, file_fingerprints, false);
				collected.push_back(std::move(result));
				++compiled;
			}
			else if (round == 0 && result.retryable()) {
				next_round.push_back(pending[i]);
			}
			else {
				if (result.dependencies.empty()) {
					result.dependencies.push_back(pending[i]->file);
				}
				save_tu_cache(tu_cache_path(idx, *pending[i]), *pending[i], idx.plugin_dll, result, file_fingerprints, true);
				++failed;
			}
		}
		pending = std::move(next_round);
	}

	idx.phase.store(index_phase::aggregating, std::memory_order_release);
	symbol_index local;
	std::size_t symbol_capacity = 0;
	for (const analysis::tu_symbols& tu : collected) {
		symbol_capacity += tu.set.symbols.size();
	}
	local.symbols.reserve(symbol_capacity);
	std::unordered_map<gse::id, file_id> raw_file_ids;
	raw_file_ids.reserve(1024);
	std::unordered_set<std::string> seen_channels;
	for (analysis::tu_symbols& tu : collected) {
		for (analysis::symbol_token& symbol : tu.set.symbols) {
			if (!is_indexed_path(idx.workspace_root, symbol.file)) {
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
			if (!is_indexed_path(idx.workspace_root, ref.file)) {
				continue;
			}
			const file_id file = intern_cached(local, raw_file_ids, ref.file);
			const file_id definition_file = intern_cached(local, raw_file_ids, ref.def_file);
			local.xrefs[file].push_back({
				.line = ref.line > 0 ? ref.line - 1 : 0,
				.column = ref.column > 0 ? ref.column - 1 : 0,
				.length = ref.length,
				.def_file = definition_file,
				.def_line = ref.def_line > 0 ? ref.def_line - 1 : 0,
				.def_column = ref.def_column > 0 ? ref.def_column - 1 : 0,
				.qualified = std::move(ref.qualified),
				.identity = std::move(ref.identity),
			});
		}
		for (analysis::channel_use& channel : tu.set.channels) {
			std::string key = (channel.produce ? "1|" : "0|") + channel.system + "|" + channel.message;
			if (seen_channels.insert(std::move(key)).second) {
				local.channels.push_back(std::move(channel));
			}
		}
	}
	rebuild_symbol_lookups(local);

	gse::log::println(gse::log::level::info, gse::log::category::general, "[symidx] {} TUs, {} cached, {} compiled, {} failed ({} cached), {} symbols, {} files", tus.size(), cached, compiled, failed, failed_cached, local.symbols.size(), local.files.size());

	idx.symbol_count.store(local.symbols.size(), std::memory_order_release);
	{
		std::unique_lock lock(idx.mutex);
		idx.symbols = std::move(local);
	}
	idx.symbols_ready.store(true, std::memory_order_release);
	idx.phase.store(index_phase::idle, std::memory_order_release);
	idx.building.store(false, std::memory_order_release);
}

auto gse::ide::search::index_state::merge_file_symbols(const std::filesystem::path& file, std::span<const analysis::symbol_token> syms, std::span<const analysis::symbol_ref> refs) -> void {
	std::unique_lock lock(mutex);
	const file_id fid = symbols.file_for(file);

	std::unordered_map<gse::id, file_id> local_fid;

	std::erase_if(symbols.symbols, [fid](const symbol_entry& e) {
		return e.file == fid;
	});
	for (const analysis::symbol_token& s : syms) {
		if (intern_cached(symbols, local_fid, s.file) != fid) {
			continue;
		}
		symbols.symbols.push_back({ .name = s.name, .name_lower = to_lower(s.name), .kind = s.kind, .file = fid, .line = s.line > 0 ? s.line - 1 : 0, .column = s.column > 0 ? s.column - 1 : 0, .qualified = s.qualified, .identity = s.identity, .is_definition = s.is_definition });
	}

	std::vector<xref_entry>& file_xrefs = symbols.xrefs[fid];
	file_xrefs.clear();
	for (const analysis::symbol_ref& r : refs) {
		if (intern_cached(symbols, local_fid, r.file) != fid) {
			continue;
		}
		const file_id dfid = intern_cached(symbols, local_fid, r.def_file);
		file_xrefs.push_back({ .line = r.line > 0 ? r.line - 1 : 0, .column = r.column > 0 ? r.column - 1 : 0, .length = r.length, .def_file = dfid, .def_line = r.def_line > 0 ? r.def_line - 1 : 0, .def_column = r.def_column > 0 ? r.def_column - 1 : 0, .qualified = r.qualified, .identity = r.identity });
	}

	rebuild_symbol_lookups(symbols);
	symbol_count.store(symbols.symbols.size(), std::memory_order_release);
}
