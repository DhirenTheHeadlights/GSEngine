export module gse.sdk:registry;

import std;

import gse.config;
import gse.core;
import gse.fs;

export namespace gse::sdk {
	constexpr std::string_view image_marker = "Engine/cmake/GSEEngineSdk.cmake";

	using image_table = std::unordered_map<std::string, std::string, transparent_hash, transparent_equal>;

	auto registry_path() -> std::filesystem::path;

	auto is_image(
		const std::filesystem::path& path
	) -> bool;

	auto image_for_version(
		std::string_view version
	) -> std::filesystem::path;

	auto register_image(
		std::string_view version,
		const std::filesystem::path& parent
	) -> void;

	auto unregister_image(
		std::string_view version,
		const std::filesystem::path& parent
	) -> void;

	auto registered_images() -> image_table;
}

namespace gse::sdk {
	auto write_images(
		const image_table& entries
	) -> void;
}

auto gse::sdk::registry_path() -> std::filesystem::path {
	return config::user_config_dir() / "engines.ini";
}

auto gse::sdk::is_image(const std::filesystem::path& path) -> bool {
	if (path.empty()) {
		return false;
	}
	std::error_code ec;
	return std::filesystem::exists(path / "gse.manifest", ec) && std::filesystem::exists(path / image_marker, ec);
}

auto gse::sdk::registered_images() -> image_table {
	image_table entries;
	for (const layout_store::section& section : layout_store::parse_sections(layout_store::read(registry_path()))) {
		if (section.name == "sdks") {
			entries.insert(section.values.begin(), section.values.end());
		}
	}
	return entries;
}

auto gse::sdk::write_images(const image_table& entries) -> void {
	std::string block = "[sdks]\n";
	for (const auto& [name, path] : entries) {
		std::format_to(std::back_inserter(block), "{} = {}\n", name, path);
	}
	layout_store::submit(registry_path(), { .names = { "sdks" } }, std::move(block));
	layout_store::flush();
}

auto gse::sdk::image_for_version(const std::string_view version) -> std::filesystem::path {
	const image_table entries = registered_images();
	const auto entry = entries.find(version);
	return entry == entries.end() ? std::filesystem::path{} : config::generic(entry->second);
}

auto gse::sdk::register_image(const std::string_view version, const std::filesystem::path& parent) -> void {
	image_table entries = registered_images();
	std::string path = config::generic(parent).generic_native_encoded_string();
	if (const auto entry = entries.find(version); entry != entries.end()) {
		entry->second = std::move(path);
	}
	else {
		entries.emplace(version, std::move(path));
	}
	write_images(entries);
}

auto gse::sdk::unregister_image(const std::string_view version, const std::filesystem::path& parent) -> void {
	image_table entries = registered_images();
	const auto entry = entries.find(version);
	if (entry == entries.end() || config::generic(entry->second) != config::generic(parent)) {
		return;
	}
	entries.erase(entry);
	write_images(entries);
}
