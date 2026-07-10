export module gse.ide.search:index;

import std;
import gse;
import gse.ide.analysis;

import :types;

export namespace gse::ide::search {
	struct file_index {
		std::vector<file_entry> entries;
		std::atomic<bool> loaded = false;
	};

	struct content_index {
		std::vector<std::filesystem::path> paths;
		std::vector<std::string> blobs;
		std::vector<std::vector<std::uint32_t>> line_starts;
		std::atomic<bool> loaded = false;
	};

	struct module_entry {
		std::string name;
		file_id file = 0;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
	};

	struct module_index {
		std::vector<module_entry> modules;
		std::vector<std::filesystem::path> files;
		std::unordered_map<std::string, file_id> file_ids;

		auto file_for(const std::filesystem::path& path) -> file_id;
		auto path_for(file_id id) const -> std::filesystem::path;
	};

	struct symbol_index {
		std::vector<symbol_entry> symbols;
		std::unordered_map<file_id, std::vector<xref_entry>> xrefs;
		std::vector<std::filesystem::path> files;
		std::unordered_map<std::string, file_id> file_ids;

		auto file_for(const std::filesystem::path& path) -> file_id;
		auto path_for(file_id id) const -> std::filesystem::path;
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
		mutable std::shared_mutex mutex;

		auto definition_at(const std::filesystem::path& file, std::uint32_t line, std::uint32_t column) const -> std::optional<location>;
		auto symbol_at(const std::filesystem::path& file, std::uint32_t line, std::uint32_t column) const -> std::optional<hover_hit>;
		auto symbol_definition(std::string_view name, std::string_view qualifier, const std::filesystem::path& click_file) const -> std::optional<location>;
		auto module_definition(std::string_view name, const std::filesystem::path& click_file) const -> std::optional<location>;
		auto declaration_of(std::string_view name, std::string_view qualifier) const -> std::optional<hover_hit>;
		auto merge_file_symbols(const std::filesystem::path& file, std::span<const analysis::symbol_token> syms, std::span<const analysis::symbol_ref> refs) -> void;
	};

	struct index_merge_request {
		std::filesystem::path path;
		std::shared_ptr<analysis::diagnostics_check> check;
	};

	auto build_files_and_content(index_state& idx, const std::filesystem::path& root) -> void;
	auto build_symbols(index_state& idx) -> void;
}

