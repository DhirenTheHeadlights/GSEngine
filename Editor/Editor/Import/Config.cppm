export module gse.ide.config;

import std;

export import :config_system;

export namespace gse::ide::config {
	constexpr std::string_view editor_target = "Editor";

	struct worktree {
		std::string name;
		std::string game_target;
		bool has_manifest = false;
		std::filesystem::path engine_root;
		std::filesystem::path engine_source;
		std::filesystem::path project_root;
		std::filesystem::path project_source;
		std::filesystem::path project_assets;
		std::filesystem::path project_state;
		std::filesystem::path project_settings;
		std::filesystem::path project_build;
		std::filesystem::path project_output;
		std::filesystem::path project_compile_commands;
		std::filesystem::path game_executable;
	};

	struct browse_root {
		std::filesystem::path path;
		std::string name;
		std::filesystem::path compile_commands;
		bool is_project = false;
		bool analyzable = true;
	};

	struct browse_root_view {
		struct iterator {
			using value_type = browse_root;
			using difference_type = std::ptrdiff_t;

			const browse_root* const* cursor = nullptr;

			auto operator*() const -> const browse_root&;

			auto operator->() const -> const browse_root*;

			auto operator++() -> iterator&;

			auto operator++(
				int
			) -> iterator;

			auto operator==(
				const iterator& other
			) const -> bool = default;
		};

		std::shared_ptr<const std::vector<const browse_root*>> generation;

		auto begin() const -> iterator;

		auto end() const -> iterator;

		auto size() const -> std::size_t;

		auto empty() const -> bool;
	};

	auto worktrees() -> std::vector<const worktree*>;

	auto primary() -> const worktree&;

	auto worktree_for(
		const std::filesystem::path& file
	) -> const worktree&;

	auto register_worktree(
		const std::filesystem::path& manifest_file
	) -> const worktree&;

	auto browse_roots() -> browse_root_view;

	auto analysis_roots() -> std::vector<std::filesystem::path>;

	auto compile_commands_for(
		const std::filesystem::path& file
	) -> const std::filesystem::path&;

	auto game_target() -> std::string_view;

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

	auto editor_layout_default() -> const std::filesystem::path&;

	auto seed_editor_layout() -> void;
}
