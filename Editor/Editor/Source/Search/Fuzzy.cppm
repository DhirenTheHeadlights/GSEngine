export module gse.ide.search:fuzzy;

import std;

import :types;

export namespace gse::ide::search {
	struct score_result {
		bool matched = false;
		float score = 0.f;
		std::vector<match_range> ranges;
	};

	auto is_word_boundary(std::string_view candidate, std::size_t index) -> bool {
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

	auto fuzzy_match(std::string_view query_lower, std::string_view candidate, std::string_view candidate_lower) -> score_result {
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

			if (!ranges.empty() && ranges.back().start + ranges.back().length == static_cast<std::uint32_t>(ci)) {
				ranges.back().length += 1;
			}
			else {
				ranges.push_back({ .start = static_cast<std::uint32_t>(ci), .length = 1 });
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
}
