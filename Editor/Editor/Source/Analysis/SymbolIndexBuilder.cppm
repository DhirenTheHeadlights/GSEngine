export module gse.ide.analysis:symbol_index_builder;

import std;

import :process;
import :gcc_diagnostics;
import :symbol_extract;

export namespace gse::ide::analysis {
	struct tu_symbols {
		std::filesystem::path tu;
		symbol_set set;
		bool ok = false;
	};

	struct symbol_index_builder {
		static auto run_one(
			const std::string& compile_commands_json,
			const std::filesystem::path& file,
			const std::filesystem::path& plugin_dll,
			const std::filesystem::path& workspace_root,
			std::uint32_t slot
		) -> tu_symbols;
	};
}

auto gse::ide::analysis::symbol_index_builder::run_one(const std::string& compile_commands_json, const std::filesystem::path& file, const std::filesystem::path& plugin_dll, const std::filesystem::path& workspace_root, const std::uint32_t slot) -> tu_symbols {
	tu_symbols out;
	out.tu = file;
	if (plugin_dll.empty()) {
		return out;
	}

	const std::optional<check_command> command = gcc_diagnostics::build_check_command(compile_commands_json, file);
	if (!command) {
		return out;
	}

	const std::filesystem::path token_temp = std::filesystem::temp_directory_path() / ("gseditor_sym_" + std::to_string(slot) + ".txt");
	const std::filesystem::path sarif_temp = std::filesystem::temp_directory_path() / ("gseditor_sym_" + std::to_string(slot) + ".sarif");

	std::string command_line = command->command_line;
	command_line += " -fplugin=\"" + plugin_dll.generic_native_encoded_string() + "\"";
	command_line += " -fplugin-arg-gse_tokens-out=\"" + token_temp.generic_native_encoded_string() + "\"";
	command_line += " -fplugin-arg-gse_tokens-root=\"" + workspace_root.generic_native_encoded_string() + "\"";

	const std::string directory = command->directory.native_encoded_string();
	const std::string sarif_path = sarif_temp.native_encoded_string();
	process::run_capture_stderr(command_line.c_str(), directory.c_str(), sarif_path.c_str());

	std::error_code ec;
	std::filesystem::remove(sarif_temp, ec);

	std::ifstream in(token_temp, std::ios::binary);
	if (in) {
		std::ostringstream stream;
		stream << in.rdbuf();
		out.set = symbol_tokens::parse(stream.str());
		out.ok = true;
	}
	std::filesystem::remove(token_temp, ec);
	return out;
}
