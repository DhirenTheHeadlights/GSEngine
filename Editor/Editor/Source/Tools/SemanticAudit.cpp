import std;

import gse;
import gse.ide.analysis;

namespace gse::ide::audit {
	using analysis::identifier_context;
	using analysis::identifier_use;
	using analysis::semantic_kind;
	using analysis::semantic_token;
	using analysis::symbol_ref;

	enum class miss_kind {
		uncoloured,
		unreferenced
	};

	enum class bucket {
		template_header,
		nested_name_specifier,
		trailing_return_head,
		lambda_capture,
		lambda_parameter,
		structured_binding,
		requires_parameter,
		requires_body,
		lambda_body,
		declaration,
		other
	};

	enum class bracket_role {
		none,
		lambda,
		structured_binding
	};

	struct options {
		std::filesystem::path database;
		std::filesystem::path plugin;
		std::filesystem::path root;
		std::filesystem::path report = "semantic_audit.txt";
		std::string filter;
		std::size_t jobs = 0;
		std::size_t per_bucket = 12;
	};

	struct miss {
		gse::id file;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::uint32_t length = 0;
		miss_kind kind = miss_kind::uncoloured;
		bucket group = bucket::other;
	};

	struct totals {
		std::size_t files = 0;
		std::size_t identifiers = 0;
	};

	struct tally {
		std::size_t uncoloured = 0;
		std::size_t unreferenced = 0;

		[[nodiscard]] auto total() const -> std::size_t;
	};

	struct file_audit {
		std::vector<miss> misses;
		std::size_t identifiers = 0;
	};

	struct sweep_result {
		std::vector<miss> misses;
		std::vector<std::string> failures;
		std::unordered_map<gse::id, std::filesystem::path> paths;
		totals counted;
	};

	struct span {
		std::uint32_t start = 0;
		std::uint32_t end = 0;
		bool referenceable = true;
	};

	auto usage() -> void;

	auto parse_options(
		std::span<const std::string_view> args,
		options& out
	) -> bool;

	auto read_lines(
		const std::filesystem::path& path
	) -> std::vector<std::string>;

	auto same_file(
		std::string_view left,
		std::string_view right
	) -> bool;

	auto covering(
		std::span<const span> spans,
		const identifier_use& use
	) -> const span*;

	auto trimmed_view(
		std::string_view row
	) -> std::string_view;

	auto bracket_role_at(
		std::string_view row,
		std::size_t at
	) -> bracket_role;

	auto enclosing_paren(
		std::string_view row,
		std::size_t at
	) -> std::size_t;

	auto preceded_by_arrow(
		std::string_view row,
		std::size_t at
	) -> bool;

	auto classify(
		std::string_view row,
		const identifier_use& use
	) -> bucket;

	auto audit_file(
		const std::filesystem::path& path,
		const analysis::tu_audit& analysed
	) -> file_audit;

	auto sweep(
		const options& opts,
		std::span<const analysis::compilation_entry* const> selected
	) -> sweep_result;

	auto write_report(
		const options& opts,
		const sweep_result& result
	) -> void;
}

auto gse::ide::audit::tally::total() const -> std::size_t {
	return uncoloured + unreferenced;
}

auto gse::ide::audit::usage() -> void {
	std::println(std::cerr, "usage: SemanticAudit --db <compile_commands.json> --plugin <gse_tokens.dll> --root <dir>");
	std::println(std::cerr, "                     [--out <report>] [--filter <substring>] [--jobs <n>] [--per-bucket <n>]");
}

