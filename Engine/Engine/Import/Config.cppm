export module gse.config;

import std;

export namespace gse::config {
	enum class run_mode : std::uint8_t {
		dev,
		installed
	};

	auto warm_up() -> void;

	auto mode() -> run_mode;

	auto executable_stem() -> std::string_view;

	auto executable_file() -> const std::filesystem::path&;

	auto generic(
		const std::filesystem::path& value
	) -> std::filesystem::path;

	auto root_dir() -> const std::filesystem::path&;

	auto build_root() -> const std::filesystem::path&;

	auto source_dir() -> const std::filesystem::path&;

	auto resource_path() -> const std::filesystem::path&;

	auto baked_resource_path() -> const std::filesystem::path&;

	auto user_config_dir() -> const std::filesystem::path&;

	auto user_state_dir() -> const std::filesystem::path&;

	auto projects_root() -> const std::filesystem::path&;

	auto logs_dir() -> const std::filesystem::path&;

	auto cache_dir() -> const std::filesystem::path&;

	auto crash_dir() -> const std::filesystem::path&;

	auto profile_dir() -> const std::filesystem::path&;

	auto captures_dir() -> const std::filesystem::path&;
}
