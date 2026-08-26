module gse.ide.analysis:compilation_database_impl;

import std;
import gse;

import :compilation_database;
import :json;

namespace gse::ide::analysis {
	struct compilation_database_cache_entry {
		std::filesystem::file_time_type modified;
		std::uintmax_t size = 0;
		std::shared_ptr<const compilation_database> database;
	};

	std::mutex compilation_database_cache_mutex;
	std::unordered_map<id, compilation_database_cache_entry> compilation_database_cache;

	struct dyndep_edge {
		std::vector<std::filesystem::path> provided;
		std::vector<std::filesystem::path> required;
	};

	struct dyndep_graph {
		std::unordered_map<id, dyndep_edge> by_output;
		std::unordered_map<id, dyndep_edge> by_provided;
	};

	struct dyndep_cache_entry {
		std::filesystem::file_time_type modified;
		std::uintmax_t size = 0;
		std::shared_ptr<const dyndep_graph> graph;
	};

	std::mutex dyndep_cache_mutex;
	std::unordered_map<id, dyndep_cache_entry> dyndep_cache;

	auto tokenize_command(
		std::string_view command
	) -> std::vector<std::string>;

	auto quote_argument(
		std::string_view argument
	) -> std::string;

	auto command_tokens(
		const json::value& entry
	) -> std::vector<std::string>;

	auto make_check_command(
		std::vector<std::string> tokens,
		const std::filesystem::path& directory,
		std::filesystem::path output
	) -> check_command;

	auto append_ninja_path(
		std::string& token,
		const std::filesystem::path& directory,
		std::vector<std::filesystem::path>& paths
	) -> void;

	auto ninja_paths(
		std::string_view text,
		const std::filesystem::path& directory
	) -> std::vector<std::filesystem::path>;

	auto parse_dyndep(
		std::string_view text,
		const std::filesystem::path& directory
	) -> std::shared_ptr<dyndep_graph>;

	auto dyndep_path(
		const std::filesystem::path& output,
		const std::filesystem::path& directory
	) -> std::filesystem::path;

	auto load_dyndep(
		const std::filesystem::path& path,
		const std::filesystem::path& directory
	) -> std::shared_ptr<const dyndep_graph>;

	auto validate_module_edge(
		const dyndep_edge& edge,
		const std::filesystem::path& directory,
		std::unordered_set<id>& visiting,
		std::unordered_set<id>& validated
	) -> std::expected<void, std::string>;

	auto validate_module(
		const std::filesystem::path& module,
		const std::filesystem::path& directory,
		std::unordered_set<id>& visiting,
		std::unordered_set<id>& validated
	) -> std::expected<void, std::string>;

	auto parse_compilation_database(
		std::string_view text
	) -> std::shared_ptr<compilation_database>;
}

