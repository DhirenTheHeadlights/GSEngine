export module gse.ide.project;

import std;

import gse.math;

export namespace gse::ide::project {
	struct engine_entry {
		std::string name;
		std::filesystem::path path;
	};

	struct manifest {
		bool valid = false;
		bool requested = false;
		std::string name;
		std::string engine_version;
		std::string engine_name;
		std::string engine_problem;
		std::map<std::string, std::string> targets;
		std::filesystem::path file;
		std::filesystem::path root;
		std::filesystem::path source;
		std::filesystem::path assets;
		std::filesystem::path state;
		std::filesystem::path engine;
		vec4f accent{ 0.f, 0.f, 0.f, 0.f };
	};

	auto engines() -> std::vector<engine_entry>;

	auto ensure_engine_registered(
		const std::filesystem::path& path
	) -> std::string;

	auto bind_engine(
		const std::filesystem::path& manifest_file,
		const engine_entry& engine
	) -> void;

	auto accent() -> vec4f;

	auto current() -> const manifest&;

	auto record_recent() -> void;

	struct project_template {
		std::string_view id;
		std::string_view label;
		std::string_view detail;
	};

	auto templates() -> std::span<const project_template>;

	auto validate_new(
		std::string_view name,
		const std::filesystem::path& parent
	) -> std::string;

	auto create(
		std::string_view name,
		const std::filesystem::path& parent,
		std::string_view template_id
	) -> std::expected<std::filesystem::path, std::string>;

	auto recent() -> std::vector<std::filesystem::path>;

	auto discover() -> std::vector<std::filesystem::path>;

	auto known() -> std::vector<std::filesystem::path>;

	auto load(
		const std::filesystem::path& file
	) -> manifest;

	auto target(
		std::string_view key,
		std::string_view fallback
	) -> std::string;
}
