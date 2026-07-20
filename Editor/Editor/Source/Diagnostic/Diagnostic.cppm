export module gse.ide.diagnostic;

import std;

export namespace gse::ide {
	enum class diagnostic_source {
		compiler,
		lint
	};

	enum class severity {
		error,
		warning,
		note,
		hint
	};

	struct text_edit {
		std::uint32_t line = 0;
		std::uint32_t end_line = 0;
		std::uint32_t start_col = 0;
		std::uint32_t end_col = 0;
		std::string expected;
		std::string replacement;
	};

	struct fix_it {
		std::string title;
		std::vector<text_edit> edits;
	};

	struct diagnostic {
		std::uint32_t line = 0;
		std::uint32_t end_line = 0;
		std::uint32_t start_col = 0;
		std::uint32_t end_col = 0;
		severity level = severity::error;
		diagnostic_source source = diagnostic_source::compiler;
		std::string rule;
		std::string message;
		std::filesystem::path file;
		std::optional<fix_it> fix;
	};

	struct fix_engine {
		static auto display_to_byte(std::string_view row, std::uint32_t display_col) -> std::uint32_t;
		static auto rule_edits(std::span<const diagnostic> diagnostics, std::string_view rule) -> std::vector<text_edit>;
		static auto apply(std::vector<std::string>& lines, std::span<const text_edit> edits) -> std::size_t;
	};
}

auto gse::ide::fix_engine::display_to_byte(const std::string_view row, const std::uint32_t display_col) -> std::uint32_t {
	constexpr std::size_t gcc_tab_width = 8;
	std::size_t display = 0;
	for (std::size_t i = 0; i < row.size(); ++i) {
		if (display >= display_col) {
			return static_cast<std::uint32_t>(i);
		}
		display += row[i] == '\t' ? gcc_tab_width - display % gcc_tab_width : 1;
	}
	return static_cast<std::uint32_t>(row.size());
}

auto gse::ide::fix_engine::rule_edits(const std::span<const diagnostic> diagnostics, const std::string_view rule) -> std::vector<text_edit> {
	std::vector<text_edit> out;
	for (const diagnostic& d : diagnostics) {
		if (d.rule != rule || !d.fix) {
			continue;
		}
		for (const text_edit& edit : d.fix->edits) {
			out.push_back(edit);
		}
	}
	return out;
}

auto gse::ide::fix_engine::apply(std::vector<std::string>& lines, const std::span<const text_edit> edits) -> std::size_t {
	std::vector<text_edit> ordered(edits.begin(), edits.end());
	std::ranges::sort(ordered, [](const text_edit& a, const text_edit& b) {
		if (a.line != b.line) {
			return a.line > b.line;
		}
		return a.start_col > b.start_col;
	});

	std::size_t applied = 0;
	std::uint32_t guard_line = 0;
	std::uint32_t guard_col = 0;
	bool has_guard = false;
	for (const text_edit& edit : ordered) {
		const std::uint32_t last_line = std::max(edit.line, edit.end_line);
		if (last_line >= lines.size()) {
			continue;
		}
		if (has_guard && edit.line == guard_line && edit.end_col > guard_col) {
			continue;
		}

		std::string& row = lines[edit.line];
		const std::uint32_t byte_start = display_to_byte(row, edit.start_col);
		if (last_line == edit.line) {
			const std::uint32_t byte_end = display_to_byte(row, edit.end_col);
			if (byte_end < byte_start || byte_end > row.size()) {
				continue;
			}
			if (!edit.expected.empty() && row.compare(byte_start, byte_end - byte_start, edit.expected) != 0) {
				continue;
			}
			row.replace(byte_start, byte_end - byte_start, edit.replacement);
		}
		else {
			const std::string& last = lines[last_line];
			const std::uint32_t byte_end = display_to_byte(last, edit.end_col);
			row = row.substr(0, byte_start) + edit.replacement + last.substr(byte_end);
			lines.erase(lines.begin() + edit.line + 1, lines.begin() + last_line + 1);
		}
		guard_line = edit.line;
		guard_col = edit.start_col;
		has_guard = true;
		++applied;
	}
	return applied;
}
