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
	struct diagnostics_check {
		std::atomic<bool> done = false;
		std::uint32_t document_id = 0;
		std::vector<diagnostic> result;
		std::vector<diagnostic> lint;
		std::vector<qualified_use> quals;
		std::vector<qualified_use> template_args;
		std::vector<semantic_token> tokens;
		std::vector<symbol_token> symbols;
		std::vector<symbol_ref> refs;
		bool symbols_complete = false;
		bool failed = false;
		bool crashed = false;
		std::string crash_output;
		gse::time duration;
	};

	struct diagnostics_runner {
		static auto find_compile_commands(
			const std::filesystem::path& root
		) -> std::optional<std::filesystem::path>;

		static auto start(
			const std::shared_ptr<diagnostics_check>& check,
			const std::filesystem::path& compile_commands,
			const std::filesystem::path& file,
			const std::filesystem::path& plugin_dll,
			void (*lint_hook)(diagnostics_check&)
		) -> void;
	};
}

auto gse::ide::analysis::diagnostics_runner::find_compile_commands(const std::filesystem::path& root) -> std::optional<std::filesystem::path> {
	const std::filesystem::path build = root / "out" / "build";
	std::error_code ec;
	if (!std::filesystem::is_directory(build, ec)) {
		return std::nullopt;
	}

	std::optional<std::filesystem::path> best;
	std::filesystem::file_time_type best_time{};
	for (const auto& entry : std::filesystem::directory_iterator(build, ec)) {
		std::error_code entry_ec;
		if (!entry.is_directory(entry_ec)) {
			continue;
		}
		const std::filesystem::path candidate = entry.path() / "compile_commands.json";
		std::error_code exists_ec;
		if (std::filesystem::exists(candidate, exists_ec)) {
			const std::filesystem::file_time_type time = std::filesystem::last_write_time(candidate, exists_ec);
			if (!best || time > best_time) {
				best = candidate;
				best_time = time;
			}
		}
	}
	return best;
}

auto gse::ide::analysis::diagnostics_runner::start(const std::shared_ptr<diagnostics_check>& check, const std::filesystem::path& compile_commands, const std::filesystem::path& file, const std::filesystem::path& plugin_dll, void (*lint_hook)(diagnostics_check&)) -> void {
	std::thread([check, compile_commands, file, plugin_dll, lint_hook] {
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
		if (entry) {
			const std::filesystem::path sarif_temp = std::filesystem::temp_directory_path() / ("gseditor_diag_" + std::to_string(check->document_id) + ".sarif");

			std::string command_line = entry->command.command_line;
			std::filesystem::path token_temp;
			if (!plugin_dll.empty()) {
				token_temp = std::filesystem::temp_directory_path() / ("gseditor_tok_" + std::to_string(check->document_id) + ".txt");
				command_line += " -fplugin=\"" + plugin_dll.generic_native_encoded_string() + "\"";
				command_line += " -fplugin-arg-gse_tokens-out=\"" + token_temp.generic_native_encoded_string() + "\"";
			}

			const std::string directory = entry->command.directory.native_encoded_string();
			const std::string sarif_path = sarif_temp.native_encoded_string();
			const process::run_result run = process::run_capture_stderr(command_line.c_str(), directory.c_str(), sarif_path.c_str());

			std::error_code ec;
			const std::string sarif = read_file(sarif_temp);
			std::filesystem::remove(sarif_temp, ec);
			check->result = gcc_diagnostics::parse_sarif(sarif);

			for (const diagnostic& d : check->result) {
				if (d.message.contains("must be built")
					|| d.message.contains("failed to read compiled module")
					|| d.message.contains("returning to the gate")) {
					check->failed = true;
					break;
				}
			}

			check->crashed = (!run.launched || run.timed_out || run.exit_code != 0) && check->result.empty();
			if (check->crashed) {
				check->crash_output = sarif;
				std::string reason = sarif.substr(0, std::min<std::size_t>(sarif.size(), 4000));
				if (reason.empty()) {
					reason = "(no compiler output captured before exit — likely a plugin segfault)";
				}
				gse::log::println(gse::log::level::error, gse::log::category::task, "analyzer exited abnormally (code {}) on {}:\n{}", run.exit_code, file.filename().display_string(), reason);
			}

			if (!token_temp.empty()) {
				const std::string token_text = read_file(token_temp);
				std::filesystem::remove(token_temp, ec);
				check->tokens = semantic_tokens::parse(token_text);

				symbol_set symbols = symbol_tokens::parse(token_text);
				check->symbols = std::move(symbols.symbols);
				check->refs = std::move(symbols.refs);
				check->quals = std::move(symbols.quals);
				check->template_args = std::move(symbols.template_args);
				check->symbols_complete = symbols.complete;
			}
		}

		if (lint_hook) {
			lint_hook(*check);
		}

		check->duration = gse::system_clock::now<gse::time>() - started;
		check->done.store(true, std::memory_order_release);
	}).detach();
}
