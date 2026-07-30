export module gse.ide.config;

import std;

export import :config_system;

export namespace gse::ide::config {
	constexpr std::string_view editor_target = "Editor";

	auto game_target() -> std::string_view;

	struct browse_root {
		std::filesystem::path path;
		std::string name;
		std::filesystem::path compile_commands;
		bool is_project = false;
	};

	auto browse_roots() -> std::span<const browse_root>;

	auto analysis_roots() -> std::vector<std::filesystem::path>;

	auto compile_commands_for(
		const std::filesystem::path& file
	) -> const std::filesystem::path&;

	auto source_dir() -> const std::filesystem::path&;

	auto engine_root() -> const std::filesystem::path&;

	auto engine_source_dir() -> const std::filesystem::path&;

	auto project_root() -> const std::filesystem::path&;

	auto project_source_dir() -> const std::filesystem::path&;

	auto project_assets_dir() -> const std::filesystem::path&;

	auto project_state_dir() -> const std::filesystem::path&;

	auto project_settings() -> const std::filesystem::path&;

	auto resource_path() -> const std::filesystem::path&;

	auto project_build_dir() -> const std::filesystem::path&;

	auto project_output_dir() -> const std::filesystem::path&;

	auto project_compile_commands() -> const std::filesystem::path&;

	auto build_dir() -> const std::filesystem::path&;

	auto token_plugin() -> const std::filesystem::path&;

	auto cppref_index() -> const std::filesystem::path&;

	auto game_executable() -> const std::filesystem::path&;

	auto editor_executable() -> const std::filesystem::path&;

	auto editor_layout() -> const std::filesystem::path&;
}
