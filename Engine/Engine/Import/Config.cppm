export module gse.config;

import std;

export namespace gse::config {
	enum class run_mode : std::uint8_t {
		dev,
		installed
	};

	auto warm_up() -> void;

	auto mode() -> run_mode;

	auto debug_asset_output() -> bool;

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

	constexpr std::string_view engine_asset_prefix = "engine:";

	struct content_root {
		std::filesystem::path source;
		std::filesystem::path baked;
		std::string_view prefix;
		bool built_in = false;
	};

	auto content_roots() -> std::span<const content_root>;

	auto project_assets_path() -> const std::filesystem::path&;

	auto project_baked_path() -> const std::filesystem::path&;

	auto has_project() -> bool;

	auto asset_tag(
		const std::filesystem::path& baked_path
	) -> std::string;

	auto asset_prefix_of(
		const std::filesystem::path& baked_path
	) -> std::string_view;

	auto baked_root_of(
		const std::filesystem::path& baked_path
	) -> const std::filesystem::path&;

	auto source_root_of(
		const std::filesystem::path& baked_path
	) -> const std::filesystem::path&;

	auto source_root_containing(
		const std::filesystem::path& source_path
	) -> const std::filesystem::path&;

	auto baked_root_for_source(
		const std::filesystem::path& source_root
	) -> const std::filesystem::path&;

	auto user_config_dir() -> const std::filesystem::path&;

	auto user_state_dir() -> const std::filesystem::path&;

	auto projects_root() -> const std::filesystem::path&;

	auto project_root() -> const std::filesystem::path&;

	auto project_data_dir() -> const std::filesystem::path&;

	auto project_settings_path() -> const std::filesystem::path&;

	auto project_settings_path_for(
		const std::filesystem::path& root
	) -> std::filesystem::path;

	auto project_data_path(
		const std::filesystem::path& relative
	) -> std::filesystem::path;

	auto logs_dir() -> const std::filesystem::path&;

	auto cache_dir() -> const std::filesystem::path&;

	auto crash_dir() -> const std::filesystem::path&;

	auto profile_dir() -> const std::filesystem::path&;

	auto captures_dir() -> const std::filesystem::path&;
}