auto gse::ide::audit::parse_options(const std::span<const std::string_view> args, options& out) -> bool {
	for (std::size_t i = 0; i < args.size(); ++i) {
		const std::string_view flag = args[i];
		const bool has_value = i + 1 < args.size();
		if (flag == "--db" && has_value) {
			out.database = args[++i];
		}
		else if (flag == "--plugin" && has_value) {
			out.plugin = args[++i];
		}
		else if (flag == "--root" && has_value) {
			out.root = args[++i];
		}
		else if (flag == "--out" && has_value) {
			out.report = args[++i];
		}
		else if (flag == "--filter" && has_value) {
			out.filter = args[++i];
		}
		else if (flag == "--jobs" && has_value) {
			std::from_chars(args[i + 1].data(), args[i + 1].data() + args[i + 1].size(), out.jobs);
			++i;
		}
		else if (flag == "--per-bucket" && has_value) {
			std::from_chars(args[i + 1].data(), args[i + 1].data() + args[i + 1].size(), out.per_bucket);
			++i;
		}
		else {
			std::println(std::cerr, "unrecognised argument: {}", flag);
			return false;
		}
	}
	if (out.database.empty() || out.plugin.empty() || out.root.empty()) {
		return false;
	}
	std::error_code ec;
	out.database = std::filesystem::absolute(out.database, ec).lexically_normal();
	out.plugin = std::filesystem::absolute(out.plugin, ec).lexically_normal();
	out.root = std::filesystem::absolute(out.root, ec).lexically_normal();
	if (!std::filesystem::exists(out.plugin, ec)) {
		std::println(std::cerr, "plugin not found: {}", out.plugin.generic_display_string());
		return false;
	}
	return true;
}

auto gse::ide::audit::read_lines(const std::filesystem::path& path) -> std::vector<std::string> {
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return {};
	}
	std::ostringstream stream;
	stream << in.rdbuf();
	const std::string text = stream.str();

	std::vector<std::string> lines;
	std::size_t start = 0;
	while (start <= text.size()) {
		const std::size_t eol = text.find('\n', start);
		const std::size_t stop = eol == std::string::npos ? text.size() : eol;
		std::string row = text.substr(start, stop - start);
		if (!row.empty() && row.back() == '\r') {
			row.pop_back();
		}
		lines.push_back(std::move(row));
		if (eol == std::string::npos) {
			break;
		}
		start = eol + 1;
	}
	return lines;
}

auto gse::ide::audit::same_file(const std::string_view left, const std::string_view right) -> bool {
	if (left.size() != right.size()) {
		return false;
	}
	for (std::size_t i = 0; i < left.size(); ++i) {
		const char a = left[i] == '\\' ? '/' : left[i];
		const char b = right[i] == '\\' ? '/' : right[i];
		if (a != b) {
			return false;
		}
	}
	return true;
}

auto gse::ide::audit::covering(const std::span<const span> spans, const identifier_use& use) -> const span* {
	const std::uint32_t start = use.column - 1;
	const std::uint32_t end = start + use.length;
	for (const span& candidate : spans) {
		if (start < candidate.end && end > candidate.start) {
			return &candidate;
		}
	}
	return nullptr;
}

auto gse::ide::audit::trimmed_view(const std::string_view row) -> std::string_view {
	std::size_t first = 0;
	while (first < row.size() && (row[first] == ' ' || row[first] == '\t')) {
		++first;
	}
	return row.substr(first);
}

auto gse::ide::audit::bracket_role_at(const std::string_view row, const std::size_t at) -> bracket_role {
	int depth = 0;
	for (std::size_t i = at; i-- > 0;) {
		if (row[i] == ']') {
			++depth;
			continue;
		}
		if (row[i] != '[') {
			continue;
		}
		if (depth > 0) {
			--depth;
			continue;
		}
		if ((i + 1 < row.size() && row[i + 1] == '[') || (i > 0 && row[i - 1] == '[')) {
			return bracket_role::none;
		}
		std::size_t before = i;
		for (bool moved = true; moved;) {
			moved = false;
			while (before > 0 && (row[before - 1] == ' ' || row[before - 1] == '\t')) {
				--before;
				moved = true;
			}
			if (before > 0 && (row[before - 1] == '&' || row[before - 1] == '*')) {
				--before;
				moved = true;
			}
		}
		if (before == 0) {
			return bracket_role::lambda;
		}
		const char preceding = row[before - 1];
		if (preceding == ']' || preceding == ')') {
			return bracket_role::none;
		}
		if (preceding != '_' && std::isalnum(static_cast<unsigned char>(preceding)) == 0) {
			return bracket_role::lambda;
		}
		std::size_t word = before;
		while (word > 0 && (row[word - 1] == '_' || std::isalnum(static_cast<unsigned char>(row[word - 1])) != 0)) {
			--word;
		}
		return row.substr(word, before - word) == "auto" ? bracket_role::structured_binding : bracket_role::none;
	}
	return bracket_role::none;
}

