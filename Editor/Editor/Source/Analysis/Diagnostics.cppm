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
	};
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
					d.file = std::string(uri->as_string());
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
