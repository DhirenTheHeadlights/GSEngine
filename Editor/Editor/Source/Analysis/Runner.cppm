export module gse.ide.analysis:diagnostics_runner;

import gse.core;
import gse.ide.diagnostic;
import gse.log;
import gse.math;
import gse.meta;
import gse.time;
import std;

import :compilation_database;
import :gcc_diagnostics;
import :process;
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

	struct diagnostics_result {
		id document_id;
		document_revision revision;
		std::vector<diagnostic> diagnostics;
		std::vector<diagnostic> lint;
		std::vector<qualified_use> quals;
		std::vector<qualified_use> template_args;
		std::vector<unused_local> unused_locals;
		std::vector<narrowable_import> narrowable_imports;
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

	struct diagnostics_request {
		id document_id;
		document_revision revision;
		std::filesystem::path compile_commands;
		std::filesystem::path file;
		std::filesystem::path plugin;
		std::vector<std::filesystem::path> workspace_roots;
		void (*lint_hook)(diagnostics_result&) = nullptr;
	};

	auto run_diagnostics(
		const diagnostics_request& request,
		const std::stop_token& stop
	) -> diagnostics_result;
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

auto gse::ide::analysis::run_diagnostics(const diagnostics_request& request, const std::stop_token& stop) -> diagnostics_result {
	const time started = system_clock::now<time>();
	diagnostics_result out{
		.document_id = request.document_id,
		.revision = request.revision,
	};
	auto read_file = [](const std::filesystem::path& path) -> std::string {
		std::ifstream in(path, std::ios::binary);
		if (!in) {
			return {};
		}
		std::ostringstream stream;
		stream << in.rdbuf();
		return stream.str();
	};

	const std::shared_ptr<const compilation_database> database = load_compilation_database(request.compile_commands);
	const compilation_entry* entry = database ? database->find(request.file) : nullptr;
	if (!database) {
		out.status = diagnostics_status::database_unavailable;
		out.failure_output = std::format("Looked for {}", request.compile_commands.generic_display_string());
	}
	else if (!entry) {
		out.status = diagnostics_status::entry_unavailable;
		out.failure_output = std::format("{} has no entry in {}", request.file.generic_display_string(), request.compile_commands.filename().generic_display_string());
	}
	else if (const std::expected<void, std::string> module_graph = validate_module_graph(*entry); !module_graph) {
		out.status = diagnostics_status::module_unavailable;
		out.failure_output = module_graph.error();
	}
	else {
		const std::filesystem::path sarif_temp = process::temporary_path("diagnostics", "sarif");
		const auto _ = make_scope_exit([&sarif_temp] {
			std::error_code ec;
			std::filesystem::remove(sarif_temp, ec);
		});

		std::string command_line = entry->command.command_line;
		std::filesystem::path token_temp;
		const auto _ = make_scope_exit([&token_temp] {
			if (!token_temp.empty()) {
				std::error_code ec;
				std::filesystem::remove(token_temp, ec);
			}
		});
		std::error_code plugin_ec;
		const bool plugin_missing = request.plugin.empty() || !std::filesystem::exists(request.plugin, plugin_ec);
		if (!plugin_missing) {
			token_temp = process::temporary_path("tokens", "txt");
			command_line += " -fplugin=\"" + request.plugin.generic_native_encoded_string() + "\"";
			command_line += " -fplugin-arg-gse_tokens-out=\"" + token_temp.generic_native_encoded_string() + "\"";
			for (const std::filesystem::path& root : request.workspace_roots) {
				command_line += " -fplugin-arg-gse_tokens-root=\"" + root.generic_native_encoded_string() + "\"";
			}
		}

		const process::run_outcome run = process::run_capture_stderr(command_line, entry->command.directory, sarif_temp, stop);

		const std::string sarif = read_file(sarif_temp);
		out.diagnostics = gcc_diagnostics::parse_sarif(sarif);

		if (!run && run.error() == process::run_error::cancelled) {
			out.status = diagnostics_status::cancelled;
		}
		else if (!run) {
			out.status = run.error() == process::run_error::timed_out ? diagnostics_status::timed_out : diagnostics_status::launch_failed;
		}
		else if (gcc_diagnostics::is_module_unavailable(out.diagnostics)) {
			out.status = diagnostics_status::module_unavailable;
		}
		else if (*run != 0 && !gcc_diagnostics::has_error(out.diagnostics)) {
			out.status = diagnostics_status::compiler_failed;
		}
		else if (plugin_missing) {
			out.status = diagnostics_status::plugin_unavailable;
		}

		if (out.status == diagnostics_status::plugin_unavailable) {
			out.failure_output = request.plugin.empty()
				? std::string("No token plugin path is configured.")
				: std::format("Looked for {}", request.plugin.generic_display_string());
		}
		else if (out.status != diagnostics_status::success) {
			out.failure_output = failure_detail(out.diagnostics, sarif, run);
		}

		if (!token_temp.empty()) {
			const std::string token_text = read_file(token_temp);
			out.tokens = semantic_tokens::parse(token_text);

			symbol_set symbols = symbol_tokens::parse(token_text, request.file.generic_native_encoded_string());
			out.symbols = std::move(symbols.symbols);
			out.refs = std::move(symbols.refs);
			out.params = std::move(symbols.params);
			out.quals = std::move(symbols.quals);
			out.template_args = std::move(symbols.template_args);
			out.unused_locals = std::move(symbols.unused_locals);
			out.narrowable_imports = std::move(symbols.narrowable_imports);
			out.files = std::move(symbols.files);
			out.symbols_complete = symbols.complete;
		}
	}

	if (out.status != diagnostics_status::success) {
		const diagnostics_status_info info = status_info(out.status);
		log::println(
			info.routine ? log::level::warning : log::level::error,
			log::category::task,
			"analysis: {} on {}:\n{}",
			std::string_view(info.label),
			request.file.filename().generic_display_string(),
			out.failure_output
		);
	}

	if (request.lint_hook) {
		request.lint_hook(out);
	}
	if (entry) {
		normalize_diagnostic_files(out.diagnostics, entry->command.directory);
		normalize_diagnostic_files(out.lint, entry->command.directory);
	}
	out.duration = system_clock::now<time>() - started;
	return out;
}