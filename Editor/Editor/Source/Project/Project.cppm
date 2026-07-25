export module gse.ide.project;

import std;

import gse.math;

export namespace gse::ide::project {
	struct manifest {
		bool valid = false;
		std::string name;
		std::string engine_version;
		std::map<std::string, std::string> targets;
		std::filesystem::path file;
		std::filesystem::path root;
		std::filesystem::path source;
		std::filesystem::path assets;
		std::filesystem::path state;
		std::filesystem::path engine;
		vec4f accent{ 0.f, 0.f, 0.f, 0.f };
	};

	auto accent() -> vec4f;

	auto current() -> const manifest&;

	auto record_recent() -> void;

	auto recent() -> std::vector<std::filesystem::path>;

	auto load(
		const std::filesystem::path& file
	) -> manifest;

	auto target(
		std::string_view key,
		std::string_view fallback
	) -> std::string;
}
