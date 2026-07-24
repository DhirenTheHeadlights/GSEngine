export module gse.ide.analysis:gcc_diagnostics;

import std;
import gse;
import gse.ide.diagnostic;

import :json;

export namespace gse::ide::analysis {
	struct gcc_diagnostics {
		static auto parse_sarif(
			std::string_view sarif
		) -> std::vector<diagnostic>;
		static auto is_module_unavailable(
			std::string_view message
		) -> bool;
		static auto is_module_unavailable(
			std::span<const diagnostic> diagnostics
		) -> bool;
	};
}

namespace gse::ide::analysis {
	auto uri_path(
		std::string_view uri
	) -> std::filesystem::path;
}

auto gse::ide::analysis::uri_path(const std::string_view uri) -> std::filesystem::path {
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

auto gse::ide::analysis::gcc_diagnostics::parse_sarif(std::string_view sarif) -> std::vector<diagnostic> {
	std::vector<diagnostic> out;

	const std::optional<json::value> root = json::parse(sarif);
	if (!root) {
		return out;
	}

	const json::value* runs = root->find("runs");
	if (!runs || !runs->is_array()) {
		return out;
	}

	for (const json::value& run : runs->children) {
		const json::value* results = run.find("results");
		if (!results || !results->is_array()) {
			continue;
		}

		for (const json::value& r : results->children) {
			diagnostic d;
			d.source = diagnostic_source::compiler;

			if (const json::value* level = r.find("level")) {
				gse::enum_from_string(level->as_string(), d.level);
			}
			if (const json::value* message = r.find("message")) {
				if (const json::value* text = message->find("text")) {
					d.message = std::string(text->as_string());
				}
			}

			const json::value* locations = r.find("locations");
			if (!locations || !locations->is_array() || locations->children.empty()) {
				continue;
			}

			const json::value* phys = locations->children[0].find("physicalLocation");
			if (!phys) {
				continue;
			}

			if (const json::value* art = phys->find("artifactLocation")) {
				if (const json::value* uri = art->find("uri")) {
					d.file = uri_path(uri->as_string());
				}
			}

			const json::value* region = phys->find("region");
			if (!region) {
				continue;
			}

			auto field = [&](std::string_view key, std::int64_t fallback) -> std::int64_t {
				const json::value* v = region->find(key);
				return v ? v->as_int() : fallback;
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

auto gse::ide::analysis::gcc_diagnostics::is_module_unavailable(const std::string_view message) -> bool {
	static constexpr std::string_view fragments[] = {
		"must be built",
		"failed to read compiled module",
		"returning to the gate",
		"failed to load binding",
		"failed to load pendings",
		"should have been declared inside",
		"compiled module file is",
	};
	return std::ranges::any_of(fragments, [message](const std::string_view fragment) {
		return message.contains(fragment);
	});
}

auto gse::ide::analysis::gcc_diagnostics::is_module_unavailable(const std::span<const diagnostic> diagnostics) -> bool {
	return std::ranges::any_of(diagnostics, [](const diagnostic& diagnostic) {
		return is_module_unavailable(diagnostic.message);
	});
}
