export module gse.ide.search:index;

import std;
import gse;
import gse.ide.analysis;

import :types;

export namespace gse::ide::search {
	struct file_index {
		id_mapped_collection<file_entry> entries;
		std::atomic<bool> loaded = false;
	};

	struct content_entry {
		std::filesystem::path path;
		std::string blob;
		std::vector<std::uint32_t> line_starts;
	};

	struct content_index {
		id_mapped_collection<std::shared_ptr<content_entry>> entries;
		std::atomic<bool> loaded = false;
	};

	struct module_entry {
		std::string name;
		file_id file{};
		std::uint32_t line = 0;
		std::uint32_t column = 0;
	};

	struct module_index : non_copyable {
		module_index() = default;

		~module_index();

		module_index(
			module_index&&
		) noexcept = default;

		auto operator=(
			module_index&&
		) noexcept -> module_index& = default;

		std::vector<module_entry> modules;
		std::unordered_map<file_id, std::filesystem::path> files;
		std::unordered_map<std::string, std::vector<std::uint32_t>, transparent_hash, transparent_equal> modules_by_name;
		std::unordered_map<file_id, std::vector<std::uint32_t>> modules_by_file;

		auto file_for(
			const std::filesystem::path& path
		) -> file_id;

		auto path_for(
			file_id id
		) const -> std::filesystem::path;
	};

	struct positioned_kind {
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::uint32_t length = 0;
		analysis::semantic_kind kind = analysis::semantic_kind::variable;
	};

	struct symbol_index : non_copyable {
		symbol_index() = default;

		~symbol_index();

		symbol_index(
			symbol_index&&
		) noexcept = default;

		auto operator=(
			symbol_index&&
		) noexcept -> symbol_index& = default;

		std::vector<symbol_entry> symbols;
		std::unordered_map<file_id, std::vector<xref_entry>> xrefs;
		std::unordered_map<file_id, std::vector<positioned_kind>> params;
		std::unordered_map<file_id, std::filesystem::path> files;
		std::vector<analysis::channel_use> channels;
		std::unordered_map<file_id, std::vector<std::uint32_t>> symbols_by_file;
		std::unordered_map<std::string, std::vector<std::uint32_t>, transparent_hash, transparent_equal> symbols_by_name;
		std::unordered_map<std::string, std::vector<std::uint32_t>, transparent_hash, transparent_equal> definitions_by_identity;
		std::unordered_map<std::string, std::vector<std::uint32_t>, transparent_hash, transparent_equal> definitions_by_qualified;
		std::unordered_map<file_id, std::string> failures;

		auto file_for(
			const std::filesystem::path& path
		) -> file_id;

		auto path_for(
			file_id id
		) const -> std::filesystem::path;
	};

	struct searchable_symbol {
		std::filesystem::path path;
		std::string name;
		std::string name_lower;
		analysis::symbol_kind kind = analysis::symbol_kind::type;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
	};

	struct file_search_snapshot {
		std::uint64_t file_generation = 0;
		std::shared_ptr<const std::vector<file_entry>> files;
		std::shared_ptr<const std::vector<std::shared_ptr<const content_entry>>> content;
	};

	struct symbol_search_snapshot {
		std::uint64_t symbol_generation = 0;
		std::shared_ptr<const std::vector<searchable_symbol>> symbols;
	};

	struct search_snapshot {
		std::shared_ptr<const file_search_snapshot> files;
		std::shared_ptr<const symbol_search_snapshot> symbols;
	};

	struct symbol_overlay {
		std::filesystem::path path;
		std::vector<analysis::symbol_token> symbols;
		std::vector<analysis::symbol_ref> refs;
		std::vector<analysis::param_token> params;
	};

	enum class loc_origin : std::uint8_t {
		engine,
		editor,
		project,
	};

	enum class loc_language : std::uint8_t {
		cpp,
		slang,
	};

	struct loc_bucket {
		loc_origin origin = loc_origin::engine;
		loc_language language = loc_language::cpp;
	};

	struct loc_group {
		std::uint64_t cpp = 0;
		std::uint64_t slang = 0;

		auto operator==(
			const loc_group&
		) const -> bool = default;
	};

