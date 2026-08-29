export module gse.ide.analysis:compilation_database;

import std;
import gse;

export namespace gse::ide::analysis {
	struct check_command {
		std::string command_line;
		std::filesystem::path directory;
		std::filesystem::path output;
		std::uint64_t fingerprint = 0;
	};

	struct compilation_entry {
		std::filesystem::path file;
		check_command command;
	};

	struct compilation_database {
		id_mapped_collection<compilation_entry> entries;
		std::uint64_t content_hash = 0;

		auto find(
			const std::filesystem::path& file
		) const -> const compilation_entry*;
	};

	auto load_compilation_database(
		const std::filesystem::path& path
	) -> std::shared_ptr<const compilation_database>;

	auto validate_module_graph(
		const compilation_entry& entry
	) -> std::expected<void, std::string>;
}