auto gse::ide::audit::enclosing_paren(const std::string_view row, const std::size_t at) -> std::size_t {
	int depth = 0;
	for (std::size_t i = at; i-- > 0;) {
		if (row[i] == ')') {
			++depth;
			continue;
		}
		if (row[i] != '(') {
			continue;
		}
		if (depth == 0) {
			return i;
		}
		--depth;
	}
	return std::string_view::npos;
}

auto gse::ide::audit::preceded_by_arrow(const std::string_view row, const std::size_t at) -> bool {
	std::size_t i = at;
	while (i > 0 && (row[i - 1] == ' ' || row[i - 1] == '\t')) {
		--i;
	}
	return i >= 2 && row[i - 1] == '>' && row[i - 2] == '-';
}

auto gse::ide::audit::classify(const std::string_view row, const identifier_use& use) -> bucket {
	const std::size_t start = use.column - 1;
	const std::size_t stop = start + use.length;
	const std::string_view trimmed = trimmed_view(row);

	if (trimmed.starts_with("template") && row.find('<') < start) {
		return bucket::template_header;
	}
	if (stop + 1 < row.size() && row[stop] == ':' && row[stop + 1] == ':') {
		return bucket::nested_name_specifier;
	}
	if (preceded_by_arrow(row, start)) {
		return bucket::trailing_return_head;
	}
	switch (bracket_role_at(row, start)) {
		case bracket_role::lambda:
			return bucket::lambda_capture;
		case bracket_role::structured_binding:
			return bucket::structured_binding;
		case bracket_role::none:
			break;
	}
	if (const std::size_t paren = enclosing_paren(row, start); paren != std::string_view::npos) {
		std::size_t before = paren;
		while (before > 0 && (row[before - 1] == ' ' || row[before - 1] == '\t')) {
			--before;
		}
		if (before > 0 && row[before - 1] == ']') {
			return bucket::lambda_parameter;
		}
		if (trimmed.starts_with("requires")) {
			return bucket::requires_parameter;
		}
	}
	if (use.context == identifier_context::requires_expression) {
		return bucket::requires_body;
	}
	if (use.context == identifier_context::lambda_body) {
		return bucket::lambda_body;
	}
	if (row.find('{') == std::string_view::npos && row.find(';') != std::string_view::npos) {
		return bucket::declaration;
	}
	return bucket::other;
}

