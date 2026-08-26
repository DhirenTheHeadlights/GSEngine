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
	struct diagnostics_status_info {
		char label[80];
		char hint[384];
		std::uint32_t color = 0;
		bool routine = false;
	};

	enum class diagnostics_status {
		success,
		not_analyzed [[= diagnostics_status_info{
			.label = "not analyzed yet",
			.hint = "This file has not been through the analyzer since it was opened, so it has no diagnostics and no semantic highlighting. Analysis runs one file at a time, so it may still be queued behind another file.",
			.color = 0x9ca5b8,
			.routine = true,
		}]],
		module_unavailable [[= diagnostics_status_info{
			.label = "analysis blocked - stale compiled module interface",
			.hint = "A module this file imports has no usable compiled interface, so the compiler stopped before it reached this file. Build the project to restore diagnostics and semantic highlighting.",
			.color = 0xc275ba,
		}]],
		database_unavailable [[= diagnostics_status_info{
			.label = "analysis unavailable - no compile database",
			.hint = "compile_commands.json could not be read, so no compiler command is known for any file. Configure or build the project.",
			.color = 0x9ca5b8,
			.routine = true,
		}]],
		entry_unavailable [[= diagnostics_status_info{
			.label = "analysis unavailable - file is not in this project's build",
			.hint = "No entry in compile_commands.json covers this file, so there is no command to analyze it with.",
			.color = 0x9ca5b8,
			.routine = true,
		}]],
		launch_failed [[= diagnostics_status_info{
			.label = "analysis failed - the compiler could not be launched",
			.hint = "The analyzer process never started. The compiler may be missing from PATH, or its working directory may no longer exist.",
			.color = 0xda736c,
		}]],
		timed_out [[= diagnostics_status_info{
			.label = "analysis timed out",
			.hint = "The compiler did not finish within the analysis time limit and was terminated.",
			.color = 0xb5911c,
		}]],
		compiler_failed [[= diagnostics_status_info{
			.label = "analysis failed - the compiler exited without diagnostics",
			.hint = "The compiler reported a failure code but produced no parseable diagnostics, which usually means it crashed or the token plugin faulted.",
			.color = 0xda736c,
		}]],
		plugin_unavailable [[= diagnostics_status_info{
			.label = "semantic highlighting unavailable - token plugin not built",
			.hint = "Diagnostics are current, but colours, hover and go-to-definition all come from the gse_tokens GCC plugin and it is missing, so the compiler was run without it. CMake only builds the plugin when the toolchain provides cc1plus.exe.a, which needs a GCC configured with --enable-plugin; reconfigure after installing such a toolchain.",
			.color = 0xb5911c,
			.routine = true,
		}]],
		cancelled [[= diagnostics_status_info{
			.label = "analysis cancelled",
			.hint = "The analyzer was stopped before it finished, so this file kept its previous state. It will be analyzed again.",
			.color = 0x9ca5b8,
			.routine = true,
		}]]
	};

	auto status_info(
		diagnostics_status status
	) -> diagnostics_status_info;

	auto status_color(
		const diagnostics_status_info& info
	) -> vec4f;

	struct diagnostics_check {
		std::atomic<bool> done = false;
		id document_id;
		document_revision revision;
		std::vector<diagnostic> result;
		std::vector<diagnostic> lint;
		std::vector<qualified_use> quals;
		std::vector<qualified_use> template_args;
		std::vector<unused_local> unused_locals;
		std::vector<semantic_token> tokens;
		std::vector<symbol_token> symbols;
		std::vector<symbol_ref> refs;
		std::vector<param_token> params;
		std::unordered_map<file_id, std::filesystem::path> files;
		bool symbols_complete = false;
		diagnostics_status status = diagnostics_status::success;
		std::string failure_output;
		time duration;
	};

	struct diagnostics_runner {
		static auto start(
			const std::shared_ptr<diagnostics_check>& check,
			const std::filesystem::path& compile_commands,
			const std::filesystem::path& file,
			const std::filesystem::path& plugin_dll,
			std::span<const std::filesystem::path> workspace_roots,
			void (*lint_hook)(diagnostics_check&)
		) -> std::jthread;
	};
}

namespace gse::ide::analysis {
	auto normalize_diagnostic_files(
		std::span<diagnostic> diagnostics,
		const std::filesystem::path& directory
	) -> void;

	auto failure_detail(
		std::span<const diagnostic> diagnostics,
		std::string_view captured,
		const process::run_outcome& run
	) -> std::string;
}

auto gse::ide::analysis::status_info(const diagnostics_status status) -> diagnostics_status_info {
	return annotation_from_enum<diagnostics_status_info>(status, {
		.label = "analysis state unknown",
		.hint = "No explanation was recorded for this analysis state.",
		.color = 0x9ca5b8,
	});
}

auto gse::ide::analysis::status_color(const diagnostics_status_info& info) -> vec4f {
	return {
		static_cast<float>((info.color >> 16) & 0xffu) / 255.f,
		static_cast<float>((info.color >> 8) & 0xffu) / 255.f,
		static_cast<float>(info.color & 0xffu) / 255.f,
		1.f
	};
}