auto gse::ide::analysis::tokenize_command(const std::string_view command) -> std::vector<std::string> {
	std::vector<std::string> tokens;
	std::string current;
	bool in_quote = false;
	for (const char c : command) {
		if (c == '"') {
			in_quote = !in_quote;
			current.push_back(c);
		}
		else if ((c == ' ' || c == '\t') && !in_quote) {
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
	return tokens;
}

auto gse::ide::analysis::quote_argument(const std::string_view argument) -> std::string {
	if (!argument.contains(' ') && !argument.contains('\t') && !argument.contains('"')) {
		return std::string(argument);
	}
	std::string quoted;
	quoted.reserve(argument.size() + 2);
	quoted.push_back('"');
	for (const char c : argument) {
		if (c == '"') {
			quoted.push_back('\\');
		}
		quoted.push_back(c);
	}
	quoted.push_back('"');
	return quoted;
}

auto gse::ide::analysis::command_tokens(const json::value& entry) -> std::vector<std::string> {
	if (const json::value* command = entry.find("command"); command && !command->as_string().empty()) {
		return tokenize_command(command->as_string());
	}
	std::vector<std::string> tokens;
	if (const json::value* arguments = entry.find("arguments"); arguments && arguments->is_array()) {
		tokens.reserve(arguments->children.size());
		for (const json::value& argument : arguments->children) {
			tokens.push_back(quote_argument(argument.as_string()));
		}
	}
	return tokens;
}

auto gse::ide::analysis::make_check_command(std::vector<std::string> tokens, const std::filesystem::path& directory, std::filesystem::path output) -> check_command {
	std::vector<std::string> kept;
	kept.reserve(tokens.size() + 2);
	for (std::size_t i = 0; i < tokens.size(); ++i) {
		const std::string& token = tokens[i];
		if (token == "-c" || token == "-MD" || token == "-MMD" || token.starts_with("-fdeps-format=")) {
			continue;
		}
		if (token == "-o" || token == "-MF" || token == "-MT" || token == "-MQ" || token == "-fdeps-file" || token == "-fdeps-target" || token == "-fdeps-format") {
			++i;
			continue;
		}
		if ((token.starts_with("-MF") && token.size() > 3) || (token.starts_with("-MT") && token.size() > 3) || (token.starts_with("-MQ") && token.size() > 3) || token.starts_with("-fdeps-file=") || token.starts_with("-fdeps-target=")) {
			continue;
		}
		kept.push_back(std::move(tokens[i]));
	}
	kept.emplace_back("-fsyntax-only");
	kept.emplace_back("-fdiagnostics-format=sarif-stderr");

	std::string command_line;
	for (std::size_t i = 0; i < kept.size(); ++i) {
		if (i > 0) {
			command_line.push_back(' ');
		}
		command_line += kept[i];
	}

	std::uint64_t fingerprint = stable_id(command_line);
	fingerprint = hash_combine(fingerprint, stable_id(directory.generic_native_encoded_string()));

	return {
		.command_line = std::move(command_line),
		.directory = directory,
		.output = std::move(output),
		.fingerprint = fingerprint,
	};
}

auto gse::ide::analysis::append_ninja_path(std::string& token, const std::filesystem::path& directory, std::vector<std::filesystem::path>& paths) -> void {
	if (token.empty()) {
		return;
	}
	std::filesystem::path path(std::move(token));
	token.clear();
	if (path.is_relative()) {
		path = directory / path;
	}
	paths.push_back(path.lexically_normal());
}

auto gse::ide::analysis::ninja_paths(const std::string_view text, const std::filesystem::path& directory) -> std::vector<std::filesystem::path> {
	std::vector<std::filesystem::path> paths;
	std::string token;
	for (std::size_t i = 0; i <= text.size(); ++i) {
		const char c = i < text.size() ? text[i] : ' ';
		if (c == '$' && i + 1 < text.size()) {
			token.push_back(text[++i]);
		}
		else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
			append_ninja_path(token, directory, paths);
		}
		else {
			token.push_back(c);
		}
	}
	return paths;
}

auto gse::ide::analysis::parse_dyndep(const std::string_view text, const std::filesystem::path& directory) -> std::shared_ptr<dyndep_graph> {
	auto graph = std::make_shared<dyndep_graph>();
	for (std::size_t line_start = 0; line_start < text.size();) {
		const std::size_t line_end = text.find('\n', line_start);
		const std::string_view line = text.substr(line_start, line_end == std::string_view::npos ? text.size() - line_start : line_end - line_start);
		line_start = line_end == std::string_view::npos ? text.size() : line_end + 1;
		if (!line.starts_with("build ")) {
			continue;
		}
		const std::size_t rule = line.find(": dyndep");
		if (rule == std::string_view::npos) {
			continue;
		}
		const std::string_view outputs = line.substr(6, rule - 6);
		const std::size_t implicit_outputs = outputs.find(" | ");
		const std::vector<std::filesystem::path> primary = ninja_paths(outputs.substr(0, implicit_outputs), directory);
		if (primary.empty()) {
			continue;
		}
		dyndep_edge edge;
		if (implicit_outputs != std::string_view::npos) {
			edge.provided = ninja_paths(outputs.substr(implicit_outputs + 3), directory);
		}
		const std::string_view inputs = line.substr(rule + 8);
		const std::size_t implicit_inputs = inputs.find('|');
		if (implicit_inputs != std::string_view::npos) {
			edge.required = ninja_paths(inputs.substr(implicit_inputs + 1), directory);
		}
		graph->by_output.emplace(generate_temp_id(primary.front()), edge);
		for (const std::filesystem::path& provided : edge.provided) {
			graph->by_provided.emplace(generate_temp_id(provided), edge);
		}
	}
	return graph;
}

auto gse::ide::analysis::dyndep_path(const std::filesystem::path& output, const std::filesystem::path& directory) -> std::filesystem::path {
	for (std::filesystem::path current = output.parent_path(); !current.empty(); current = current.parent_path()) {
		if (current.filename().native_encoded_string().ends_with(".dir")) {
			return current / "CXX.dd";
		}
		if (current == directory) {
			break;
		}
	}
	return {};
}

auto gse::ide::analysis::load_dyndep(const std::filesystem::path& path, const std::filesystem::path& directory) -> std::shared_ptr<const dyndep_graph> {
	if (path.empty()) {
		return {};
	}
	std::error_code time_ec;
	const std::filesystem::file_time_type modified = std::filesystem::last_write_time(path, time_ec);
	std::error_code size_ec;
	const std::uintmax_t size = std::filesystem::file_size(path, size_ec);
	if (time_ec || size_ec) {
		return {};
	}
	const id path_id = generate_temp_id(path);
	{
		std::lock_guard lock(dyndep_cache_mutex);
		if (const auto found = dyndep_cache.find(path_id); found != dyndep_cache.end() && found->second.modified == modified && found->second.size == size) {
			return found->second.graph;
		}
	}
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return {};
	}
	std::ostringstream stream;
	stream << in.rdbuf();
	std::shared_ptr<dyndep_graph> graph = parse_dyndep(stream.str(), directory);
	{
		std::lock_guard lock(dyndep_cache_mutex);
		dyndep_cache[path_id] = {
			.modified = modified,
			.size = size,
			.graph = graph,
		};
	}
	return graph;
}