	struct loc_counts {
		loc_group engine;
		loc_group editor;
		loc_group project;

		auto operator==(
			const loc_counts&
		) const -> bool = default;
	};

	struct loc_index {
		std::atomic<std::uint64_t> engine_cpp = 0;
		std::atomic<std::uint64_t> engine_slang = 0;
		std::atomic<std::uint64_t> editor_cpp = 0;
		std::atomic<std::uint64_t> editor_slang = 0;
		std::atomic<std::uint64_t> project_cpp = 0;
		std::atomic<std::uint64_t> project_slang = 0;

		auto store(
			const loc_counts& counts
		) -> void;

		auto load() const -> loc_counts;
	};

	auto loc_total(
		const loc_counts& counts
	) -> std::uint64_t;

	struct file_build_result : non_copyable {
		file_build_result() = default;

		~file_build_result();

		file_build_result(
			file_build_result&&
		) noexcept = default;

		auto operator=(
			file_build_result&&
		) noexcept -> file_build_result& = default;

		module_index modules;
		id_mapped_collection<file_entry> files;
		id_mapped_collection<std::shared_ptr<content_entry>> content;
		loc_counts loc;
	};

	enum class lookup_failure {
		index_unavailable,
		file_not_indexed,
		reference_not_found,
		symbol_not_found,
		definition_not_found,
		qualified_symbol_not_found,
		ambiguous_symbol,
		module_name_empty,
		module_context_not_found,
		module_not_found,
		index_building,
		translation_unit_failed,
		kind_not_recorded,
		type_not_recorded,
	};

	struct lookup_error {
		lookup_failure reason = lookup_failure::reference_not_found;
		std::string subject;
		std::string detail;
	};

	struct hover_hit {
		location def;
		std::string qualified;
		std::string kind;
		std::string type;
		std::string value;
		std::vector<lookup_error> issues;
	};

	auto describe(
		const lookup_error& error
	) -> std::string;

	auto is_pending(
		const lookup_error& error
	) -> bool;

	struct index_phase_info {
		char label[48];
		char detail[192];
	};

	enum class index_phase : std::uint8_t {
		idle [[= index_phase_info{
			.label = "Ready",
			.detail = "The semantic index is available.",
		}]],
		loading_database [[= index_phase_info{
			.label = "Loading compile commands",
			.detail = "Reading and parsing compile_commands.json.",
		}]],
		discovering_translation_units [[= index_phase_info{
			.label = "Discovering translation units",
			.detail = "Filtering compiler entries to source files inside the workspace.",
		}]],
		validating_cache [[= index_phase_info{
			.label = "Validating semantic cache",
			.detail = "Checking cached semantic data against source dependencies, compiler commands, and the token plugin.",
		}]],
		checking_modules [[= index_phase_info{
			.label = "Checking compiled modules",
			.detail = "Comparing generated compiled modules against the structured CMake dependency graph.",
		}]],
		compiling [[= index_phase_info{
			.label = "Extracting semantic data",
			.detail = "Running the compiler and semantic token plugin for translation units that were not cached.",
		}]],
		retrying [[= index_phase_info{
			.label = "Retrying semantic extraction",
			.detail = "Retrying failed semantic compiler runs serially.",
		}]],
		saving_results [[= index_phase_info{
			.label = "Saving semantic results",
			.detail = "Validating compiler output, recording failures, and updating translation-unit caches.",
		}]],
		merging_records [[= index_phase_info{
			.label = "Merging semantic records",
			.detail = "Combining symbols, references, resolved types, parameters, and channel uses from every translation unit.",
		}]],
		building_lookups [[= index_phase_info{
			.label = "Building symbol lookups",
			.detail = "Building file, name, identity, and qualified-name lookup tables.",
		}]],
		sorting_locations [[= index_phase_info{
			.label = "Sorting source locations",
			.detail = "Sorting symbols and references by source location for hover, highlighting, and navigation.",
		}]],
		publishing [[= index_phase_info{
			.label = "Publishing semantic index",
			.detail = "Applying live-file overlays and replacing the visible immutable semantic index.",
		}]],
	};

