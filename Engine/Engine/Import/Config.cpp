module;

#include <cstdio>

module gse.config;

import std;

import gse.meta;
import gse.win32;

namespace gse::config {
	constexpr int max_walk_up = 8;
	constexpr std::string_view manifest_name = "gse.manifest";

	struct resolved {
		run_mode mode;
		std::string exe_stem;
		std::filesystem::path root;
		std::filesystem::path build_root;
		std::filesystem::path source;
		std::filesystem::path resources;
		std::filesystem::path baked_resources;
		std::filesystem::path user_config;
		std::filesystem::path user_state;
		std::filesystem::path logs;
		std::filesystem::path cache;
		std::filesystem::path crash;
		std::filesystem::path profile;
		std::filesystem::path captures;
	};

	[[noreturn]] auto fatal(
		std::string_view detail
	) -> void;

	auto trim(
		std::string_view text
	) -> std::string_view;

	auto executable_path() -> std::filesystem::path;

	auto find_manifest(
		const std::filesystem::path& start
	) -> std::filesystem::path;

	auto read_file(
		const std::filesystem::path& path
	) -> std::string;

	auto manifest_value(
		std::string_view text,
		std::string_view key
	) -> std::string;

	auto known_folder(
		int csidl
	) -> std::filesystem::path;

	auto env_path(
		const char* name
	) -> std::filesystem::path;

	auto resolve() -> resolved;

	auto table() -> const resolved&;
}

auto gse::config::fatal(const std::string_view detail) -> void {
	std::fputs("gse.config: ", stderr);
	std::fwrite(detail.data(), 1, detail.size(), stderr);
	std::fputs("\ngse.config: cannot resolve engine paths; aborting.\n", stderr);
	std::fflush(stderr);
	std::abort();
}

auto gse::config::trim(const std::string_view text) -> std::string_view {
	constexpr std::string_view blanks = " \t\r";
	const std::size_t begin = text.find_first_not_of(blanks);
	if (begin == std::string_view::npos) {
		return {};
	}
	return text.substr(begin, text.find_last_not_of(blanks) - begin + 1);
}

auto gse::config::executable_path() -> std::filesystem::path {
	wchar_t buffer[win32::max_path]{};
	const win32::DWORD length = win32::GetModuleFileNameW(nullptr, buffer, win32::max_path);
	if (length == 0 || length >= win32::max_path) {
		return {};
	}
	return { std::wstring_view(buffer, length) };
}

auto gse::config::find_manifest(const std::filesystem::path& start) -> std::filesystem::path {
	std::filesystem::path directory = start;
	for (int level = 0; level < max_walk_up; ++level) {
		std::error_code ec;
		const std::filesystem::path candidate = directory / manifest_name;
		if (std::filesystem::exists(candidate, ec)) {
			return candidate;
		}
		const std::filesystem::path parent = directory.parent_path();
		if (parent.empty() || parent == directory) {
			break;
		}
		directory = parent;
	}
	return {};
}

auto gse::config::read_file(const std::filesystem::path& path) -> std::string {
	std::ifstream stream(path, std::ios::binary);
	if (!stream) {
		return {};
	}
	return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}

auto gse::config::manifest_value(const std::string_view text, const std::string_view key) -> std::string {
	for (const auto& line : std::views::split(text, '\n')) {
		const std::string_view entry(line);
		const std::size_t separator = entry.find('=');
		if (separator != std::string_view::npos && trim(entry.substr(0, separator)) == key) {
			return std::string(trim(entry.substr(separator + 1)));
		}
	}
	return {};
}

auto gse::config::known_folder(const int csidl) -> std::filesystem::path {
	wchar_t buffer[win32::max_path]{};
	if (win32::SHGetFolderPathW(nullptr, csidl, nullptr, win32::shgfp_type_current, buffer) != 0) {
		return {};
	}
	return { std::wstring_view(buffer) };
}

