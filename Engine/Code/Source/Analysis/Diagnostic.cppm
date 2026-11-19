export module gse.analysis:diagnostic;

import std;

import gse.json;
import gse.meta;

export namespace gse::analysis {
	enum class severity {
		error,
		warning,
		note,
		hint
	};

	struct diagnostic {
		std::uint32_t line = 0;
		std::uint32_t end_line = 0;
		std::uint32_t start_col = 0;
		std::uint32_t end_col = 0;
		severity level = severity::error;
		std::string message;
		std::filesystem::path file;
	};

	struct byte_range {
		std::uint32_t line = 0;
		std::uint32_t start = 0;
		std::uint32_t end = 0;
	};

	auto parse_sarif(
		std::string_view sarif
	) -> std::vector<diagnostic>;

	auto has_error(
		std::span<const diagnostic> diagnostics
	) -> bool;

	auto summarize(
		std::span<const diagnostic> diagnostics,
		std::size_t limit
	) -> std::string;

	auto display_to_byte(
		std::string_view row,
		std::uint32_t display_col
	) -> std::uint32_t;

	template <std::invocable<const byte_range&> F>
	auto for_each_byte_range(
		const diagnostic& d,
		std::span<const std::string> lines,
		F&& emit
	) -> void;
}

namespace gse::analysis {
	auto uri_path(
		std::string_view uri
	) -> std::filesystem::path;
}

auto gse::analysis::uri_path(const std::string_view uri) -> std::filesystem::path {
	std::string decoded;
	decoded.reserve(uri.size());
	for (std::size_t i = 0; i < uri.size(); ++i) {
		if (uri[i] == '%' && i + 2 < uri.size()) {
			auto hex_value = [](const char ch) -> int {
				if (ch >= '0' && ch <= '9') {
					return ch - '0';
				}
				if (ch >= 'a' && ch <= 'f') {
					return ch - 'a' + 10;
				}
				if (ch >= 'A' && ch <= 'F') {
					return ch - 'A' + 10;
				}
				return -1;
			};
			const int high = hex_value(uri[i + 1]);
			const int low = hex_value(uri[i + 2]);
			if (high >= 0 && low >= 0) {
				decoded.push_back(static_cast<char>(high * 16 + low));
				i += 2;
				continue;
			}
		}
		decoded.push_back(uri[i]);
	}

	constexpr std::string_view file_scheme = "file://";
	if (!decoded.starts_with(file_scheme)) {
		return decoded;
	}

	const bool local = decoded.size() > file_scheme.size() && decoded[file_scheme.size()] == '/';
	decoded.erase(0, file_scheme.size());
	if (!local) {
		decoded.insert(0, "//");
	}
	else if (decoded.size() >= 3 && decoded[0] == '/' && std::isalpha(static_cast<unsigned char>(decoded[1])) && decoded[2] == ':') {
		decoded.erase(0, 1);
	}
	return decoded;
}

auto gse::analysis::parse_sarif(const std::string_view sarif) -> std::vector<diagnostic> {
	std::vector<diagnostic> out;

	const std::string_view document = sarif.substr(std::min(sarif.find('{'), sarif.size()));
	std::expected<json::value, json::parse_error> root = json::parse(document);
	if (!root && root.error().code == json::error::trailing_content) {
		root = json::parse(document.substr(0, root.error().offset));
	}
	if (!root) {
		return out;
	}

	const json::value* runs = root->find("runs");
	if (!runs || !runs->is_array()) {
		return out;
	}

	for (const json::value& run : runs->elements()) {
		const json::value* results = run.find("results");
		if (!results || !results->is_array()) {
			continue;
		}

		for (const json::value& r : results->elements()) {
			diagnostic d;

			if (const json::value* level = r.find("level")) {
				enum_from_string(level->text(), d.level);
			}
			if (const json::value* message = r.find("message")) {
				if (const json::value* text = message->find("text")) {
					d.message = std::string(text->text());
				}
			}

			const json::value* locations = r.find("locations");
			if (!locations || !locations->is_array() || locations->empty()) {
				continue;
			}

			const json::value* phys = locations->elements().front().find("physicalLocation");
			if (!phys) {
				continue;
			}

			if (const json::value* art = phys->find("artifactLocation")) {
				if (const json::value* uri = art->find("uri")) {
					d.file = uri_path(uri->text());
				}
			}

			const json::value* region = phys->find("region");
			if (!region) {
				continue;
			}

			auto field = [&](const std::string_view key, const std::int64_t fallback) -> std::int64_t {
				const json::value* v = region->find(key);
				return v ? v->integer(fallback) : fallback;
			};

			const std::int64_t start_line = field("startLine", 1);
			const std::int64_t start_col = field("startColumn", 1);
			const std::int64_t end_col = field("endColumn", 0);

			d.line = static_cast<std::uint32_t>(start_line > 0 ? start_line - 1 : 0);
			d.start_col = static_cast<std::uint32_t>(start_col > 0 ? start_col - 1 : 0);
			d.end_col = end_col > start_col ? static_cast<std::uint32_t>(end_col - 1) : d.start_col + 1;

			out.push_back(std::move(d));
		}
	}

	return out;
}

auto gse::analysis::has_error(const std::span<const diagnostic> diagnostics) -> bool {
	return std::ranges::any_of(diagnostics, [](const diagnostic& d) {
		return d.level == severity::error;
	});
}

auto gse::analysis::summarize(const std::span<const diagnostic> diagnostics, const std::size_t limit) -> std::string {
	std::string out;
	std::size_t written = 0;
	for (const diagnostic& d : diagnostics) {
		if (written == limit) {
			out += std::format("\n... and more diagnostics beyond the first {}", limit);
			break;
		}
		if (written != 0) {
			out += '\n';
		}
		out += d.file.empty()
			? d.message
			: std::format("{}:{}:{}: {}", d.file.filename().generic_display_string(), d.line + 1, d.start_col + 1, d.message);
		++written;
	}
	return out;
}

auto gse::analysis::display_to_byte(const std::string_view row, const std::uint32_t display_col) -> std::uint32_t {
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

template <std::invocable<const gse::analysis::byte_range&> F>
auto gse::analysis::for_each_byte_range(const diagnostic& d, const std::span<const std::string> lines, F&& emit) -> void {
	if (d.line >= lines.size()) {
		return;
	}
	const std::uint32_t last_line = std::min<std::uint32_t>(std::max(d.end_line, d.line), static_cast<std::uint32_t>(lines.size()) - 1);
	for (std::uint32_t line = d.line; line <= last_line; ++line) {
		const std::string_view row = lines[line];
		const std::uint32_t start = line == d.line ? display_to_byte(row, d.start_col) : 0;
		std::uint32_t end = line == last_line ? display_to_byte(row, d.end_col) : static_cast<std::uint32_t>(row.size());
		if (line == d.line && line == last_line) {
			end = std::max(start + 1, end);
		}
		if (end <= start) {
			continue;
		}
		emit(byte_range{
			.line = line,
			.start = start,
			.end = end,
		});
	}
}
