module gse.config;

import std;

auto gse::config::executable_path() -> std::filesystem::path {
	const char* value = std::getenv("GSE_EXECUTABLE_PATH");
	if (value == nullptr || *value == '\0') {
		return {};
	}
	return { value };
}

auto gse::config::known_folder(known_directory) -> std::filesystem::path {
	return {};
}