auto gse::ide::analysis::validate_module(const std::filesystem::path& module, const std::filesystem::path& directory, std::unordered_set<id>& visiting, std::unordered_set<id>& validated) -> std::expected<void, std::string> {
	std::error_code exists_ec;
	if (!std::filesystem::is_regular_file(module, exists_ec)) {
		return std::unexpected(std::format("compiled module '{}' is missing", module.generic_display_string()));
	}
	const id module_id = generate_temp_id(module);
	if (validated.contains(module_id)) {
		return {};
	}
	if (!visiting.insert(module_id).second) {
		return {};
	}
	const auto erase_visiting = make_scope_exit([&] {
		visiting.erase(module_id);
	});
	const std::shared_ptr<const dyndep_graph> graph = load_dyndep(dyndep_path(module, directory), directory);
	if (!graph) {
		validated.insert(module_id);
		return {};
	}
	const auto provider = graph->by_provided.find(module_id);
	if (provider == graph->by_provided.end()) {
		validated.insert(module_id);
		return {};
	}
	const std::expected<void, std::string> status = validate_module_edge(provider->second, directory, visiting, validated);
	if (status) {
		validated.insert(module_id);
	}
	return status;
}

auto gse::ide::analysis::validate_module_edge(const dyndep_edge& edge, const std::filesystem::path& directory, std::unordered_set<id>& visiting, std::unordered_set<id>& validated) -> std::expected<void, std::string> {
	for (const std::filesystem::path& required : edge.required) {
		if (const std::expected<void, std::string> status = validate_module(required, directory, visiting, validated); !status) {
			return status;
		}
	}
	for (const std::filesystem::path& provided : edge.provided) {
		std::error_code provided_ec;
		const std::filesystem::file_time_type provided_time = std::filesystem::last_write_time(provided, provided_ec);
		if (provided_ec) {
			return std::unexpected(std::format("compiled module '{}' is missing", provided.generic_display_string()));
		}
		for (const std::filesystem::path& required : edge.required) {
			std::error_code required_ec;
			const std::filesystem::file_time_type required_time = std::filesystem::last_write_time(required, required_ec);
			if (required_ec) {
				return std::unexpected(std::format("compiled module dependency '{}' is missing", required.generic_display_string()));
			}
			if (provided_time < required_time) {
				return std::unexpected(std::format("compiled module '{}' is older than dependency '{}'", provided.generic_display_string(), required.generic_display_string()));
			}
		}
	}
	return {};
}

