export module gse.ide.search:engine;

import std;
import gse;
import gse.ide.analysis;

import :types;
import :index;
import :fuzzy;

namespace gse::ide::search {
	struct query_buffer {
		std::atomic<bool> done = false;
		std::atomic<bool> cancelled = false;
		std::uint64_t request_id = 0;
		std::uint64_t index_generation = 0;
		std::vector<result> results;
	};

	struct engine {
		static auto rank(
			const search_snapshot& snapshot,
			std::string_view query,
			const options& opts,
			std::vector<result>& out,
			const std::atomic<bool>& cancelled
		) -> void;

		static auto submit(
			const std::shared_ptr<query_buffer>& out,
			std::shared_ptr<const search_snapshot> snapshot,
			std::string query,
			options opts
		) -> void;
	};
}

namespace gse::ide::search {
	constexpr float score_symbol_base = 0.60f;
	constexpr float score_symbol_fuzzy = 0.40f;
	constexpr float score_file_base = 0.45f;
	constexpr float score_file_fuzzy = 0.40f;
	constexpr float score_content_base = 0.25f;
	constexpr std::uint32_t max_hits_per_file = 20;
	constexpr std::size_t max_display_chars = 200;

	struct result_key {
		domain source = domain::file;
		float score = 0.f;
		const std::filesystem::path* path = nullptr;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
	};

	struct bounded_results {
		std::size_t limit = 0;
		std::vector<result> values;

		auto accepts(
			const result_key& candidate
		) const -> bool;

		auto add_accepted(
			result value
		) -> void;
	};

	auto domain_priority(
		domain source
	) -> int;

	auto key_of(
		const result& value
	) -> result_key;

	auto key_better(
		const result_key& a,
		const result_key& b
	) -> bool;

	auto result_better(
		const result& a,
		const result& b
	) -> bool;

	auto find_ci(
		std::string_view haystack,
		std::string_view needle_lower,
		std::size_t from,
		const std::atomic<bool>& cancelled
	) -> std::size_t;

	auto scan_blob(
		std::string_view blob,
		std::span<const std::uint32_t> starts,
		std::string_view q_lower,
		const std::filesystem::path& path,
		std::vector<result>& sink,
		const std::atomic<bool>& cancelled
	) -> void;

	auto scan_content(
		const search_snapshot& snapshot,
		std::string_view q_lower,
		bounded_results& out,
		const std::atomic<bool>& cancelled
	) -> void;
}

auto gse::ide::search::domain_priority(const domain source) -> int {
	return annotation_from_enum<domain_info>(source, {
		.priority = 3,
	}).priority;
}

auto gse::ide::search::key_of(const result& value) -> result_key {
	return {
		.source = value.source,
		.score = value.score,
		.path = &value.path,
		.line = value.line,
		.column = value.column,
	};
}

auto gse::ide::search::key_better(const result_key& a, const result_key& b) -> bool {
	if (a.score != b.score) {
		return a.score > b.score;
	}
	if (a.source != b.source) {
		return domain_priority(a.source) < domain_priority(b.source);
	}
	if (a.path->native() != b.path->native()) {
		return a.path->native() < b.path->native();
	}
	if (a.line != b.line) {
		return a.line < b.line;
	}
	return a.column < b.column;
}

auto gse::ide::search::result_better(const result& a, const result& b) -> bool {
	return key_better(key_of(a), key_of(b));
}

auto gse::ide::search::bounded_results::accepts(const result_key& candidate) const -> bool {
	if (values.size() < limit) {
		return true;
	}
	return key_better(candidate, key_of(values.front()));
}

auto gse::ide::search::bounded_results::add_accepted(result value) -> void {
	if (values.size() < limit) {
		values.push_back(std::move(value));
		std::ranges::push_heap(values, result_better);
		return;
	}
	std::ranges::pop_heap(values, result_better);
	values.back() = std::move(value);
	std::ranges::push_heap(values, result_better);
}

auto gse::ide::search::find_ci(const std::string_view haystack, const std::string_view needle_lower, const std::size_t from, const std::atomic<bool>& cancelled) -> std::size_t {
	if (needle_lower.empty() || haystack.size() < needle_lower.size()) {
		return std::string_view::npos;
	}
	const std::size_t last = haystack.size() - needle_lower.size();
	for (std::size_t i = from; i <= last; ++i) {
		if ((i & 4095u) == 0 && cancelled.load(std::memory_order_relaxed)) {
			return std::string_view::npos;
		}
		std::size_t j = 0;
		for (; j < needle_lower.size(); ++j) {
			if (static_cast<char>(std::tolower(static_cast<unsigned char>(haystack[i + j]))) != needle_lower[j]) {
				break;
			}
		}
		if (j == needle_lower.size()) {
			return i;
		}
	}
	return std::string_view::npos;
}

