export module gse.meta:text;

import std;

export namespace gse {
	auto next_line(
		std::string_view text,
		std::size_t& position
	) -> std::optional<std::string_view>;

	auto split_fields(
		std::string_view line,
		char delimiter,
		std::span<std::string_view> out
	) -> std::size_t;
}

auto gse::next_line(const std::string_view text, std::size_t& position) -> std::optional<std::string_view> {
	if (position >= text.size()) {
		return std::nullopt;
	}
	const std::size_t eol = text.find('\n', position);
	std::string_view line = text.substr(position, (eol == std::string_view::npos ? text.size() : eol) - position);
	position = eol == std::string_view::npos ? text.size() : eol + 1;
	if (!line.empty() && line.back() == '\r') {
		line.remove_suffix(1);
	}
	return line;
}

auto gse::split_fields(const std::string_view line, const char delimiter, const std::span<std::string_view> out) -> std::size_t {
	std::size_t count = 0;
	std::size_t start = 0;
	for (std::size_t i = 0; i <= line.size() && count < out.size(); ++i) {
		if (i == line.size() || line[i] == delimiter) {
			out[count++] = line.substr(start, i - start);
			start = i + 1;
		}
	}
	return count;
}