auto gse::ide::audit::audit_file(const std::filesystem::path& path, const analysis::tu_audit& analysed) -> file_audit {
	const std::vector<std::string> lines = read_lines(path);
	if (lines.empty()) {
		return {};
	}

	std::unordered_map<std::uint32_t, std::vector<span>> highlights;
	for (const semantic_token& token : analysed.highlights) {
		if (token.line == 0 || token.column == 0) {
			continue;
		}
		highlights[token.line].push_back({
			.start = token.column - 1,
			.end = token.column - 1 + token.length,
			.referenceable = token.kind != semantic_kind::attribute,
		});
	}

	const std::string key = path.generic_display_string();
	std::unordered_map<std::uint32_t, std::vector<span>> references;
	for (const symbol_ref& ref : analysed.symbols.set.refs) {
		if (ref.line == 0 || ref.column == 0 || !same_file(ref.file, key)) {
			continue;
		}
		references[ref.line].push_back({
			.start = ref.column - 1,
			.end = ref.column - 1 + ref.length,
		});
	}

	const gse::id identity = gse::generate_temp_id(path);
	file_audit out;
	out.identifiers = analysed.identifiers.size();

	for (const identifier_use& use : analysed.identifiers) {
		if (use.line == 0 || use.line > lines.size() || use.column == 0) {
			continue;
		}
		const auto highlight_row = highlights.find(use.line);
		const auto reference_row = references.find(use.line);
		const span* highlight = highlight_row == highlights.end() ? nullptr : covering(highlight_row->second, use);
		const span* reference = reference_row == references.end() ? nullptr : covering(reference_row->second, use);
		if (highlight && (reference || !highlight->referenceable)) {
			continue;
		}
		out.misses.push_back({
			.file = identity,
			.line = use.line,
			.column = use.column,
			.length = use.length,
			.kind = highlight ? miss_kind::unreferenced : miss_kind::uncoloured,
			.group = classify(lines[use.line - 1], use),
		});
	}
	return out;
}

auto gse::ide::audit::sweep(const options& opts, const std::span<const analysis::compilation_entry* const> selected) -> sweep_result {
	const std::size_t jobs = opts.jobs != 0 ? opts.jobs : std::max<std::size_t>(1, std::thread::hardware_concurrency() / 2);
	std::println("auditing {} translation units across {} jobs", selected.size(), jobs);

	const std::array<std::filesystem::path, 1> roots{
		opts.root,
	};
	const analysis::symbol_request request{
		.plugin_dll = opts.plugin,
		.workspace_roots = roots,
	};

	std::atomic<std::size_t> next = 0;
	std::atomic<std::size_t> done = 0;
	std::mutex guard;
	sweep_result result;

	{
		std::vector<std::jthread> workers;
		for (std::size_t w = 0; w < jobs; ++w) {
			workers.emplace_back([&] {
				for (;;) {
					const std::size_t index = next.fetch_add(1, std::memory_order_relaxed);
					if (index >= selected.size()) {
						return;
					}
					const analysis::compilation_entry& entry = *selected[index];
					const analysis::tu_audit analysed = analysis::symbol_index_builder::audit_one(entry, request);

					file_audit audited;
					std::string failure;
					if (analysed.symbols.failure != analysis::symbol_index_failure::none) {
						failure = std::format(
							"{}: {}{}",
							entry.file.generic_display_string(),
							analysis::describe(analysed.symbols.failure),
							analysed.symbols.failure_detail.empty() ? std::string{} : ": " + analysed.symbols.failure_detail
						);
					}
					else {
						audited = audit_file(entry.file, analysed);
					}

					const std::lock_guard held(guard);
					if (failure.empty()) {
						++result.counted.files;
						result.counted.identifiers += audited.identifiers;
						result.paths.emplace(gse::generate_temp_id(entry.file), entry.file);
						result.misses.insert(result.misses.end(), audited.misses.begin(), audited.misses.end());
					}
					else {
						result.failures.push_back(std::move(failure));
					}
					const std::size_t finished = done.fetch_add(1, std::memory_order_relaxed) + 1;
					if (finished % 25 == 0 || finished == selected.size()) {
						std::println("  {}/{}", finished, selected.size());
					}
				}
			});
		}
	}

	std::ranges::sort(result.misses, [](const miss& l, const miss& r) {
		return std::tie(l.file, l.line, l.column) < std::tie(r.file, r.line, r.column);
	});
	return result;
}

