export module gse.ide.analysis:gcc_diagnostics;

import std;
import gse;

import :json;

export namespace gse::ide::analysis {
	enum class severity {
		error,
		warning,
		note
	};

	struct gcc_diagnostic {
		std::uint32_t line = 0;
		std::uint32_t start_col = 0;
		std::uint32_t end_col = 0;
		severity level = severity::error;
		std::string message;
		std::string file;
	};

	struct check_command {
		std::string command_line;
		std::filesystem::path directory;
	};

	struct gcc_diagnostics {
		static auto parse_sarif(
			std::string_view sarif
		) -> std::vector<gcc_diagnostic>;

		static auto build_check_command(
			std::string_view compile_commands,
			const std::filesystem::path& file
		) -> std::optional<check_command>;
	};
}

auto gse::ide::analysis::gcc_diagnostics::parse_sarif(std::string_view sarif) -> std::vector<gcc_diagnostic> {
	std::vector<gcc_diagnostic> out;

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
			gcc_diagnostic d;

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

auto gse::ide::analysis::gcc_diagnostics::build_check_command(std::string_view compile_commands, const std::filesystem::path& file) -> std::optional<check_command> {
	const std::optional<json::value> root = json::parse(compile_commands);
	if (!root || !root->is_array()) {
		return std::nullopt;
	}

	auto normalize = [](std::string s) -> std::string {
		for (char& c : s) {
			c = c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		return s;
	};
	const std::string target = normalize(file.native_encoded_string());

	for (const json::value& entry : root->children) {
		const json::value* file_field = entry.find("file");
		const json::value* command_field = entry.find("command");
		if (!file_field || !command_field) {
			continue;
		}
		if (normalize(std::string(file_field->as_string())) != target) {
			continue;
		}

		std::vector<std::string> tokens;
		{
			std::string current;
			bool in_quote = false;
			for (const char c : command_field->as_string()) {
				if (c == '"') {
					in_quote = !in_quote;
					current.push_back(c);
				}
				else if (c == ' ' && !in_quote) {
					if (!current.empty()) {
						tokens.push_back(std::move(current));
						current.clear();
					}
				}
				else {
					current.push_back(c);
				}
			}
			if (!current.empty()) {
				tokens.push_back(std::move(current));
			}
		}

		std::vector<std::string> kept;
		for (std::size_t k = 0; k < tokens.size(); ++k) {
			const std::string& t = tokens[k];
			if (t == "-c" || t == "-MD" || t == "-fdeps-format=p1689r5") {
				continue;
			}
			if (t == "-o" || t == "-MF" || t == "-MT") {
				++k;
				continue;
			}
			kept.push_back(t);
		}
		kept.emplace_back("-fsyntax-only");
		kept.emplace_back("-fdiagnostics-format=sarif-stderr");

		std::string command_line;
		for (std::size_t k = 0; k < kept.size(); ++k) {
			if (k > 0) {
				command_line.push_back(' ');
			}
			command_line += kept[k];
		}

		check_command result;
		result.command_line = std::move(command_line);
		if (const json::value* directory = entry.find("directory")) {
			result.directory = std::filesystem::path(std::string(directory->as_string()));
		}
		return result;
	}

	return std::nullopt;
}
