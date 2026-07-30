export module gse.ide.analysis:symbol_index_builder;

import std;
import gse;

import :process;
import :compilation_database;
import :symbol_extract;
import :gcc_diagnostics;

export namespace gse::ide::analysis {
	enum class symbol_index_failure {
		none,
		launch,
		timeout,
		compiler,
		module_unavailable,
		incomplete,
	};

	struct tu_symbols {
		std::filesystem::path tu;
		symbol_set set;
		std::vector<std::filesystem::path> dependencies;
		symbol_index_failure failure = symbol_index_failure::none;
		std::string failure_detail;

		auto retryable() const -> bool;
	};

	auto describe(symbol_index_failure failure) -> std::string_view;

	struct symbol_index_builder {
		static auto run_one(
			const compilation_entry& entry,
			const std::filesystem::path& plugin_dll,
			std::span<const std::filesystem::path> workspace_roots,
			bool module_graph_validated = false,
			std::stop_token cancel = {}
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
	return failure == symbol_index_failure::launch
		|| failure == symbol_index_failure::timeout
		|| failure == symbol_index_failure::compiler
		|| failure == symbol_index_failure::incomplete;
}

auto gse::ide::analysis::describe(const symbol_index_failure failure) -> std::string_view {
	switch (failure) {
	case symbol_index_failure::none:
		return "none";
	case symbol_index_failure::launch:
		return "compiler launch failed";
	case symbol_index_failure::timeout:
		return "compiler timed out";
	case symbol_index_failure::compiler:
		return "compiler exited with an error";
	case symbol_index_failure::module_unavailable:
		return "compiled module artifacts are unavailable";
	case symbol_index_failure::incomplete:
		return "compiler plugin output was incomplete";
	}
	return "unknown compiler failure";
}

auto gse::ide::analysis::symbol_index_builder::run_one(const compilation_entry& entry, const std::filesystem::path& plugin_dll, const std::span<const std::filesystem::path> workspace_roots, const bool module_graph_validated, std::stop_token cancel) -> tu_symbols {
	tu_symbols out;
	out.tu = entry.file;
	if (plugin_dll.empty()) {
		return out;
	}
	if (!module_graph_validated) {
		const std::expected<void, std::string> module_graph = validate_module_graph(entry);
		if (!module_graph) {
			out.failure = symbol_index_failure::module_unavailable;
			out.failure_detail = module_graph.error();
			return out;
		}
	}

	const std::filesystem::path token_temp = process::temporary_path("symbols", "txt");
	const std::filesystem::path sarif_temp = process::temporary_path("symbols", "sarif");
	const std::filesystem::path dependency_temp = process::temporary_path("symbols", "d");
	const auto remove_temporary_files = gse::make_scope_exit([&] {
		std::error_code ec;
		std::filesystem::remove(token_temp, ec);
		std::filesystem::remove(sarif_temp, ec);
		std::filesystem::remove(dependency_temp, ec);
	});

	std::string command_line = entry.command.command_line;
	command_line += " -fplugin=\"" + plugin_dll.generic_native_encoded_string() + "\"";
	command_line += " -fplugin-arg-gse_tokens-out=\"" + token_temp.generic_native_encoded_string() + "\"";
	for (const std::filesystem::path& root : workspace_roots) {
		command_line += " -fplugin-arg-gse_tokens-root=\"" + root.generic_native_encoded_string() + "\"";
	}
	command_line += " -MMD -MF \"" + dependency_temp.generic_native_encoded_string() + "\" -MT gseditor_index";

	const process::run_outcome run = process::run_capture_stderr(command_line, entry.command.directory, sarif_temp, std::move(cancel));

	std::ifstream in(token_temp, std::ios::binary);
	if (in) {
		std::ostringstream stream;
		stream << in.rdbuf();
		out.set = symbol_tokens::parse(stream.str(), out.tu.generic_native_encoded_string());
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

	if (!run) {
		out.failure = run.error() == process::run_error::timed_out ? symbol_index_failure::timeout : symbol_index_failure::launch;
	}
	else if (*run != 0) {
		out.failure = symbol_index_failure::compiler;
	}
	else if (!out.set.complete) {
		out.failure = symbol_index_failure::incomplete;
	}
	if (out.failure != symbol_index_failure::none) {
		std::ifstream sarif_in(sarif_temp, std::ios::binary);
		std::ostringstream sarif_stream;
		if (sarif_in) {
			sarif_stream << sarif_in.rdbuf();
		}
		const std::vector<diagnostic> diagnostics = gcc_diagnostics::parse_sarif(sarif_stream.str());
		if (!diagnostics.empty()) {
			if (gcc_diagnostics::is_module_unavailable(diagnostics)) {
				out.failure = symbol_index_failure::module_unavailable;
			}
			const diagnostic& first = diagnostics.front();
			out.failure_detail = std::format(
				"{}:{}:{}: {}",
				first.file.empty() ? entry.file.generic_display_string() : first.file.generic_display_string(),
				first.line + 1,
				first.start_col + 1,
				first.message
			);
		}
		else if (out.failure == symbol_index_failure::compiler) {
			out.failure_detail = std::format("compiler exit code {}", run.value_or(-1));
		}
		else {
			out.failure_detail = std::string(describe(out.failure));
		}
	}
	return out;
}
