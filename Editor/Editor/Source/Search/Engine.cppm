export module gse.ide.search:engine;

import std;
import gse;
import gse.ide.analysis;

import :types;
import :index;
import :fuzzy;

export namespace gse::ide::search {
	struct query_buffer {
		std::atomic<bool> done = false;
		std::uint64_t generation = 0;
		std::string query;
		std::vector<result> results;
	};

	struct engine {
		static auto rank(const index_state& idx, std::string_view query, const options& opts, std::vector<result>& out) -> void;
		static auto submit(const std::shared_ptr<query_buffer>& out, const index_state& idx, std::string query, options opts) -> void;
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

	auto domain_priority(domain source) -> int {
		switch (source) {
			case domain::symbol: return 0;
			case domain::file: return 1;
			case domain::content: return 2;
		}
		return 3;
	}

	auto find_ci(std::string_view haystack, std::string_view needle_lower, std::size_t from) -> std::size_t {
		if (needle_lower.empty() || haystack.size() < needle_lower.size()) {
			return std::string_view::npos;
		}
		const std::size_t last = haystack.size() - needle_lower.size();
		for (std::size_t i = from; i <= last; ++i) {
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

	auto scan_blob(std::string_view blob, const std::vector<std::uint32_t>& starts, std::string_view q_lower, const std::filesystem::path& path, std::vector<result>& sink) -> void {
		std::uint32_t hits = 0;
		std::size_t pos = 0;
		while (hits < max_hits_per_file) {
			const std::size_t found = find_ci(blob, q_lower, pos);
			if (found == std::string_view::npos) {
				break;
			}
			const auto up = std::ranges::upper_bound(starts, static_cast<std::uint32_t>(found));
			const std::size_t line_idx = static_cast<std::size_t>(std::distance(starts.begin(), up)) - 1;
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
				.display = std::string(display_view.substr(0, max_display_chars)),
			};
			if (col >= trim) {
				const std::uint32_t disp_col = col - static_cast<std::uint32_t>(trim);
				if (disp_col < r.display.size()) {
					const auto len = static_cast<std::uint32_t>(std::min<std::size_t>(q_lower.size(), r.display.size() - disp_col));
					r.highlight.push_back({ .start = disp_col, .length = len });
				}
			}
			sink.push_back(std::move(r));

			++hits;
			pos = found + 1;
		}
	}

	auto scan_content(const index_state& idx, std::string_view q_lower, std::vector<result>& out) -> void {
		if (!idx.content.loaded.load(std::memory_order_acquire)) {
			return;
		}
		const std::size_t n = idx.content.blobs.size();
		std::vector<std::vector<result>> per_file(n);

		gse::task::coarse_parallel(n, 4, [&](std::size_t i) {
			scan_blob(idx.content.blobs[i], idx.content.line_starts[i], q_lower, idx.content.paths[i], per_file[i]);
		});

		for (std::vector<result>& v : per_file) {
			for (result& r : v) {
				out.push_back(std::move(r));
			}
		}
	}
}

auto gse::ide::search::engine::rank(const index_state& idx, std::string_view query, const options& opts, std::vector<result>& out) -> void {
	if (query.empty()) {
		return;
	}
	const std::string q_lower = to_lower(query);
	std::shared_lock lock(idx.mutex);

	if (opts.include_symbols) {
		for (const symbol_entry& s : idx.symbols.symbols) {
			const score_result sc = fuzzy_match(q_lower, s.name, s.name_lower);
			if (!sc.matched) {
				continue;
			}
			out.push_back({
				.source = domain::symbol,
				.score = score_symbol_base + score_symbol_fuzzy * sc.score,
				.path = idx.symbols.path_for(s.file),
				.line = s.line,
				.column = s.column,
				.display = s.name,
				.detail = std::string(gse::enum_to_string(s.kind)),
				.highlight = sc.ranges,
			});
		}
	}

	if (opts.include_files) {
		for (const file_entry& f : idx.files.entries) {
			const score_result sc = fuzzy_match(q_lower, f.rel, f.rel_lower);
			if (!sc.matched) {
				continue;
			}
			out.push_back({
				.source = domain::file,
				.score = score_file_base + score_file_fuzzy * sc.score,
				.path = f.path,
				.display = f.rel,
				.highlight = sc.ranges,
			});
		}
	}

	if (opts.include_content && !opts.content_is_regex) {
		scan_content(idx, q_lower, out);
	}

	std::ranges::sort(out, [](const result& a, const result& b) {
		if (a.score != b.score) {
			return a.score > b.score;
		}
		if (a.source != b.source) {
			return domain_priority(a.source) < domain_priority(b.source);
		}
		if (a.path.native() != b.path.native()) {
			return a.path.native() < b.path.native();
		}
		return a.line < b.line;
	});

	if (out.size() > opts.max_results) {
		out.resize(opts.max_results);
	}
}

auto gse::ide::search::engine::submit(const std::shared_ptr<query_buffer>& out, const index_state& idx, std::string query, options opts) -> void {
	gse::task::post([out, &idx, query = std::move(query), opts] {
		std::vector<result> results;
		engine::rank(idx, query, opts, results);
		out->results = std::move(results);
		out->done.store(true, std::memory_order_release);
	});
}