	struct index_root {
		std::filesystem::path path;
		std::string name;
		std::filesystem::path compile_commands;
		bool is_project = false;
	};

	struct index_state {
		index_state();

		symbol_index symbols;
		module_index modules;
		file_index files;
		content_index content;
		std::vector<std::filesystem::path> compile_commands;
		std::filesystem::path plugin_dll;
		std::vector<index_root> roots;
		std::atomic<bool> symbols_ready = false;
		std::atomic<bool> building = false;
		loc_index loc;
		std::atomic<std::size_t> progress_total = 0;
		std::atomic<std::size_t> progress_done = 0;
		std::atomic<gse::time_t<double>> phase_started{};
		std::atomic<std::size_t> symbol_count = 0;
		std::atomic<index_phase> phase = index_phase::idle;
		std::atomic<bool> cancel = false;
		std::atomic<std::uint64_t> generation = 0;
		std::atomic<std::uint64_t> file_generation = 0;
		std::atomic<std::uint64_t> symbol_generation = 0;
		mutable std::shared_mutex mutex;
		std::mutex build_mutex;
		std::condition_variable build_cv;
		bool build_requested = false;
		bool build_stop = false;
		std::optional<file_build_result> completed_files;
		std::optional<symbol_index> completed_symbols;
		std::unordered_map<file_id, symbol_overlay> symbol_overlays;
		std::unordered_set<file_id> pending_symbol_files;
		std::shared_ptr<const search_snapshot> current_search_snapshot;
		std::jthread build_worker;

		~index_state();

		auto definition_at(
			const std::filesystem::path& file,
			std::uint32_t line,
			std::uint32_t column
		) const -> std::expected<location, lookup_error>;

		auto symbol_at(
			const std::filesystem::path& file,
			std::uint32_t line,
			std::uint32_t column
		) const -> std::expected<hover_hit, lookup_error>;

		auto symbol_definition(
			std::string_view name,
			std::string_view qualifier,
			const std::filesystem::path& click_file
		) const -> std::expected<location, lookup_error>;

		auto module_definition(
			std::string_view name,
			const std::filesystem::path& click_file
		) const -> std::expected<location, lookup_error>;

		auto declaration_of(
			std::string_view name,
			std::string_view qualifier
		) const -> std::expected<hover_hit, lookup_error>;

		auto semantic_kind_of(
			std::string_view name
		) const -> std::expected<analysis::semantic_kind, lookup_error>;

		auto semantic_tokens_in(
			const std::filesystem::path& file,
			std::uint32_t line_begin,
			std::uint32_t line_end
		) const -> std::vector<positioned_kind>;

		auto channel_links() const -> std::vector<analysis::channel_use>;

		auto merge_file_symbols(
			const std::filesystem::path& file,
			std::span<const analysis::symbol_token> syms,
			std::span<const analysis::symbol_ref> refs,
			std::span<const analysis::param_token> params
		) -> void;

		auto query_snapshot() const -> std::shared_ptr<const search_snapshot>;
	};

	struct index_merge_request {
		std::filesystem::path path;
		std::vector<analysis::symbol_token> symbols;
		std::vector<analysis::symbol_ref> refs;
		std::vector<analysis::param_token> params;
	};

	struct index_file_update_request {
		std::filesystem::path path;
	};

	struct lookup_probe {
		std::filesystem::path file;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::uint32_t length = 0;
		std::string ident;
		std::string row;
		std::string context;
	};

	auto report_lookup_failure(
		const index_state& index,
		const lookup_probe& probe,
		std::span<const std::string> causes
	) -> void;
}

namespace gse::ide::search {
	enum class symbol_selection_mode {
		definition,
		declaration
	};

	struct symbol_selection {
		const symbol_entry* value = nullptr;
		bool ambiguous = false;
	};

	struct symbol_batch_request {
		std::span<const analysis::compilation_entry* const> entries;
		const std::filesystem::path& plugin_dll;
		std::span<const std::filesystem::path> roots;
		std::size_t workers = 0;
		index_phase phase = index_phase::compiling;
	};

	constexpr std::uint32_t tu_cache_magic = 0x47535455;
	constexpr std::uint32_t tu_cache_version = 11;

