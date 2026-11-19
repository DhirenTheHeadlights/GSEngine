export module gse.graphics:text_buffer;

import std;

import gse.math;

import :font;

export namespace gse::gui {
	struct buffer_position {
		std::uint32_t line = 0;
		std::uint32_t column = 0;

		friend auto operator<=>(const buffer_position&, const buffer_position&) = default;
	};

	enum class text_face : std::uint8_t {
		inherit,
		text,
		text_strong,
		text_emphasis,
		code,
		code_strong,
		code_emphasis,
	};

	struct text_span {
		std::uint32_t line = 0;
		std::uint32_t start_col = 0;
		std::uint32_t end_col = 0;
		vec4f color{ 1.f, 1.f, 1.f, 1.f };
		text_face face = text_face::inherit;
		float scale = 1.f;
	};

	struct text_stop {
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		float x = 0.f;
	};

	struct text_underline {
		std::uint32_t line = 0;
		std::uint32_t start_col = 0;
		std::uint32_t end_col = 0;
		vec4f color{ 1.f, 0.f, 0.f, 1.f };
	};

	struct text_rule {
		std::uint32_t line = 0;
		float x0 = 0.f;
		float x1 = 0.f;
		float y = 0.5f;
		float thickness = 1.f;
		vec4f color{ 1.f, 1.f, 1.f, 1.f };
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

		[[nodiscard]] auto text() const -> std::string;

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

	struct text_run {
		std::size_t start = 0;
		std::size_t end = 0;
		vec4f color{ 1.f, 1.f, 1.f, 1.f };
		text_face face = text_face::inherit;
		float scale = 1.f;
	};

	struct styled_line {
		std::string_view text;
		std::span<const text_run> runs{};
		vec4f color{ 1.f, 1.f, 1.f, 1.f };
		text_face face = text_face::inherit;
		float scale = 1.f;
	};

	struct wrap_layout {
		const font& face;
		float size = 0.f;
		float width = 0.f;
		std::string_view lead;
		std::string_view indent;
		vec4f lead_color{ 1.f, 1.f, 1.f, 1.f };
		bool wrap = true;
	};

	auto append_wrapped(
		text_buffer& buffer,
		std::vector<text_span>& spans,
		const styled_line& line,
		const wrap_layout& layout
	) -> std::size_t;
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

auto gse::gui::text_buffer::text() const -> std::string {
	std::size_t total = 0;
	for (const std::string& line : lines) {
		total += line.size() + 1;
	}

	std::string joined;
	joined.reserve(total);
	for (std::size_t i = 0; i < lines.size(); ++i) {
		if (i != 0) {
			joined += '\n';
		}
		joined += lines[i];
	}
	return joined;
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

auto gse::gui::append_wrapped(text_buffer& buffer, std::vector<text_span>& spans, const styled_line& line, const wrap_layout& layout) -> std::size_t {
	const std::vector<std::string_view> wrapped = layout.wrap
		? layout.face.wrap(line.text, layout.width, layout.size * line.scale)
		: std::vector<std::string_view>{ line.text };

	for (const auto [row, piece] : std::views::enumerate(wrapped)) {
		std::string_view segment = piece;
		if (layout.wrap) {
			while (!segment.empty() && segment.back() == ' ') {
				segment.remove_suffix(1);
			}
		}

		const auto offset = static_cast<std::size_t>(segment.data() - line.text.data());
		const std::size_t last = offset + segment.size();
		std::string text(row == 0 ? layout.lead : layout.indent);
		const auto column = static_cast<std::uint32_t>(text.size());
		const auto index = static_cast<std::uint32_t>(buffer.lines.size());
		text += segment;

		if (column > 0) {
			spans.push_back({
				.line = index,
				.start_col = 0,
				.end_col = column,
				.color = layout.lead_color,
			});
		}

		const auto plain = [&](const std::size_t from, const std::size_t to) {
			spans.push_back({
				.line = index,
				.start_col = column + static_cast<std::uint32_t>(from - offset),
				.end_col = column + static_cast<std::uint32_t>(to - offset),
				.color = line.color,
				.face = line.face,
				.scale = line.scale,
			});
		};

		std::size_t written = offset;
		for (const text_run& run : line.runs) {
			if (run.end <= written) {
				continue;
			}
			if (run.start >= last) {
				break;
			}
			const std::size_t from = std::max(run.start, written);
			const std::size_t to = std::min(run.end, last);
			if (to <= from) {
				continue;
			}
			if (from > written) {
				plain(written, from);
			}
			spans.push_back({
				.line = index,
				.start_col = column + static_cast<std::uint32_t>(from - offset),
				.end_col = column + static_cast<std::uint32_t>(to - offset),
				.color = run.color,
				.face = run.face,
				.scale = line.scale * run.scale,
			});
			written = to;
		}
		if (written < last) {
			plain(written, last);
		}

		buffer.lines.push_back(std::move(text));
	}

	return wrapped.size();
}