auto gse::ide::search::scan_blob(const std::string_view blob, const std::span<const std::uint32_t> starts, const std::string_view q_lower, const std::filesystem::path& path, std::vector<result>& sink, const std::atomic<bool>& cancelled) -> void {
	std::uint32_t hits = 0;
	std::size_t pos = 0;
	while (hits < max_hits_per_file && !cancelled.load(std::memory_order_relaxed)) {
		const std::size_t found = find_ci(blob, q_lower, pos, cancelled);
		if (found == std::string_view::npos) {
			break;
		}
		const auto up = std::ranges::upper_bound(starts, static_cast<std::uint32_t>(found));
		const std::size_t line_idx = static_cast<std::size_t>(std::ranges::distance(starts.begin(), up)) - 1;
		const std::uint32_t line_start = starts[line_idx];
		const std::uint32_t line_end = line_idx + 1 < starts.size() ? starts[line_idx + 1] - 1 : static_cast<std::uint32_t>(blob.size());
		const std::uint32_t col = static_cast<std::uint32_t>(found) - line_start;

		std::string_view line_view = blob.substr(line_start, line_end - line_start);
		std::size_t trim = 0;
		while (trim < line_view.size() && (line_view[trim] == ' ' || line_view[trim] == '\t')) {
			++trim;
		}
		const std::string_view display_view = line_view.substr(trim);

		result r{
			.source = domain::content,
			.score = score_content_base,
			.path = path,
			.line = static_cast<std::uint32_t>(line_idx),
			.column = col,
			.length = static_cast<std::uint32_t>(q_lower.size()),
			.display = std::string(display_view.substr(0, max_display_chars)),
		};
		if (col >= trim) {
			const std::uint32_t disp_col = col - static_cast<std::uint32_t>(trim);
			if (disp_col < r.display.size()) {
				const auto len = static_cast<std::uint32_t>(std::min<std::size_t>(q_lower.size(), r.display.size() - disp_col));
				r.highlight.push_back({
					.start = disp_col,
					.length = len,
				});
			}
		}
		sink.push_back(std::move(r));

		++hits;
		pos = found + 1;
	}
}

auto gse::ide::search::scan_content(const search_snapshot& snapshot, const std::string_view q_lower, bounded_results& out, const std::atomic<bool>& cancelled) -> void {
	if (cancelled.load(std::memory_order_relaxed)) {
		return;
	}
	const std::span<const std::shared_ptr<const content_entry>> entries = *snapshot.files->content;
	std::mutex result_mutex;

	task::coarse_parallel(entries.size(), 4, [&](std::size_t i) {
		if (!cancelled.load(std::memory_order_relaxed)) {
			const content_entry& entry = *entries[i];
			std::vector<result> local;
			local.reserve(max_hits_per_file);
			scan_blob(entry.blob, entry.line_starts, q_lower, entry.path, local, cancelled);
			std::lock_guard lock(result_mutex);
			for (result& match : local) {
				if (out.accepts(key_of(match))) {
					out.add_accepted(std::move(match));
				}
			}
		}
	});
}

auto gse::ide::search::engine::rank(const search_snapshot& snapshot, const std::string_view query, const options& opts, std::vector<result>& out, const std::atomic<bool>& cancelled) -> void {
	if (query.empty() || opts.max_results == 0 || cancelled.load(std::memory_order_relaxed)) {
		return;
	}
	const std::string q_lower = to_lower(query);
	bounded_results matches{
		.limit = opts.max_results,
	};
	matches.values.reserve(opts.max_results);

	if (opts.include_symbols) {
		for (const searchable_symbol& s : *snapshot.symbols->symbols) {
			if (cancelled.load(std::memory_order_relaxed)) {
				return;
			}
			const score_result sc = fuzzy_match(q_lower, s.name, s.name_lower, false);
			if (!sc.matched) {
				continue;
			}
			const float score = score_symbol_base + score_symbol_fuzzy * sc.score;
			const result_key key{
				.source = domain::symbol,
				.score = score,
				.path = &s.path,
				.line = s.line,
				.column = s.column,
			};
			if (!matches.accepts(key)) {
				continue;
			}
			matches.add_accepted({
				.source = domain::symbol,
				.score = score,
				.path = s.path,
				.line = s.line,
				.column = s.column,
				.length = static_cast<std::uint32_t>(s.name.size()),
				.display = s.name,
				.detail = std::format("{}", s.kind),
			});
		}
	}

	if (opts.include_files) {
		for (const file_entry& f : *snapshot.files->files) {
			if (cancelled.load(std::memory_order_relaxed)) {
				return;
			}
			const score_result sc = fuzzy_match_path(q_lower, f.rel, f.rel_lower, false);
			if (!sc.matched) {
				continue;
			}
			const float score = score_file_base + score_file_fuzzy * sc.score;
			const result_key key{
				.source = domain::file,
				.score = score,
				.path = &f.path,
			};
			if (!matches.accepts(key)) {
				continue;
			}
			matches.add_accepted({
				.source = domain::file,
				.score = score,
				.path = f.path,
				.display = f.rel,
			});
		}
	}

	if (opts.include_content) {
		scan_content(snapshot, q_lower, matches, cancelled);
	}

	if (cancelled.load(std::memory_order_relaxed)) {
		return;
	}
	std::ranges::sort(matches.values, result_better);
	for (result& match : matches.values) {
		if (match.source == domain::symbol) {
			const std::string display_lower = to_lower(match.display);
			match.highlight = fuzzy_match(q_lower, match.display, display_lower).ranges;
		}
		else if (match.source == domain::file) {
			const std::string display_lower = to_lower(match.display);
			match.highlight = fuzzy_match_path(q_lower, match.display, display_lower).ranges;
		}
	}
	out = std::move(matches.values);
}

auto gse::ide::search::engine::submit(const std::shared_ptr<query_buffer>& out, std::shared_ptr<const search_snapshot> snapshot, std::string query, const options opts) -> void {
	task::post([out, snapshot = std::move(snapshot), query = std::move(query), opts] {
		std::vector<result> results;
		engine::rank(*snapshot, query, opts, results, out->cancelled);
		if (!out->cancelled.load(std::memory_order_acquire)) {
			out->results = std::move(results);
		}
		out->done.store(true, std::memory_order_release);
	});
}