auto gse::ide::audit::write_report(const options& opts, const sweep_result& result) -> void {
	std::map<bucket, tally> by_bucket;
	std::unordered_map<gse::id, tally> by_file;
	for (const miss& entry : result.misses) {
		tally& group = by_bucket[entry.group];
		tally& file = by_file[entry.file];
		if (entry.kind == miss_kind::uncoloured) {
			++group.uncoloured;
			++file.uncoloured;
		}
		else {
			++group.unreferenced;
			++file.unreferenced;
		}
	}

	std::ofstream out(opts.report, std::ios::binary | std::ios::trunc);
	std::println(out, "semantic audit: {} files, {} identifiers, {} misses", result.counted.files, result.counted.identifiers, result.misses.size());
	std::println(out, "");

	if (!result.failures.empty()) {
		std::println(out, "== translation units that did not analyse ({}) ==", result.failures.size());
		for (const std::string& failure : result.failures) {
			std::println(out, "  {}", failure);
		}
		std::println(out, "");
	}

	std::vector<std::pair<bucket, tally>> ranked(by_bucket.begin(), by_bucket.end());
	std::ranges::sort(ranked, std::ranges::greater{}, [](const auto& row) {
		return row.second.total();
	});

	std::println(out, "== classes, worst first ==");
	for (const auto& [group, counts] : ranked) {
		std::println(out, "  {:<24} {:>6} total  ({} uncoloured, {} unreferenced)", gse::enum_to_string(group), counts.total(), counts.uncoloured, counts.unreferenced);
	}
	std::println(out, "");

	std::vector<std::pair<gse::id, tally>> worst(by_file.begin(), by_file.end());
	std::ranges::sort(worst, std::ranges::greater{}, [](const auto& row) {
		return row.second.total();
	});
	std::println(out, "== worst files ==");
	for (const auto& [file, counts] : worst | std::views::take(25)) {
		std::println(out, "  {:>6}  {}", counts.total(), result.paths.at(file).generic_display_string());
	}
	std::println(out, "");

	std::unordered_map<gse::id, std::vector<std::string>> sources;
	for (const auto& [group, counts] : ranked) {
		std::println(out, "== {} ({} uncoloured, {} unreferenced) ==", group, counts.uncoloured, counts.unreferenced);
		std::size_t shown = 0;
		for (const miss& entry : result.misses) {
			if (entry.group != group || shown >= opts.per_bucket) {
				continue;
			}
			++shown;
			const std::filesystem::path& path = result.paths.at(entry.file);
			auto lines = sources.find(entry.file);
			if (lines == sources.end()) {
				lines = sources.emplace(entry.file, read_lines(path)).first;
			}
			std::println(out, "  {} at {}:{}:{}", entry.kind, path.generic_display_string(), entry.line, entry.column);
			if (entry.line <= lines->second.size()) {
				std::println(out, "    |{}|", lines->second[entry.line - 1]);
			}
		}
		std::println(out, "");
	}
}

auto main(int argc, char** argv) -> int {
	using namespace gse::ide;

	std::vector<std::string_view> args;
	for (int i = 1; i < argc; ++i) {
		args.emplace_back(argv[i]);
	}

	audit::options opts;
	if (!audit::parse_options(args, opts)) {
		audit::usage();
		return 2;
	}

	const std::shared_ptr<const analysis::compilation_database> database = analysis::load_compilation_database(opts.database);
	if (!database) {
		std::println(std::cerr, "could not load a compilation database from {}", opts.database.generic_display_string());
		return 1;
	}

	std::vector<const analysis::compilation_entry*> selected;
	for (const analysis::compilation_entry& entry : database->entries.items()) {
		if (opts.filter.empty() || entry.file.generic_display_string().contains(opts.filter)) {
			selected.push_back(&entry);
		}
	}
	if (selected.empty()) {
		std::println(std::cerr, "no translation units matched");
		return 1;
	}

	const audit::sweep_result result = audit::sweep(opts, selected);
	audit::write_report(opts, result);
	std::println(
		"{} misses over {} identifiers in {} files -> {}",
		result.misses.size(),
		result.counted.identifiers,
		result.counted.files,
		opts.report.generic_display_string()
	);
	return 0;
}