auto gse::ide::analysis::failure_detail(const std::span<const diagnostic> diagnostics, const std::string_view captured, const process::run_outcome& run) -> std::string {
	constexpr std::size_t detail_limit = 8;
	constexpr std::size_t captured_limit = 2000;

	std::string detail = gcc_diagnostics::summarize(diagnostics, detail_limit);
	if (detail.empty()) {
		const std::size_t sarif_start = captured.find('{');
		std::string_view text = sarif_start == std::string_view::npos ? captured : captured.substr(0, sarif_start);
		while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
			text.remove_suffix(1);
		}
		detail = text.empty()
			? std::string("The compiler produced no output before it exited.")
			: std::string(text.substr(0, std::min(text.size(), captured_limit)));
	}
	if (run) {
		detail += std::format("\nCompiler exit code {}.", *run);
	}
	return detail;
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

auto gse::ide::analysis::diagnostics_runner::start(const std::shared_ptr<diagnostics_check>& check, const std::filesystem::path& compile_commands, const std::filesystem::path& file, const std::filesystem::path& plugin_dll, const std::span<const std::filesystem::path> workspace_roots, void (*lint_hook)(diagnostics_check&)) -> std::jthread {
	std::vector<std::filesystem::path> roots(workspace_roots.begin(), workspace_roots.end());
	return std::jthread([check, compile_commands, file, plugin_dll, roots = std::move(roots), lint_hook](const std::stop_token& stop) {
		const time started = system_clock::now<time>();
		const auto publish = make_scope_exit([check, started] {
			check->duration = system_clock::now<time>() - started;
			check->done.store(true, std::memory_order_release);
		});
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
			check->failure_output = std::format("Looked for {}", compile_commands.generic_display_string());
		}
		else if (!entry) {
			check->status = diagnostics_status::entry_unavailable;
			check->failure_output = std::format("{} has no entry in {}", file.generic_display_string(), compile_commands.filename().generic_display_string());
		}
		else if (const std::expected<void, std::string> module_graph = validate_module_graph(*entry); !module_graph) {
			check->status = diagnostics_status::module_unavailable;
			check->failure_output = module_graph.error();
		}
		else {
			const std::filesystem::path sarif_temp = process::temporary_path("diagnostics", "sarif");
			const auto remove_sarif = make_scope_exit([&sarif_temp] {
				std::error_code ec;
				std::filesystem::remove(sarif_temp, ec);
			});

			std::string command_line = entry->command.command_line;
			std::filesystem::path token_temp;
			const auto remove_tokens = make_scope_exit([&token_temp] {
				if (!token_temp.empty()) {
					std::error_code ec;
					std::filesystem::remove(token_temp, ec);
				}
			});
			std::error_code plugin_ec;
			const bool plugin_missing = plugin_dll.empty() || !std::filesystem::exists(plugin_dll, plugin_ec);
			if (!plugin_missing) {
				token_temp = process::temporary_path("tokens", "txt");
				command_line += " -fplugin=\"" + plugin_dll.generic_native_encoded_string() + "\"";
				command_line += " -fplugin-arg-gse_tokens-out=\"" + token_temp.generic_native_encoded_string() + "\"";
				for (const std::filesystem::path& root : roots) {
					command_line += " -fplugin-arg-gse_tokens-root=\"" + root.generic_native_encoded_string() + "\"";
				}
			}

			const process::run_outcome run = process::run_capture_stderr(command_line, entry->command.directory, sarif_temp, stop);

			const std::string sarif = read_file(sarif_temp);
			check->result = gcc_diagnostics::parse_sarif(sarif);

			if (!run && run.error() == process::run_error::cancelled) {
				check->status = diagnostics_status::cancelled;
			}
			else if (!run) {
				check->status = run.error() == process::run_error::timed_out ? diagnostics_status::timed_out : diagnostics_status::launch_failed;
			}
			else if (gcc_diagnostics::is_module_unavailable(check->result)) {
				check->status = diagnostics_status::module_unavailable;
			}
			else if (*run != 0 && !gcc_diagnostics::has_error(check->result)) {
				check->status = diagnostics_status::compiler_failed;
			}
			else if (plugin_missing) {
				check->status = diagnostics_status::plugin_unavailable;
			}

			if (check->status == diagnostics_status::plugin_unavailable) {
				check->failure_output = plugin_dll.empty()
					? std::string("No token plugin path is configured.")
					: std::format("Looked for {}", plugin_dll.generic_display_string());
			}
			else if (check->status != diagnostics_status::success) {
				check->failure_output = failure_detail(check->result, sarif, run);
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
				check->unused_locals = std::move(symbols.unused_locals);
				check->files = std::move(symbols.files);
				check->symbols_complete = symbols.complete;
			}
		}

		if (check->status != diagnostics_status::success) {
			const diagnostics_status_info info = status_info(check->status);
			log::println(
				info.routine ? log::level::warning : log::level::error,
				log::category::task,
				"analysis: {} on {}:\n{}",
				std::string_view(info.label),
				file.filename().generic_display_string(),
				check->failure_output
			);
		}

		if (lint_hook) {
			lint_hook(*check);
		}
		if (entry) {
			normalize_diagnostic_files(check->result, entry->command.directory);
			normalize_diagnostic_files(check->lint, entry->command.directory);
		}
	});
}