auto gse::ide::analysis::parse_compilation_database(const std::string_view text) -> std::shared_ptr<compilation_database> {
	const std::optional<json::value> root = json::parse(text);
	if (!root || !root->is_array()) {
		return {};
	}

	auto database = std::make_shared<compilation_database>();
	database->content_hash = stable_id(text);
	database->entries.reserve(root->children.size());
	for (const json::value& value : root->children) {
		const json::value* file_value = value.find("file");
		if (!file_value || file_value->as_string().empty()) {
			continue;
		}

		std::filesystem::path directory;
		if (const json::value* directory_value = value.find("directory")) {
			directory = std::filesystem::path(std::string(directory_value->as_string()));
		}
		std::filesystem::path file = std::filesystem::path(std::string(file_value->as_string()));
		if (file.is_relative()) {
			file = directory / file;
		}
		std::error_code ec;
		const std::filesystem::path canonical = std::filesystem::weakly_canonical(file, ec);
		if (!ec) {
			file = canonical;
		}
		std::filesystem::path output;
		if (const json::value* output_value = value.find("output")) {
			output = std::filesystem::path(std::string(output_value->as_string()));
			if (output.is_relative()) {
				output = directory / output;
			}
			output = output.lexically_normal();
		}

		std::vector<std::string> tokens = command_tokens(value);
		if (tokens.empty()) {
			continue;
		}
		compilation_entry entry{
			.file = file,
			.command = make_check_command(std::move(tokens), directory, std::move(output)),
		};
		const id file_id = generate_temp_id(file);
		if (compilation_entry* existing = database->entries.try_get(file_id)) {
			*existing = std::move(entry);
		}
		else {
			database->entries.add(file_id, std::move(entry));
		}
	}
	return database;
}

auto gse::ide::analysis::validate_module_graph(const compilation_entry& entry) -> std::expected<void, std::string> {
	if (entry.command.output.empty()) {
		return {};
	}
	const std::shared_ptr<const dyndep_graph> graph = load_dyndep(dyndep_path(entry.command.output, entry.command.directory), entry.command.directory);
	if (!graph) {
		return {};
	}
	const auto found = graph->by_output.find(generate_temp_id(entry.command.output));
	if (found == graph->by_output.end()) {
		return {};
	}
	std::unordered_set<id> visiting;
	std::unordered_set<id> validated;
	return validate_module_edge(found->second, entry.command.directory, visiting, validated);
}

auto gse::ide::analysis::compilation_database::find(const std::filesystem::path& file) const -> const compilation_entry* {
	return entries.try_get(generate_temp_id(file));
}

auto gse::ide::analysis::load_compilation_database(const std::filesystem::path& path) -> std::shared_ptr<const compilation_database> {
	std::error_code canonical_ec;
	const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, canonical_ec);
	const std::filesystem::path resolved = canonical_ec ? path.lexically_normal() : canonical;
	const id database_id = generate_temp_id(resolved);

	std::error_code time_ec;
	const std::filesystem::file_time_type modified = std::filesystem::last_write_time(resolved, time_ec);
	std::error_code size_ec;
	const std::uintmax_t size = std::filesystem::file_size(resolved, size_ec);
	if (time_ec || size_ec) {
		return {};
	}

	{
		std::lock_guard lock(compilation_database_cache_mutex);
		if (const auto it = compilation_database_cache.find(database_id); it != compilation_database_cache.end() && it->second.modified == modified && it->second.size == size) {
			return it->second.database;
		}
	}

	std::ifstream in(resolved, std::ios::binary);
	if (!in) {
		return {};
	}
	std::ostringstream stream;
	stream << in.rdbuf();
	std::shared_ptr<compilation_database> database = parse_compilation_database(stream.str());
	if (!database) {
		return {};
	}

	{
		std::lock_guard lock(compilation_database_cache_mutex);
		compilation_database_cache[database_id] = {
			.modified = modified,
			.size = size,
			.database = database,
		};
	}
	return database;
}