	using interned_file_cache = std::unordered_map<std::string, file_id, transparent_hash, transparent_equal>;
	using indexed_path_cache = std::unordered_map<std::string, bool, transparent_hash, transparent_equal>;
	using file_fingerprint_cache = std::unordered_map<std::string, std::uint64_t, transparent_hash, transparent_equal>;

	auto build_files_and_content(
		index_state& index,
		std::span<const index_root> roots
	) -> void;

	auto build_symbols(
		index_state& index,
		std::stop_token stop
	) -> void;

	auto start_symbol_worker(
		index_state& index
	) -> void;

	auto request_symbol_build(
		index_state& index
	) -> void;

	auto publish_file_build(
		index_state& index
	) -> void;

	auto publish_symbol_build(
		index_state& index
	) -> void;

	auto update_file(
		index_state& index,
		const std::filesystem::path& path
	) -> void;

	auto is_indexed_path(
		const std::filesystem::path& root,
		const std::filesystem::path& path
	) -> bool;

	auto is_symbol_source(
		const std::filesystem::path& path
	) -> bool;

	auto owning_root(
		std::span<const index_root> roots,
		const std::filesystem::path& path
	) -> const index_root*;

	auto unexpected_lookup(
		lookup_failure reason,
		std::string subject = {}
	) -> std::unexpected<lookup_error>;

	auto is_skipped_dir(
		std::string_view name
	) -> bool;

	auto is_binary_ext(
		std::string_view ext
	) -> bool;

	auto is_cpp_ext(
		std::string_view ext
	) -> bool;

	auto source_dir_origin(
		std::string_view name
	) -> std::optional<loc_origin>;

	auto loc_language_of(
		std::string_view ext
	) -> std::optional<loc_language>;

	auto classify_loc(
		const index_root& owner,
		const std::filesystem::path& path
	) -> std::optional<loc_bucket>;

	auto add_loc(
		loc_counts& counts,
		loc_bucket bucket,
		std::uint64_t lines
	) -> void;

	auto compute_line_starts(
		std::string_view blob
	) -> std::vector<std::uint32_t>;

	auto canonical_path_id(
		const std::filesystem::path& path
	) -> std::pair<std::filesystem::path, id>;

	auto module_ident_start(
		char ch
	) -> bool;

	auto module_ident_continue(
		char ch
	) -> bool;

	auto skip_module_ws(
		std::string_view text,
		std::size_t pos
	) -> std::size_t;

	auto consume_module_keyword(
		std::string_view text,
		std::size_t& pos,
		std::string_view keyword
	) -> bool;

	auto parse_module_name(
		std::string_view text,
		std::size_t pos
	) -> std::optional<std::pair<std::string, std::uint32_t>>;

	auto line_at(
		std::string_view blob,
		std::span<const std::uint32_t> starts,
		std::size_t line
	) -> std::string_view;

	auto parse_module_declaration(
		std::string_view line
	) -> std::optional<std::pair<std::string, std::uint32_t>>;

	auto scan_modules(
		const std::filesystem::path& path,
		std::string_view blob,
		std::span<const std::uint32_t> starts,
		module_index& modules
	) -> void;

	auto symbol_location(
		const symbol_index& index,
		const symbol_entry& symbol
	) -> location;

	auto rebuild_module_lookups(
		module_index& index
	) -> void;

	auto build_symbol_lookups(
		symbol_index& index,
		std::atomic<std::size_t>* progress
	) -> void;

	auto sort_symbol_ids(
		const symbol_index& index,
		std::vector<std::uint32_t>& ids
	) -> void;

	auto sort_xrefs(
		std::vector<xref_entry>& refs
	) -> void;

	auto sort_symbol_locations(
		symbol_index& index,
		std::atomic<std::size_t>* progress
	) -> void;

	auto rebuild_symbol_lookups(
		symbol_index& index
	) -> void;

	auto next_file_search_snapshot(
		index_state& index
	) -> std::shared_ptr<file_search_snapshot>;

	auto next_symbol_search_snapshot(
		index_state& index
	) -> std::shared_ptr<symbol_search_snapshot>;

	auto publish_file_search_snapshot(
		index_state& index
	) -> void;