namespace gse::ide::search {
	auto is_skipped_dir(std::string_view name) -> bool {
		return name == "out" || name == ".git" || name == ".vs" || name == ".vscode" || name == ".claude"
			|| name == "External" || name == "vcpkg" || name == ".gcc-ci-build"
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

	auto canonical_path_key(const std::filesystem::path& path) -> std::pair<std::filesystem::path, std::string> {
		std::error_code ec;
		std::filesystem::path canon = std::filesystem::weakly_canonical(path, ec);
		if (ec) {
			canon = path;
		}
		return { canon, canon.generic_native_encoded_string() };
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

	auto is_definition_kind(analysis::symbol_kind kind) -> bool {
		switch (kind) {
			case analysis::symbol_kind::function:
			case analysis::symbol_kind::type:
			case analysis::symbol_kind::enumeration:
			case analysis::symbol_kind::concept_decl:
				return true;
			default:
				return false;
		}
	}

	auto same_location(const symbol_index& index, const symbol_entry& s, const location& loc) -> bool {
		return index.path_for(s.file) == loc.path && s.line == loc.line && s.column == loc.column;
	}

	auto symbol_location(const symbol_index& index, const symbol_entry& s) -> location {
		return location{ .path = index.path_for(s.file), .line = s.line, .column = s.column };
	}

	auto matching_function_definition(const symbol_index& index, std::string_view identity, std::string_view qualified, const std::optional<location>& from) -> std::optional<location> {
		const symbol_entry* fallback = nullptr;
		for (const symbol_entry& s : index.symbols) {
			if (s.kind != analysis::symbol_kind::function || !s.is_definition) {
				continue;
			}
			const bool identity_match = !identity.empty() && s.identity == identity;
			const bool qualified_match = identity.empty() && !qualified.empty() && s.qualified == qualified;
			if (!identity_match && !qualified_match) {
				continue;
			}
			if (from && same_location(index, s, *from)) {
				continue;
			}
			if (identity_match) {
				return symbol_location(index, s);
			}
			if (!fallback) {
				fallback = &s;
			}
		}
		if (!fallback) {
			return std::nullopt;
		}
		return symbol_location(index, *fallback);
	}

	auto promoted_definition(const symbol_index& index, const location& target, std::string_view identity, std::string_view qualified) -> location {
		for (const symbol_entry& s : index.symbols) {
			if (same_location(index, s, target)) {
				if (s.kind == analysis::symbol_kind::function && !s.is_definition) {
					return matching_function_definition(index, s.identity.empty() ? identity : std::string_view(s.identity), s.qualified.empty() ? qualified : std::string_view(s.qualified), std::optional<location>{ target }).value_or(target);
				}
				return target;
			}
		}
		if (std::optional<location> def = matching_function_definition(index, identity, qualified, std::optional<location>{ target })) {
			return *def;
		}
		return target;
	}

	auto run_symbol_batch(const std::string& json_text, const std::vector<std::filesystem::path>& list, const std::filesystem::path& plugin_dll, const std::filesystem::path& root, std::size_t workers, std::atomic<std::size_t>* progress) -> std::vector<analysis::tu_symbols> {
		std::vector<analysis::tu_symbols> out(list.size());
		std::atomic<std::size_t> next = 0;
		std::vector<std::thread> pool;
		const std::size_t n = std::min(workers, list.size());
		for (std::size_t w = 0; w < n; ++w) {
			pool.emplace_back([&out, &next, &list, &json_text, &plugin_dll, &root, w, progress] {
				while (true) {
					const std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
					if (i >= list.size()) {
						break;
					}
					out[i] = analysis::symbol_index_builder::run_one(json_text, list[i], plugin_dll, root, static_cast<std::uint32_t>(w));
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

auto gse::ide::search::module_index::file_for(const std::filesystem::path& path) -> file_id {
	auto [canon, key] = canonical_path_key(path);
	if (const auto it = file_ids.find(key); it != file_ids.end()) {
		return it->second;
	}
	const file_id id = static_cast<file_id>(files.size());
	files.push_back(std::move(canon));
	file_ids.emplace(std::move(key), id);
	return id;
}

auto gse::ide::search::module_index::path_for(file_id id) const -> std::filesystem::path {
	if (id < files.size()) {
		return files[id];
	}
	return {};
}

auto gse::ide::search::symbol_index::file_for(const std::filesystem::path& path) -> file_id {
	auto [canon, key] = canonical_path_key(path);
	if (const auto it = file_ids.find(key); it != file_ids.end()) {
		return it->second;
	}
	const file_id id = static_cast<file_id>(files.size());
	files.push_back(std::move(canon));
	file_ids.emplace(std::move(key), id);
	return id;
}

auto gse::ide::search::symbol_index::path_for(file_id id) const -> std::filesystem::path {
	if (id < files.size()) {
		return files[id];
	}
	return {};
}

auto gse::ide::search::index_state::definition_at(const std::filesystem::path& file, std::uint32_t line, std::uint32_t column) const -> std::optional<location> {
	std::error_code ec;
	const std::filesystem::path canon = std::filesystem::weakly_canonical(file, ec);
	const std::string key = (ec ? file : canon).generic_native_encoded_string();
	std::shared_lock lock(mutex);
	const auto id_it = symbols.file_ids.find(key);
	if (id_it == symbols.file_ids.end()) {
		return std::nullopt;
	}
	const auto xr_it = symbols.xrefs.find(id_it->second);
	if (xr_it != symbols.xrefs.end()) {
		for (const xref_entry& x : xr_it->second) {
			if (x.line == line && column >= x.column && column < x.column + x.length) {
				const location target{ .path = symbols.path_for(x.def_file), .line = x.def_line, .column = x.def_column };
				return promoted_definition(symbols, target, x.identity, x.qualified);
			}
		}
	}
	const location cursor{ .path = symbols.path_for(id_it->second), .line = line, .column = column };
	for (const symbol_entry& s : symbols.symbols) {
		if (s.file == id_it->second && s.line == line && column >= s.column && column < s.column + s.name.size()) {
			if (s.kind == analysis::symbol_kind::function && !s.is_definition) {
				return matching_function_definition(symbols, s.identity, s.qualified, cursor);
			}
		}
	}
	return std::nullopt;
}

auto gse::ide::search::index_state::symbol_at(const std::filesystem::path& file, std::uint32_t line, std::uint32_t column) const -> std::optional<hover_hit> {
	std::error_code ec;
	const std::filesystem::path canon = std::filesystem::weakly_canonical(file, ec);
	const std::string key = (ec ? file : canon).generic_native_encoded_string();
	std::shared_lock lock(mutex);
	const auto id_it = symbols.file_ids.find(key);
	if (id_it == symbols.file_ids.end()) {
		return std::nullopt;
	}
	const auto xr_it = symbols.xrefs.find(id_it->second);
	if (xr_it == symbols.xrefs.end()) {
		return std::nullopt;
	}
	for (const xref_entry& x : xr_it->second) {
		if (x.line == line && column >= x.column && column < x.column + x.length) {
			std::string kind;
			for (const symbol_entry& s : symbols.symbols) {
				if (s.file == x.def_file && s.line == x.def_line) {
					kind = std::format("{}", s.kind);
					break;
				}
			}
			const location target{ .path = symbols.path_for(x.def_file), .line = x.def_line, .column = x.def_column };
			return hover_hit{ .def = promoted_definition(symbols, target, x.identity, x.qualified), .qualified = x.qualified, .kind = kind };
		}
	}
	return std::nullopt;
}

auto gse::ide::search::index_state::symbol_definition(std::string_view name, std::string_view qualifier, const std::filesystem::path& click_file) const -> std::optional<location> {
	std::string needle;
	needle.reserve(qualifier.size() + name.size());
	needle.append(qualifier);
	needle.append(name);
	std::string suffix = "::";
	suffix.append(needle);
	std::error_code ec;
	const std::filesystem::path canon = std::filesystem::weakly_canonical(click_file, ec);
	const std::string ckey = (ec ? click_file : canon).generic_native_encoded_string();
	std::shared_lock lock(mutex);
	file_id click_fid = static_cast<file_id>(-1);
	if (const auto it = symbols.file_ids.find(ckey); it != symbols.file_ids.end()) {
		click_fid = it->second;
	}
	const symbol_entry* best = nullptr;
	int best_score = 0;
	for (const symbol_entry& s : symbols.symbols) {
		if (s.name != name || !is_definition_kind(s.kind)) {
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
		else if (s.kind == analysis::symbol_kind::type || s.kind == analysis::symbol_kind::enumeration || s.kind == analysis::symbol_kind::concept_decl) {
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
	const std::string key = canonical_path_key(click_file).second;
	std::shared_lock lock(mutex);
	if (target.starts_with(':')) {
		const auto file_it = modules.file_ids.find(key);
		if (file_it == modules.file_ids.end()) {
			return std::nullopt;
		}
		std::string current;
		for (const module_entry& m : modules.modules) {
			if (m.file == file_it->second) {
				current = m.name;
				break;
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

	const module_entry* best = nullptr;
	for (const module_entry& m : modules.modules) {
		if (m.name != target) {
			continue;
		}
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
	const symbol_entry* best = nullptr;
	const symbol_entry* fallback = nullptr;
	int best_score = 0;
	for (const symbol_entry& s : symbols.symbols) {
		if (s.name != name || !is_definition_kind(s.kind)) {
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

auto gse::ide::search::build_files_and_content(index_state& idx, const std::filesystem::path& root) -> void {
	std::vector<file_entry> files;
	std::vector<std::filesystem::path> text_paths;

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
			if (is_skipped_dir(entry.path().filename().native_encoded_string())) {
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
		files.push_back({ .path = path, .rel = rel, .rel_lower = to_lower(rel) });

		const std::string ext = to_lower(path.extension().native_encoded_string());
		if (!is_binary_ext(ext)) {
			const auto size = entry.file_size(type_ec);
			if (!type_ec && size <= 2u * 1024u * 1024u) {
				text_paths.push_back(path);
			}
		}
	}

	const std::size_t n = text_paths.size();
	std::vector<std::string> blobs(n);
	std::vector<std::vector<std::uint32_t>> line_starts(n);

	gse::task::coarse_parallel(n, 8, [&](std::size_t i) {
		std::ifstream in(text_paths[i], std::ios::binary);
		if (!in) {
			return;
		}
		std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		line_starts[i] = compute_line_starts(data);
		blobs[i] = std::move(data);
	});

	std::uint64_t loc = 0;
	module_index module_defs;
	for (std::size_t i = 0; i < n; ++i) {
		if (line_starts[i].empty() || !is_cpp_ext(to_lower(text_paths[i].extension().native_encoded_string()))) {
			continue;
		}
		scan_modules(text_paths[i], blobs[i], line_starts[i], module_defs);
		std::error_code rel_ec;
		const std::filesystem::path rel = std::filesystem::relative(text_paths[i], root, rel_ec);
		if (rel_ec || rel.empty() || !is_counted_source_dir(rel.begin()->string())) {
			continue;
		}
		loc += line_starts[i].size() - 1;
	}
	idx.cpp_loc.store(loc, std::memory_order_release);

	{
		std::unique_lock lock(idx.mutex);
		idx.modules = std::move(module_defs);
		idx.files.entries = std::move(files);
		idx.content.paths = std::move(text_paths);
		idx.content.blobs = std::move(blobs);
		idx.content.line_starts = std::move(line_starts);
	}
	idx.files.loaded.store(true, std::memory_order_release);
	idx.content.loaded.store(true, std::memory_order_release);
}

namespace gse::ide::search {
	constexpr std::uint32_t symbol_cache_magic = 0x47534958;
	constexpr std::uint32_t symbol_cache_version = 2;

	struct symbol_cache {
		std::uint64_t fingerprint = 0;
		std::vector<std::string> files;
		std::vector<symbol_entry> symbols;
		std::unordered_map<file_id, std::vector<xref_entry>> xrefs;
	};

	auto symbol_cache_path(const index_state& idx) -> std::filesystem::path {
		return idx.compile_commands.parent_path() / "gseditor_symbols.bin";
	}

	auto fnv_mix(std::uint64_t h, std::uint64_t value) -> std::uint64_t {
		h ^= value;
		h *= 1099511628211ull;
		return h;
	}

	auto fnv_mix_file(std::uint64_t h, const std::filesystem::path& p) -> std::uint64_t {
		std::error_code time_ec;
		const std::filesystem::file_time_type t = std::filesystem::last_write_time(p, time_ec);
		h = fnv_mix(h, time_ec ? 0ull : static_cast<std::uint64_t>(t.time_since_epoch().count()));
		std::error_code size_ec;
		const std::uintmax_t s = std::filesystem::file_size(p, size_ec);
		return fnv_mix(h, size_ec ? 0ull : static_cast<std::uint64_t>(s));
	}

	auto fnv_hash_str(std::string_view s) -> std::uint64_t {
		std::uint64_t h = 1469598103934665603ull;
		for (const char c : s) {
			h = fnv_mix(h, static_cast<std::uint64_t>(static_cast<unsigned char>(c)));
		}
		return h;
	}

	auto intern_cached(symbol_index& symbols, std::unordered_map<std::string, file_id>& cache, const std::string& raw) -> file_id {
		if (const auto it = cache.find(raw); it != cache.end()) {
			return it->second;
		}
		const file_id id = symbols.file_for(raw);
		cache.emplace(raw, id);
		return id;
	}

	auto compute_symbol_fingerprint(std::uint64_t cc_hash, const std::filesystem::path& plugin, std::span<const std::filesystem::path> files) -> std::uint64_t {
		std::uint64_t h = 1469598103934665603ull;
		h = fnv_mix(h, symbol_cache_version);
		h = fnv_mix(h, cc_hash);
		if (!plugin.empty()) {
			h = fnv_mix_file(h, plugin);
		}
		h = fnv_mix(h, files.size());
		for (const std::filesystem::path& f : files) {
			h = fnv_mix(h, fnv_hash_str(f.generic_native_encoded_string()));
			h = fnv_mix_file(h, f);
		}
		return h;
	}

	auto save_symbol_cache(const std::filesystem::path& path, std::uint64_t cc_hash, const std::filesystem::path& plugin, const symbol_index& index) -> void {
		std::vector<std::string> files;
		files.reserve(index.files.size());
		for (const std::filesystem::path& p : index.files) {
			files.push_back(p.generic_native_encoded_string());
		}
		const symbol_cache cache{
			.fingerprint = compute_symbol_fingerprint(cc_hash, plugin, index.files),
			.files = std::move(files),
			.symbols = index.symbols,
			.xrefs = index.xrefs,
		};

		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);
		std::ofstream out(path, std::ios::binary);
		if (!out) {
			return;
		}
		gse::binary_writer writer(out, symbol_cache_magic, symbol_cache_version);
		writer & cache;
	}

	auto load_symbol_cache(const std::filesystem::path& path, std::uint64_t cc_hash, const std::filesystem::path& plugin, symbol_index& out) -> bool {
		std::ifstream in(path, std::ios::binary);
		if (!in) {
			return false;
		}
		gse::binary_reader reader(in);
		std::uint32_t magic = 0;
		std::uint32_t version = 0;
		reader & magic & version;
		if (magic != symbol_cache_magic || version != symbol_cache_version) {
			return false;
		}
		symbol_cache cache;
		reader & cache;
		if (!in) {
			return false;
		}
		std::vector<std::filesystem::path> files;
		files.reserve(cache.files.size());
		for (const std::string& f : cache.files) {
			files.emplace_back(f);
		}
		if (compute_symbol_fingerprint(cc_hash, plugin, files) != cache.fingerprint) {
			return false;
		}
		out.symbols = std::move(cache.symbols);
		out.xrefs = std::move(cache.xrefs);
		out.file_ids.clear();
		out.file_ids.reserve(files.size());
		for (std::uint32_t i = 0; i < files.size(); ++i) {
			out.file_ids.emplace(cache.files[i], static_cast<file_id>(i));
		}
		out.files = std::move(files);
		return true;
	}
}

auto gse::ide::search::build_symbols(index_state& idx) -> void {
	if (idx.compile_commands.empty() || idx.plugin_dll.empty()) {
		idx.symbols_ready.store(true, std::memory_order_release);
		return;
	}
	idx.building.store(true, std::memory_order_release);
	idx.phase.store(index_phase::scanning, std::memory_order_release);

	std::string json_text;
	{
		std::ifstream in(idx.compile_commands, std::ios::binary);
		if (in) {
			std::ostringstream stream;
			stream << in.rdbuf();
			json_text = stream.str();
		}
	}

	std::vector<std::filesystem::path> tus;
	if (const std::optional<analysis::json::value> root = analysis::json::parse(json_text); root && root->is_array()) {
		for (const analysis::json::value& entry : root->children) {
			if (const analysis::json::value* f = entry.find("file")) {
				if (const std::string_view file = f->as_string(); !file.empty()) {
					tus.emplace_back(std::string(file));
				}
			}
		}
	}

	const std::filesystem::path cache_path = symbol_cache_path(idx);
	const std::uint64_t cc_hash = fnv_hash_str(json_text);
	{
		symbol_index cached;
		if (load_symbol_cache(cache_path, cc_hash, idx.plugin_dll, cached)) {
			const std::size_t count = cached.symbols.size();
			idx.symbol_count.store(count, std::memory_order_release);
			{
				std::unique_lock lock(idx.mutex);
				idx.symbols = std::move(cached);
			}
			idx.symbols_ready.store(true, std::memory_order_release);
			idx.phase.store(index_phase::idle, std::memory_order_release);
			idx.building.store(false, std::memory_order_release);
			gse::log::println(gse::log::level::error, gse::log::category::general, "[symidx] cache hit ({} symbols)", count);
			return;
		}
	}

	const std::size_t round0 = std::max<std::size_t>(2, gse::task::thread_count() / 2);

	std::vector<analysis::tu_symbols> collected;
	std::vector<std::filesystem::path> pending = tus;
	std::size_t failed = 0;
	for (int round = 0; round < 3 && !pending.empty(); ++round) {
		idx.phase.store(round == 0 ? index_phase::compiling : index_phase::retrying, std::memory_order_release);
		idx.tus_total.store(pending.size(), std::memory_order_release);
		idx.tus_done.store(0, std::memory_order_release);
		std::vector<analysis::tu_symbols> res = run_symbol_batch(json_text, pending, idx.plugin_dll, idx.workspace_root, round == 0 ? round0 : 1, &idx.tus_done);
		std::vector<std::filesystem::path> next_round;
		for (analysis::tu_symbols& r : res) {
			if (r.set.complete) {
				collected.push_back(std::move(r));
			}
			else if (round == 2) {
				++failed;
			}
			else {
				next_round.push_back(std::move(r.tu));
			}
		}
		pending = std::move(next_round);
	}

	idx.phase.store(index_phase::aggregating, std::memory_order_release);
	symbol_index local;
	for (analysis::tu_symbols& tu : collected) {
		for (const analysis::symbol_token& s : tu.set.symbols) {
			const file_id fid = local.file_for(s.file);
			local.symbols.push_back({ .name = s.name, .name_lower = to_lower(s.name), .kind = s.kind, .file = fid, .line = s.line > 0 ? s.line - 1 : 0, .column = s.column > 0 ? s.column - 1 : 0, .qualified = s.qualified, .identity = s.identity, .is_definition = s.is_definition });
		}
		for (const analysis::symbol_ref& r : tu.set.refs) {
			const file_id rfid = local.file_for(r.file);
			const file_id dfid = local.file_for(r.def_file);
			local.xrefs[rfid].push_back({ .line = r.line > 0 ? r.line - 1 : 0, .column = r.column > 0 ? r.column - 1 : 0, .length = r.length, .def_file = dfid, .def_line = r.def_line > 0 ? r.def_line - 1 : 0, .def_column = r.def_column > 0 ? r.def_column - 1 : 0, .qualified = r.qualified, .identity = r.identity });
		}
	}

	gse::log::println(gse::log::level::error, gse::log::category::general, "[symidx] {} TUs, {} failed, {} symbols, {} files", tus.size(), failed, local.symbols.size(), local.files.size());

	save_symbol_cache(cache_path, cc_hash, idx.plugin_dll, local);

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

	std::unordered_map<std::string, file_id> local_fid;

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

	symbol_count.store(symbols.symbols.size(), std::memory_order_release);
}
