export module gse.ide.analysis:diagnostics_runner;

import std;
import gse;
import gse.ide.diagnostic;

import :process;
import :compilation_database;
import :gcc_diagnostics;
import :semantic_tokens;
import :symbol_extract;

export namespace gse::ide::analysis {
	enum class diagnostics_status {
		success,
		module_unavailable,
		database_unavailable,
		entry_unavailable,
		launch_failed,
		timed_out,
		compiler_failed
	};

	constexpr auto analysis_failed(diagnostics_status status) -> bool;
	constexpr auto analysis_crashed(diagnostics_status status) -> bool;

	struct diagnostics_check {
		std::atomic<bool> done = false;
		std::uint32_t document_id = 0;
		document_revision revision;
		std::vector<diagnostic> result;
		std::vector<diagnostic> lint;
		std::vector<qualified_use> quals;
		std::vector<qualified_use> template_args;
		std::vector<semantic_token> tokens;
		std::vector<symbol_token> symbols;
		std::vector<symbol_ref> refs;
		std::vector<param_token> params;
		bool symbols_complete = false;
		diagnostics_status status = diagnostics_status::success;
		std::string failure_output;
		gse::time duration;
	};

	struct diagnostics_runner {
		static auto start(
			const std::shared_ptr<diagnostics_check>& check,
			const std::filesystem::path& compile_commands,
			const std::filesystem::path& file,
			const std::filesystem::path& plugin_dll,
			const std::filesystem::path& workspace_root,
			void (*lint_hook)(diagnostics_check&)
		) -> void;
	};
}

namespace gse::ide::analysis {
	auto normalize_diagnostic_files(
		std::span<diagnostic> diagnostics,
		const std::filesystem::path& directory
	) -> void;
}

constexpr auto gse::ide::analysis::analysis_failed(const diagnostics_status status) -> bool {
	return status == diagnostics_status::module_unavailable;
}

constexpr auto gse::ide::analysis::analysis_crashed(const diagnostics_status status) -> bool {
	return status != diagnostics_status::success && !analysis_failed(status);
}

auto gse::ide::analysis::normalize_diagnostic_files(const std::span<diagnostic> diagnostics, const std::filesystem::path& directory) -> void {
	for (diagnostic& diagnostic : diagnostics) {
		if (diagnostic.file.empty()) {
			continue;
		}
		if (diagnostic.file.is_relative()) {
			diagnostic.file = directory / diagnostic.file;
		}
		std::error_code ec;
		const std::filesystem::path canonical = std::filesystem::weakly_canonical(diagnostic.file, ec);
		diagnostic.file = ec ? diagnostic.file.lexically_normal() : canonical;
	}
}

auto gse::ide::analysis::diagnostics_runner::start(const std::shared_ptr<diagnostics_check>& check, const std::filesystem::path& compile_commands, const std::filesystem::path& file, const std::filesystem::path& plugin_dll, const std::filesystem::path& workspace_root, void (*lint_hook)(diagnostics_check&)) -> void {
	std::thread([check, compile_commands, file, plugin_dll, workspace_root, lint_hook] {
		const gse::time started = gse::system_clock::now<gse::time>();
		auto read_file = [](const std::filesystem::path& path) -> std::string {
			std::ifstream in(path, std::ios::binary);
			if (!in) {
				return {};
			}
			std::ostringstream stream;
			stream << in.rdbuf();
			return stream.str();
		};

		const std::shared_ptr<const compilation_database> database = load_compilation_database(compile_commands);
		const compilation_entry* entry = database ? database->find(file) : nullptr;
		if (!database) {
			check->status = diagnostics_status::database_unavailable;
			check->failure_output = std::format("compilation database unavailable: {}", compile_commands.display_string());
			gse::log::println(gse::log::level::error, gse::log::category::task, "analysis: {}", check->failure_output);
		}
		else if (!entry) {
			check->status = diagnostics_status::entry_unavailable;
			check->failure_output = std::format("no compilation database entry for {}", file.display_string());
			gse::log::println(gse::log::level::error, gse::log::category::task, "analysis: {}", check->failure_output);
		}
		else if (const std::expected<void, std::string> module_graph = validate_module_graph(*entry); !module_graph) {
			check->status = diagnostics_status::module_unavailable;
			check->failure_output = module_graph.error();
		}
		else {
			const std::filesystem::path sarif_temp = process::temporary_path("diagnostics", "sarif");
			const auto remove_sarif = gse::make_scope_exit([&sarif_temp] {
				std::error_code ec;
				std::filesystem::remove(sarif_temp, ec);
			});

			std::string command_line = entry->command.command_line;
			std::filesystem::path token_temp;
			const auto remove_tokens = gse::make_scope_exit([&token_temp] {
				if (!token_temp.empty()) {
					std::error_code ec;
					std::filesystem::remove(token_temp, ec);
				}
			});
			if (!plugin_dll.empty()) {
				token_temp = process::temporary_path("tokens", "txt");
				command_line += " -fplugin=\"" + plugin_dll.generic_native_encoded_string() + "\"";
				command_line += " -fplugin-arg-gse_tokens-out=\"" + token_temp.generic_native_encoded_string() + "\"";
				command_line += " -fplugin-arg-gse_tokens-root=\"" + workspace_root.generic_native_encoded_string() + "\"";
			}

			const std::string directory = entry->command.directory.native_encoded_string();
			const std::string sarif_path = sarif_temp.native_encoded_string();
			const process::run_result run = process::run_capture_stderr(command_line.c_str(), directory.c_str(), sarif_path.c_str());

			const std::string sarif = read_file(sarif_temp);
			check->result = gcc_diagnostics::parse_sarif(sarif);

			if (gcc_diagnostics::is_module_unavailable(check->result)) {
				check->status = diagnostics_status::module_unavailable;
			}

			if (check->status == diagnostics_status::success && check->result.empty()) {
				if (!run.launched) {
					check->status = diagnostics_status::launch_failed;
				}
				else if (run.timed_out) {
					check->status = diagnostics_status::timed_out;
				}
				else if (run.exit_code != 0) {
					check->status = diagnostics_status::compiler_failed;
				}
			}
			if (analysis_crashed(check->status)) {
				check->failure_output = sarif;
				std::string reason = sarif.substr(0, std::min<std::size_t>(sarif.size(), 4000));
				if (reason.empty()) {
					reason = "(no compiler output captured before exit — likely a plugin segfault)";
				}
				gse::log::println(gse::log::level::error, gse::log::category::task, "analyzer exited abnormally (code {}) on {}:\n{}", run.exit_code, file.filename().display_string(), reason);
			}

			if (!token_temp.empty()) {
				const std::string token_text = read_file(token_temp);
				check->tokens = semantic_tokens::parse(token_text);

				symbol_set symbols = symbol_tokens::parse(token_text, file.generic_native_encoded_string());
				check->symbols = std::move(symbols.symbols);
				check->refs = std::move(symbols.refs);
				check->params = std::move(symbols.params);
				check->quals = std::move(symbols.quals);
				check->template_args = std::move(symbols.template_args);
				check->symbols_complete = symbols.complete;
			}
		}

		if (lint_hook) {
			lint_hook(*check);
		}
		if (entry) {
			normalize_diagnostic_files(check->result, entry->command.directory);
			normalize_diagnostic_files(check->lint, entry->command.directory);
		}

		check->duration = gse::system_clock::now<gse::time>() - started;
		check->done.store(true, std::memory_order_release);
	}).detach();
}
