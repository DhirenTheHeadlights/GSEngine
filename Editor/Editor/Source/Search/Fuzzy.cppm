export module gse.ide.search:fuzzy;

import std;

import gse.complete;

import :types;

namespace gse::ide::search {
	using complete::score_result;
	using complete::fuzzy_match;

	auto fuzzy_match_path(
		std::string_view query_lower,
		std::string_view rel,
		std::string_view rel_lower,
		bool capture_ranges = true
	) -> score_result;
}

auto gse::ide::search::fuzzy_match_path(const std::string_view query_lower, const std::string_view rel, const std::string_view rel_lower, const bool capture_ranges) -> score_result {
	const std::size_t sep = rel_lower.find_last_of("/\\");
	const std::size_t base = sep == std::string_view::npos ? 0 : sep + 1;

	if (base < rel_lower.size()) {
		score_result name = fuzzy_match(query_lower, rel.substr(base), rel_lower.substr(base), capture_ranges);
		if (name.matched) {
			for (match_range& range : name.ranges) {
				range.start += static_cast<std::uint32_t>(base);
			}
			name.score = 0.5f + 0.5f * name.score;
			return name;
		}
	}

	score_result full = fuzzy_match(query_lower, rel, rel_lower, capture_ranges);
	if (full.matched) {
		full.score *= 0.5f;
	}
	return full;
}