auto gse::config::env_path(const char* name) -> std::filesystem::path {
	const char* value = std::getenv(name);
	if (value == nullptr || *value == '\0') {
		return {};
	}
	return { value };
}

auto gse::config::generic(const std::filesystem::path& value) -> std::filesystem::path {
	return { value.generic_wstring() };
}

auto gse::config::resolve() -> resolved {
	const std::filesystem::path executable = executable_path();
	if (executable.empty()) {
		fatal("could not determine the running executable's path");
	}

	const std::filesystem::path manifest = find_manifest(executable.parent_path());
	if (manifest.empty()) {
		fatal("no gse.manifest found within 8 directories above the executable; run from a build tree or an install image");
	}

	const std::string text = read_file(manifest);
	const std::string mode_text = manifest_value(text, "mode");
	const std::string root_text = manifest_value(text, "root");

	run_mode active = run_mode::dev;
	enum_from_string(mode_text, active);

	const std::filesystem::path build = manifest.parent_path();
	const std::filesystem::path root = active == run_mode::installed || root_text.empty() ? build : std::filesystem::path(root_text);

	std::filesystem::path config_root = env_path("GSE_USER_DIR");
	if (config_root.empty()) {
		const std::filesystem::path appdata = known_folder(win32::csidl_appdata);
		if (appdata.empty()) {
			fatal("could not resolve the roaming AppData folder");
		}
		config_root = appdata / "GSE";
	}

	std::filesystem::path state_root = env_path("GSE_STATE_DIR");
	if (state_root.empty()) {
		const std::filesystem::path local = known_folder(win32::csidl_local_appdata);
		if (local.empty()) {
			fatal("could not resolve the local AppData folder");
		}
		state_root = local / "GSE";
	}

	return {
		.mode = active,
		.exe_stem = executable.stem().native_encoded_string(),
		.root = generic(root),
		.build_root = generic(build),
		.source = generic(active == run_mode::installed ? root / "Engine" / "Source" : root / "Engine" / "Engine"),
		.resources = generic(root / "Engine" / "Resources"),
		.baked_resources = generic(active == run_mode::installed ? root / "Engine" / "Baked" : build / "Engine" / "Resources"),
		.user_config = generic(config_root),
		.user_state = generic(state_root),
		.logs = generic(state_root / "logs"),
		.cache = generic(state_root / "cache"),
		.crash = generic(state_root / "crash"),
		.profile = generic(state_root / "profile"),
		.captures = generic(state_root / "captures")
	};
}

auto gse::config::table() -> const resolved& {
	static const resolved value = resolve();
	return value;
}

auto gse::config::warm_up() -> void {
	(void)table();
}

auto gse::config::mode() -> run_mode {
	return table().mode;
}

auto gse::config::executable_stem() -> std::string_view {
	return table().exe_stem;
}

auto gse::config::root_dir() -> const std::filesystem::path& {
	return table().root;
}

auto gse::config::build_root() -> const std::filesystem::path& {
	return table().build_root;
}

auto gse::config::source_dir() -> const std::filesystem::path& {
	return table().source;
}

auto gse::config::resource_path() -> const std::filesystem::path& {
	return table().resources;
}

auto gse::config::baked_resource_path() -> const std::filesystem::path& {
	return table().baked_resources;
}

auto gse::config::user_config_dir() -> const std::filesystem::path& {
	return table().user_config;
}

auto gse::config::user_state_dir() -> const std::filesystem::path& {
	return table().user_state;
}

auto gse::config::logs_dir() -> const std::filesystem::path& {
	return table().logs;
}

auto gse::config::cache_dir() -> const std::filesystem::path& {
	return table().cache;
}

auto gse::config::crash_dir() -> const std::filesystem::path& {
	return table().crash;
}

auto gse::config::profile_dir() -> const std::filesystem::path& {
	return table().profile;
}

auto gse::config::captures_dir() -> const std::filesystem::path& {
	return table().captures;
}