	auto publish_symbol_search_snapshot(
		index_state& index
	) -> void;

	auto xref_at(
		const symbol_index& index,
		file_id file,
		std::uint32_t line,
		std::uint32_t column
	) -> const xref_entry*;

	auto symbol_at_location(
		const symbol_index& index,
		file_id file,
		std::uint32_t line,
		std::uint32_t column,
		bool contains_column
	) -> const symbol_entry*;

	auto is_lookup_declaration(
		const symbol_entry& symbol
	) -> bool;

	auto same_symbol_candidate(
		const symbol_entry& lhs,
		const symbol_entry& rhs
	) -> bool;

	auto selection_score(
		const symbol_entry& symbol,
		std::string_view needle,
		std::string_view suffix,
		bool qualified,
		std::optional<file_id> click_file,
		symbol_selection_mode mode
	) -> std::optional<int>;

	auto select_symbol(
		const symbol_index& index,
		std::span<const std::uint32_t> candidates,
		std::string_view name,
		std::string_view qualifier,
		std::optional<file_id> click_file,
		symbol_selection_mode mode
	) -> symbol_selection;

	auto matching_definition(
		const symbol_index& index,
		std::string_view identity,
		std::string_view qualified,
		file_id from_file,
		std::uint32_t from_line,
		std::uint32_t from_column
	) -> std::optional<location>;

	auto promoted_definition(
		const symbol_index& index,
		file_id file,
		std::uint32_t line,
		std::uint32_t column,
		std::string_view identity,
		std::string_view qualified
	) -> location;

	auto log_index_phase_completion(
		const index_state& index
	) -> void;

	auto append_entry(
		std::string& out,
		const std::string& entry
	) -> void;

	auto xrefs_on_line(
		const symbol_index& index,
		file_id file,
		std::uint32_t line
	) -> std::string;

	auto symbols_on_line(
		const symbol_index& index,
		file_id file,
		std::uint32_t line
	) -> std::string;

	auto params_on_line(
		const symbol_index& index,
		file_id file,
		std::uint32_t line
	) -> std::string;

	auto begin_index_phase(
		index_state& index,
		index_phase phase,
		std::size_t total
	) -> void;

	auto end_index_progress(
		index_state& index
	) -> void;

	auto run_symbol_batch(
		const symbol_batch_request& request,
		index_state& index,
		const std::atomic<bool>* cancel,
		std::stop_token stop
	) -> std::vector<analysis::tu_symbols>;

	auto intern_cached(
		symbol_index& symbols,
		interned_file_cache& cache,
		const std::string& raw
	) -> file_id;

	auto make_xref_entry(
		file_id definition_file,
		analysis::symbol_ref ref
	) -> xref_entry;

	auto indexed_cached(
		std::span<const index_root> roots,
		const std::string& raw,
		indexed_path_cache& cache
	) -> bool;

	auto file_fingerprint(
		const std::filesystem::path& file,
		file_fingerprint_cache& cache
	) -> std::uint64_t;

	auto tu_fingerprint(
		const analysis::compilation_entry& entry,
		const std::filesystem::path& plugin,
		std::span<const std::filesystem::path> dependencies,
		file_fingerprint_cache& file_fingerprints
	) -> std::uint64_t;

	auto tu_cache_path(
		const index_state& index,
		const analysis::compilation_entry& entry
	) -> std::filesystem::path;

	auto save_tu_cache(
		const std::filesystem::path& path,
		const analysis::compilation_entry& entry,
		const std::filesystem::path& plugin,
		const analysis::tu_symbols& symbols,
		file_fingerprint_cache& file_fingerprints
	) -> std::expected<void, std::string>;

	auto load_tu_cache(
		const std::filesystem::path& path,
		const analysis::compilation_entry& entry,
		const std::filesystem::path& plugin,
		analysis::tu_symbols& out,
		file_fingerprint_cache& file_fingerprints
	) -> bool;

	auto symbol_worker_loop(
		std::stop_token stop,
		index_state* index
	) -> void;

	auto apply_symbol_overlay(
		symbol_index& index,
		const symbol_overlay& overlay,
		bool rebuild_lookups
	) -> void;
}
