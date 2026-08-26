export module gse.math:vector_parse;

import std;

import gse.meta;

import :vector;

export template <gse::internal::is_vec_element T, std::size_t N>
requires std::is_arithmetic_v<T>
struct gse::parser<gse::vec<T, N>> {
	static auto parse(
		std::string_view raw,
		vec<T, N>& out
	) -> bool;
};

namespace gse::internal {
	auto strip_vector_padding(
		std::string_view text
	) -> std::string_view;
}

auto gse::internal::strip_vector_padding(std::string_view text) -> std::string_view {
	while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '(')) {
		text.remove_prefix(1);
	}
	while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == ')')) {
		text.remove_suffix(1);
	}
	return text;
}

template <gse::internal::is_vec_element T, std::size_t N>
requires std::is_arithmetic_v<T>
auto gse::parser<gse::vec<T, N>>::parse(const std::string_view raw, vec<T, N>& out) -> bool {
	std::size_t index = 0;

	for (const auto& part : std::views::split(internal::strip_vector_padding(raw), ',')) {
		if (index == N) {
			return false;
		}

		T element{};
		if (!gse::parse(internal::strip_vector_padding(std::string_view(part)), element)) {
			return false;
		}

		out.at(index) = element;
		++index;
	}

	return index == N;
}