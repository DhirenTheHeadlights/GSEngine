export module gse.ide.search:fuzzy;

import std;

import :types;

namespace gse::ide::search {
	struct score_result {
		bool matched = false;
		float score = 0.f;
		std::vector<match_range> ranges;
	};

	auto fuzzy_match(
		std::string_view query_lower,
		std::string_view candidate,
		std::string_view candidate_lower,
		bool capture_ranges = true
	) -> score_result;

	auto fuzzy_match_path(
		std::string_view query_lower,
		std::string_view rel,
		std::string_view rel_lower,
		bool capture_ranges = true
	) -> score_result;
}

namespace gse::ide::search {
	auto is_word_boundary(
		std::string_view candidate,
		std::size_t index
	) -> bool;
}

auto gse::ide::search::is_word_boundary(const std::string_view candidate, const std::size_t index) -> bool {
	if (index == 0) {
		return true;
	}
	const char prev = candidate[index - 1];
	const char cur = candidate[index];
	if (prev == '_' || prev == '-' || prev == '/' || prev == '\\' || prev == '.' || prev == ' ') {
		return true;
	}
	const bool prev_lower = prev >= 'a' && prev <= 'z';
	const bool cur_upper = cur >= 'A' && cur <= 'Z';
	if (prev_lower && cur_upper) {
		return true;
	}
	const bool prev_digit = prev >= '0' && prev <= '9';
	const bool cur_alpha = (cur >= 'a' && cur <= 'z') || (cur >= 'A' && cur <= 'Z');
	if (prev_digit && cur_alpha) {
		return true;
	}
	return false;
}

auto gse::ide::search::fuzzy_match(const std::string_view query_lower, const std::string_view candidate, const std::string_view candidate_lower, const bool capture_ranges) -> score_result {
	score_result out;
	if (query_lower.empty()) {
		out.matched = true;
		return out;
	}
	if (candidate_lower.size() < query_lower.size()) {
		return out;
	}

	const std::size_t none = candidate_lower.size();
	std::size_t qi = 0;
	std::size_t first_index = none;
	std::size_t prev_match = none;
	float score = 0.f;
	std::vector<match_range> ranges;

	for (std::size_t ci = 0; ci < candidate_lower.size() && qi < query_lower.size(); ++ci) {
		if (candidate_lower[ci] != query_lower[qi]) {
			continue;
		}
		if (first_index == none) {
			first_index = ci;
		}
		float bonus = 1.f;
		if (prev_match != none && ci == prev_match + 1) {
			bonus += 3.f;
		}
		if (is_word_boundary(candidate, ci)) {
			bonus += 4.f;
		}
		if (prev_match != none && ci > prev_match + 1) {
			const float gap = static_cast<float>(ci - prev_match - 1);
			bonus -= std::min(gap * 0.5f, 3.f);
		}
		score += std::max(bonus, 0.1f);

		if (capture_ranges) {
			if (!ranges.empty() && ranges.back().start + ranges.back().length == static_cast<std::uint32_t>(ci)) {
				ranges.back().length += 1;
			}
			else {
				ranges.push_back({
					.start = static_cast<std::uint32_t>(ci),
					.length = 1,
				});
			}
		}

		prev_match = ci;
		++qi;
	}

	if (qi < query_lower.size()) {
		return out;
	}

	score += static_cast<float>(candidate_lower.size() - first_index) * 0.01f;

	out.matched = true;
	out.score = std::min(score / (static_cast<float>(query_lower.size()) * 8.f), 1.f);
	out.ranges = std::move(ranges);
	return out;
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
