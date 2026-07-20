export module gse.ide.analysis:symbol_index_builder;

import std;
import gse;

import :process;
import :compilation_database;
import :symbol_extract;

export namespace gse::ide::analysis {
	enum class symbol_index_failure {
		none,
		launch,
		timeout,
		compiler,
		incomplete,
	};

	struct tu_symbols {
		std::filesystem::path tu;
		symbol_set set;
		std::vector<std::filesystem::path> dependencies;
		symbol_index_failure failure = symbol_index_failure::none;

		auto retryable() const -> bool;
	};

	struct symbol_index_builder {
		static auto run_one(
			const compilation_entry& entry,
			const std::filesystem::path& plugin_dll,
			const std::filesystem::path& workspace_root,
			std::uint32_t slot
		) -> tu_symbols;
	};
}

namespace gse::ide::analysis {
	auto dependency_paths(std::string_view text, const std::filesystem::path& directory, const std::filesystem::path& source) -> std::vector<std::filesystem::path> {
		std::string flattened;
		flattened.reserve(text.size());
		for (std::size_t i = 0; i < text.size(); ++i) {
			if (text[i] == '\\' && i + 1 < text.size() && text[i + 1] == '\n') {
				++i;
				continue;
			}
			if (text[i] == '\\' && i + 2 < text.size() && text[i + 1] == '\r' && text[i + 2] == '\n') {
				i += 2;
				continue;
			}
			flattened.push_back(text[i]);
		}

		std::vector<std::filesystem::path> paths;
		std::unordered_set<gse::id> seen;
		auto append = [&](std::filesystem::path path) {
			if (path.is_relative()) {
				path = directory / path;
			}
			std::error_code ec;
			const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
			if (!ec) {
				path = canonical;
			}
			if (seen.insert(gse::generate_temp_id(path)).second) {
				paths.push_back(std::move(path));
			}
		};
		append(source);

		const std::size_t colon = flattened.find(':');
		if (colon == std::string::npos) {
			return paths;
		}
		std::string token;
		for (std::size_t i = colon + 1; i <= flattened.size(); ++i) {
			const char c = i < flattened.size() ? flattened[i] : ' ';
			if (c == '\\' && i + 1 < flattened.size() && (flattened[i + 1] == ' ' || flattened[i + 1] == '\t' || flattened[i + 1] == '\\' || flattened[i + 1] == '#')) {
				token.push_back(flattened[++i]);
			}
			else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
				if (!token.empty()) {
					append(std::filesystem::path(std::move(token)));
					token.clear();
				}
			}
			else {
				token.push_back(c);
			}
		}
		return paths;
	}
}

auto gse::ide::analysis::tu_symbols::retryable() const -> bool {
	return failure == symbol_index_failure::launch || failure == symbol_index_failure::timeout || failure == symbol_index_failure::incomplete;
}

auto gse::ide::analysis::symbol_index_builder::run_one(const compilation_entry& entry, const std::filesystem::path& plugin_dll, const std::filesystem::path& workspace_root, const std::uint32_t slot) -> tu_symbols {
	tu_symbols out;
	out.tu = entry.file;
	if (plugin_dll.empty()) {
		return out;
	}

	const std::filesystem::path token_temp = std::filesystem::temp_directory_path() / ("gseditor_sym_" + std::to_string(slot) + ".txt");
	const std::filesystem::path sarif_temp = std::filesystem::temp_directory_path() / ("gseditor_sym_" + std::to_string(slot) + ".sarif");
	const std::filesystem::path dependency_temp = std::filesystem::temp_directory_path() / ("gseditor_sym_" + std::to_string(slot) + ".d");

	std::string command_line = entry.command.command_line;
	command_line += " -fplugin=\"" + plugin_dll.generic_native_encoded_string() + "\"";
	command_line += " -fplugin-arg-gse_tokens-out=\"" + token_temp.generic_native_encoded_string() + "\"";
	command_line += " -fplugin-arg-gse_tokens-root=\"" + workspace_root.generic_native_encoded_string() + "\"";
	command_line += " -fplugin-arg-gse_tokens-index";
	command_line += " -MMD -MF \"" + dependency_temp.generic_native_encoded_string() + "\" -MT gseditor_index";

	std::error_code ec;
	std::filesystem::remove(token_temp, ec);
	std::filesystem::remove(sarif_temp, ec);
	std::filesystem::remove(dependency_temp, ec);

	const std::string directory = entry.command.directory.native_encoded_string();
	const std::string sarif_path = sarif_temp.native_encoded_string();
	const process::run_result run = process::run_capture_stderr(command_line.c_str(), directory.c_str(), sarif_path.c_str());

	std::filesystem::remove(sarif_temp, ec);

	std::ifstream in(token_temp, std::ios::binary);
	if (in) {
		std::ostringstream stream;
		stream << in.rdbuf();
		out.set = symbol_tokens::parse(stream.str());
	}
	std::ifstream dependency_in(dependency_temp, std::ios::binary);
	if (dependency_in) {
		std::ostringstream stream;
		stream << dependency_in.rdbuf();
		out.dependencies = dependency_paths(stream.str(), entry.command.directory, entry.file);
	}
	if (out.dependencies.empty()) {
		out.dependencies.push_back(entry.file);
	}

	if (!run.launched) {
		out.failure = symbol_index_failure::launch;
	}
	else if (run.timed_out) {
		out.failure = symbol_index_failure::timeout;
	}
	else if (run.exit_code != 0) {
		out.failure = symbol_index_failure::compiler;
	}
	else if (!out.set.complete) {
		out.failure = symbol_index_failure::incomplete;
	}
	std::filesystem::remove(token_temp, ec);
	std::filesystem::remove(dependency_temp, ec);
	return out;
}
