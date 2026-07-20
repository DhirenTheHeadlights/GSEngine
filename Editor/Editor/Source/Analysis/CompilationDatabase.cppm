export module gse.ide.analysis:compilation_database;

import std;
import gse;

import :json;

export namespace gse::ide::analysis {
	struct check_command {
		std::string command_line;
		std::filesystem::path directory;
		std::uint64_t fingerprint = 0;
	};

	struct compilation_entry {
		std::filesystem::path file;
		check_command command;
	};

	struct compilation_database {
		gse::id_mapped_collection<compilation_entry> entries;
		std::uint64_t content_hash = 0;

		auto find(const std::filesystem::path& file) const -> const compilation_entry*;
	};

	auto load_compilation_database(
		const std::filesystem::path& path
	) -> std::shared_ptr<const compilation_database>;
}

namespace gse::ide::analysis {
	struct compilation_database_cache_entry {
		std::filesystem::file_time_type modified;
		std::uintmax_t size = 0;
		std::shared_ptr<const compilation_database> database;
	};

	inline std::mutex compilation_database_cache_mutex;
	inline std::unordered_map<gse::id, compilation_database_cache_entry> compilation_database_cache;

	auto tokenize_command(std::string_view command) -> std::vector<std::string> {
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

	auto quote_argument(std::string_view argument) -> std::string {
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

	auto command_tokens(const json::value& entry) -> std::vector<std::string> {
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

	auto make_check_command(std::vector<std::string> tokens, const std::filesystem::path& directory) -> check_command {
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
			.fingerprint = fingerprint,
		};
	}

	auto parse_compilation_database(std::string_view text) -> std::shared_ptr<compilation_database> {
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

			std::vector<std::string> tokens = command_tokens(value);
			if (tokens.empty()) {
				continue;
			}
			compilation_entry entry{
				.file = file,
			.command = make_check_command(std::move(tokens), directory),
		};
		const gse::id file_id = gse::generate_temp_id(file);
		if (compilation_entry* existing = database->entries.try_get(file_id)) {
			*existing = std::move(entry);
		}
		else {
			database->entries.add(file_id, std::move(entry));
		}
		}
		return database;
	}
}

auto gse::ide::analysis::compilation_database::find(const std::filesystem::path& file) const -> const compilation_entry* {
	return entries.try_get(gse::generate_temp_id(file));
}

auto gse::ide::analysis::load_compilation_database(const std::filesystem::path& path) -> std::shared_ptr<const compilation_database> {
	std::error_code canonical_ec;
	const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, canonical_ec);
	const std::filesystem::path resolved = canonical_ec ? path.lexically_normal() : canonical;
	const gse::id database_id = gse::generate_temp_id(resolved);

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
