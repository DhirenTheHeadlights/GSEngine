export module gse.graphics:text_buffer;

import std;

import gse.math;

export namespace gse::gui {
	struct buffer_position {
		std::uint32_t line = 0;
		std::uint32_t column = 0;

		friend auto operator<=>(const buffer_position&, const buffer_position&) = default;
	};

	struct text_span {
		std::uint32_t line = 0;
		std::uint32_t start_col = 0;
		std::uint32_t end_col = 0;
		vec4f color{ 1.f, 1.f, 1.f, 1.f };
	};

	struct text_underline {
		std::uint32_t line = 0;
		std::uint32_t start_col = 0;
		std::uint32_t end_col = 0;
		vec4f color{ 1.f, 0.f, 0.f, 1.f };
	};

	struct text_fade {
		std::uint32_t line = 0;
		std::uint32_t start_col = 0;
		std::uint32_t end_col = 0;
		float alpha = 1.f;
	};

	struct text_block {
		std::uint32_t first_line = 0;
		std::uint32_t last_line = 0;
		vec4f fill{ 0.f, 0.f, 0.f, 0.f };
		vec4f border{ 0.f, 0.f, 0.f, 0.f };
		bool align_right = false;
	};

	struct text_buffer {
		std::vector<std::string> lines;

		static auto from_file(
			const std::filesystem::path& path
		) -> std::expected<text_buffer, std::string>;

		auto line_count() const -> std::size_t {
			return lines.size();
		}

		auto line(std::size_t index) const -> std::string_view {
			return index < lines.size() ? std::string_view(lines[index]) : std::string_view{};
		}

		auto insert(
			buffer_position at,
			std::string_view text
		) -> buffer_position;

		auto erase(
			buffer_position from,
			buffer_position to
		) -> void;

		auto clamp(
			buffer_position pos
		) const -> buffer_position;
	};
}

auto gse::gui::text_buffer::from_file(const std::filesystem::path& path) -> std::expected<text_buffer, std::string> {
	text_buffer buf;
	std::ifstream in(path);
	if (!in) {
		return std::unexpected(std::format("could not open '{}'", path.generic_display_string()));
	}

	std::string line;
	while (std::getline(in, line)) {
		buf.lines.push_back(std::move(line));
	}
	if (!in.eof()) {
		return std::unexpected(std::format("could not read '{}'", path.generic_display_string()));
	}
	if (buf.lines.empty()) {
		buf.lines.emplace_back();
	}
	return buf;
}

auto gse::gui::text_buffer::insert(buffer_position at, std::string_view text) -> buffer_position {
	if (lines.empty()) {
		lines.emplace_back();
	}
	at = clamp(at);

	std::size_t prev = 0;
	for (std::size_t i = 0; i <= text.size(); ++i) {
		const bool newline = i < text.size() && text[i] == '\n';
		const bool end = i == text.size();
		if (!newline && !end) {
			continue;
		}

		const std::string_view chunk = text.substr(prev, i - prev);
		std::string& cur = lines[at.line];
		cur.insert(at.column, chunk);
		at.column += static_cast<std::uint32_t>(chunk.size());

		if (newline) {
			std::string tail = cur.substr(at.column);
			cur.resize(at.column);
			lines.insert(lines.begin() + at.line + 1, std::move(tail));
			++at.line;
			at.column = 0;
			prev = i + 1;
		}
	}

	return at;
}

auto gse::gui::text_buffer::erase(buffer_position from, buffer_position to) -> void {
	from = clamp(from);
	to = clamp(to);
	if (from == to) {
		return;
	}
	if (to < from) {
		std::swap(from, to);
	}

	if (from.line == to.line) {
		lines[from.line].erase(from.column, to.column - from.column);
		return;
	}

	std::string& first = lines[from.line];
	const std::string& last = lines[to.line];
	first.resize(from.column);
	first.append(last.substr(to.column));
	lines.erase(lines.begin() + from.line + 1, lines.begin() + to.line + 1);
}

auto gse::gui::text_buffer::clamp(buffer_position pos) const -> buffer_position {
	if (lines.empty()) {
		return { 0, 0 };
	}
	const std::uint32_t line_cap = static_cast<std::uint32_t>(lines.size() - 1);
	if (pos.line > line_cap) {
		pos.line = line_cap;
	}
	const std::uint32_t col_cap = static_cast<std::uint32_t>(lines[pos.line].size());
	if (pos.column > col_cap) {
		pos.column = col_cap;
	}
	return pos;
}