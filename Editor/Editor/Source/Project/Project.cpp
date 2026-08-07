module gse.ide.project;

import std;

import gse.config;
import gse.fs;
import gse.log;
import gse.math;
import gse.win32;

namespace gse::ide::project {
	constexpr std::string_view manifest_extension = ".gseproj";
	constexpr std::string_view engine_marker = "Engine/cmake/GSEEngine.cmake";

	auto read_file(
		const std::filesystem::path& path
	) -> std::string;

	constexpr std::size_t recent_limit = 10;
	constexpr std::size_t max_project_path = 120;

	auto write_file(
		const std::filesystem::path& path,
		std::string_view text
	) -> bool;

	auto command_line_manifest() -> std::filesystem::path;

	auto recent_path() -> std::filesystem::path;

	auto module_namespace(
		std::string_view name
	) -> std::string;

	auto engines_path() -> std::filesystem::path;

	auto is_engine_tree(
		const std::filesystem::path& path
	) -> bool;

	auto engine_for_name(
		std::string_view name
	) -> std::filesystem::path;

	auto write_engines(
		const std::vector<engine_entry>& entries
	) -> void;

	auto recent_manifest() -> std::filesystem::path;

	auto dev_default_manifest() -> std::filesystem::path;

	auto hue_color(
		float hue
	) -> vec4f;

	auto resolve_accent(
		std::string_view text,
		std::string_view name
	) -> vec4f;

	auto resolve() -> manifest;
}

auto gse::ide::project::hue_color(const float hue) -> vec4f {
	constexpr float saturation = 0.62f;
	constexpr float value = 0.94f;
	const float chroma = value * saturation;
	const float sector = hue / 60.f;
	const float second = chroma * (1.f - std::abs(std::fmod(sector, 2.f) - 1.f));
	const float base = value - chroma;

	const float high = chroma + base;
	const float mid = second + base;

	switch (static_cast<int>(sector) % 6) {
		case 0:
			return { high, mid, base, 1.f };
		case 1:
			return { mid, high, base, 1.f };
		case 2:
			return { base, high, mid, 1.f };
		case 3:
			return { base, mid, high, 1.f };
		case 4:
			return { mid, base, high, 1.f };
		default:
			return { high, base, mid, 1.f };
	}
}

auto gse::ide::project::resolve_accent(const std::string_view text, const std::string_view name) -> vec4f {
	if (text.size() == 7 && text.front() == '#') {
		int channels[3]{};
		bool parsed = true;
		for (const auto& [index, channel] : std::views::enumerate(channels)) {
			const auto result = std::from_chars(text.data() + 1 + index * 2, text.data() + 3 + index * 2, channel, 16);
			parsed = parsed && result.ec == std::errc{};
		}
		if (parsed) {
			return { static_cast<float>(channels[0]) / 255.f, static_cast<float>(channels[1]) / 255.f, static_cast<float>(channels[2]) / 255.f, 1.f };
		}
	}

	return hue_color(static_cast<float>(std::hash<std::string_view>{}(name) % 360u));
}

auto gse::ide::project::read_file(const std::filesystem::path& path) -> std::string {
	std::ifstream stream(path, std::ios::binary);
	if (!stream) {
		return {};
	}
	return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}

auto gse::ide::project::command_line_manifest() -> std::filesystem::path {
	int count = 0;
	wchar_t** arguments = win32::CommandLineToArgvW(win32::GetCommandLineW(), &count);
	if (arguments == nullptr) {
		return {};
	}

	std::filesystem::path found;
	for (int index = 1; index < count && found.empty(); ++index) {
		if (std::filesystem::path candidate(arguments[index]); candidate.extension() == manifest_extension) {
			found = std::move(candidate);
		}
	}

	win32::LocalFree(arguments);
	return found;
}

auto gse::ide::project::write_file(const std::filesystem::path& path, const std::string_view text) -> bool {
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream) {
		return false;
	}
	stream.write(text.data(), static_cast<std::streamsize>(text.size()));
	return stream.good();
}

auto gse::ide::project::templates() -> std::span<const project_template> {
	static constexpr std::array<project_template, 2> entries{
		project_template{
			.id = "blank",
			.label = "Blank",
			.detail = "Empty game loop with the 3D world running.",
		},
		project_template{
			.id = "tool",
			.label = "Tool",
			.detail = "2D tool: world simulation and rendering off, one starter system.",
		},
	};
	return entries;
}

auto gse::ide::project::module_namespace(const std::string_view name) -> std::string {
	std::string out;
	for (const char c : name) {
		if (std::isalnum(static_cast<unsigned char>(c))) {
			out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
		}
	}
	if (out.empty() || std::isdigit(static_cast<unsigned char>(out.front()))) {
		out.insert(out.begin(), 'p');
	}
	return out;
}

auto gse::ide::project::validate_new(const std::string_view name, const std::filesystem::path& parent) -> std::string {
	if (name.empty()) {
		return "Enter a project name.";
	}
	if (name.find_first_of("\\/:*?\"<>|") != std::string_view::npos) {
		return "Name cannot contain \\ / : * ? \" < > |";
	}
	if (name.front() == ' ' || name.back() == ' ' || name.back() == '.') {
		return "Name cannot start or end with a space, or end with a period.";
	}

	const std::filesystem::path destination = parent / name;

	if (destination.generic_display_string().size() > max_project_path) {
		return std::format("Path is too long ({} chars); keep it under {}.", destination.generic_display_string().size(), max_project_path);
	}

	std::error_code ec;
	if (std::filesystem::exists(destination, ec)) {
		return "A folder with that name already exists.";
	}
	return {};
}

auto gse::ide::project::create(const std::string_view name, const std::filesystem::path& parent, const std::string_view template_id) -> std::expected<std::filesystem::path, std::string> {
	if (const std::string problem = validate_new(name, parent); !problem.empty()) {
		return std::unexpected(problem);
	}

	const std::filesystem::path destination = parent / name;
	const std::string project(name);

	std::error_code ec;
	std::filesystem::create_directories(destination / "Source", ec);
	std::filesystem::create_directories(destination / "Assets", ec);
	std::filesystem::create_directories(destination / "Art", ec);
	if (ec) {
		return std::unexpected(std::format("Could not create {}", destination.generic_display_string()));
	}

	const std::string cmake = std::format(R"(cmake_minimum_required(VERSION 4.2)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set by the editor when it configures this project, from the .gseproj [engine] section.
if(NOT DEFINED GSE_ENGINE_DIR)
    set(GSE_ENGINE_DIR "$ENV{{GSE_ENGINE_DIR}}")
endif()
if(NOT GSE_ENGINE_DIR)
    message(FATAL_ERROR "GSE_ENGINE_DIR is not set. Pass -DGSE_ENGINE_DIR=<path to the engine tree>.")
endif()
if(NOT EXISTS "${{GSE_ENGINE_DIR}}/Engine/cmake/GSEEngine.cmake")
    message(FATAL_ERROR "GSE_ENGINE_DIR does not point at an engine tree:\n  ${{GSE_ENGINE_DIR}}\nThe engine it names has moved or been deleted. Fix the [engine] section of this project's .gseproj.")
endif()

include("${{GSE_ENGINE_DIR}}/Engine/cmake/GSEEngine.cmake")

project({0})

gse_configure_compiler()

add_subdirectory("${{GSE_ENGINE_ROOT}}/Engine" "${{CMAKE_BINARY_DIR}}/Engine")

gse_write_manifest()

file(GLOB_RECURSE {0}_MODULES CONFIGURE_DEPENDS "${{CMAKE_CURRENT_SOURCE_DIR}}/Source/*.cppm")
file(GLOB_RECURSE {0}_IMPLS CONFIGURE_DEPENDS "${{CMAKE_CURRENT_SOURCE_DIR}}/Source/*.cpp")

# A .cpp declaring an implementation partition (module x:y;) provides a module and
# must live in a CXX_MODULES file set; plain implementation units stay ordinary sources.
set({0}_MODULE_IMPLS "")
set({0}_PLAIN_IMPLS "")
foreach(_impl IN LISTS {0}_IMPLS)
    file(STRINGS "${{_impl}}" _part_decl REGEX "^module [A-Za-z_0-9.]+:[A-Za-z_0-9]+ *;" LIMIT_COUNT 1)
    if(_part_decl)
        list(APPEND {0}_MODULE_IMPLS "${{_impl}}")
    else()
        list(APPEND {0}_PLAIN_IMPLS "${{_impl}}")
    endif()
endforeach()

add_executable({0} ${{{0}_PLAIN_IMPLS}})

if({0}_MODULES OR {0}_MODULE_IMPLS)
    target_sources({0}
        PRIVATE
            FILE_SET gse_project_modules TYPE CXX_MODULES
            BASE_DIRS "${{CMAKE_CURRENT_SOURCE_DIR}}/Source"
            FILES ${{{0}_MODULES}} ${{{0}_MODULE_IMPLS}}
    )
endif()

target_link_libraries({0} PRIVATE Engine)

# GCC-trunk's C++ modules codegen on MinGW emits vague-linkage function-local statics as
# strong symbols in every importing TU, so the final link sees duplicate definitions.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
    target_link_options({0} PRIVATE "-Wl,--allow-multiple-definition")
endif()

gse_copy_runtime_deps({0})
)", project);

	const std::string engine_name = ensure_engine_registered(config::root_dir());
	const std::string engine_block = engine_name.empty()
		? std::format("source = {}\n", config::root_dir().generic_display_string())
		: std::format("name = {}\nsource = {}\n", engine_name, config::root_dir().generic_display_string());

	const std::string manifest_text = std::format(R"([project]
name = {0}
engine_version = 0.1.0
source = Source
assets = Assets
art = Art

[engine]
{1}
[targets]
game = {0}
)", project, engine_block);

	const bool tool = template_id == "tool";
	const std::string ns = module_namespace(project);

	const std::string main_text = tool
		? std::format(R"(import std;

import gse;
import gse.system_manifest;

import {1}.app;

auto main() -> int {{
	gse::start(
		[](gse::engine& e) -> void {{
			gse::register_systems<^^{1}::app>(e);
		}},
		{{
			.title = "{0}",
			.render_world = false,
			.simulate_world = false,
		}}
	);
	return 0;
}}
)", project, ns)
		: std::format(R"(import std;

import gse;

auto main() -> int {{
	gse::start(
		[](gse::engine& e) -> void {{
		}},
		{{
			.title = "{0}",
		}}
	);
	return 0;
}}
)", project);

	const std::string app_module = tool
		? std::format(R"(export module {1}.app;

import std;
import gse;

export namespace {1}::app {{
	struct [[= gse::system_state<"{0}">{{}}]] data {{
		bool ready = false;
	}};

	[[= gse::system_run<>{{}}]]
	auto run(
		gse::context& ctx,
		data& d
	) -> gse::async::task<>;
}}

auto {1}::app::run(gse::context&, data& d) -> gse::async::task<> {{
	d.ready = true;
	co_return;
}}
)", project, ns)
		: std::string{};

	const std::filesystem::path manifest = destination / (project + std::string(manifest_extension));

	const std::string art_readme = R"(# Art

Authoring output that engine tools consume: skinned meshes and skeletons
exported from a DCC tool, before they are turned into engine assets.

Nothing here is loaded at runtime. This directory sits outside Assets/ on
purpose -- the asset compiler scans Assets/ recursively, so an intermediate
file left in there is scanned, fails its version check, and reports nothing.

    Art/  ->  Tools/RigDerive  ->  Assets/
)";

	if (!write_file(destination / "CMakeLists.txt", cmake)
		|| !write_file(manifest, manifest_text)
		|| !write_file(destination / "Source" / "Main.cpp", main_text)
		|| !write_file(destination / "Art" / "README.md", art_readme)
		|| !write_file(destination / ".gitignore", ".gse/\n")) {
		return std::unexpected(std::format("Could not write project files under {}", destination.generic_display_string()));
	}

	if (!app_module.empty() && !write_file(destination / "Source" / "App.cppm", app_module)) {
		return std::unexpected(std::format("Could not write project files under {}", destination.generic_display_string()));
	}

	return config::generic(manifest);
}

auto gse::ide::project::recent_path() -> std::filesystem::path {
	return config::user_config_dir() / "recent_projects.ini";
}

auto gse::ide::project::engines_path() -> std::filesystem::path {
	return config::user_config_dir() / "engines.ini";
}

auto gse::ide::project::is_engine_tree(const std::filesystem::path& path) -> bool {
	if (path.empty()) {
		return false;
	}
	std::error_code ec;
	return std::filesystem::exists(path / engine_marker, ec);
}

auto gse::ide::project::engines() -> std::vector<engine_entry> {
	std::vector<engine_entry> entries;
	for (const layout_store::section& section : layout_store::parse_sections(layout_store::read(engines_path()))) {
		if (section.name != "engines") {
			continue;
		}
		for (const auto& [name, value] : section.values) {
			if (!name.empty() && !value.empty()) {
				entries.push_back({ .name = name, .path = config::generic(value) });
			}
		}
	}
	return entries;
}

auto gse::ide::project::write_engines(const std::vector<engine_entry>& entries) -> void {
	std::string block = "[engines]\n";
	for (const engine_entry& entry : entries) {
		block += std::format("{} = {}\n", entry.name, entry.path.generic_native_encoded_string());
	}
	layout_store::submit(engines_path(), { .names = { "engines" } }, std::move(block));
}

auto gse::ide::project::engine_for_name(const std::string_view name) -> std::filesystem::path {
	for (const engine_entry& entry : engines()) {
		if (entry.name == name) {
			return entry.path;
		}
	}
	return {};
}

auto gse::ide::project::ensure_engine_registered(const std::filesystem::path& path) -> std::string {
	if (!is_engine_tree(path)) {
		return {};
	}

	const std::filesystem::path canonical = config::generic(path);
	std::vector<engine_entry> entries = engines();
	for (const engine_entry& entry : entries) {
		if (entry.path == canonical) {
			return entry.name;
		}
	}

	std::string base = canonical.filename().native_encoded_string();
	if (base.empty()) {
		base = "engine";
	}

	std::string name = base;
	for (int suffix = 2; std::ranges::any_of(entries, [&name](const engine_entry& entry) { return entry.name == name; }); ++suffix) {
		name = std::format("{}-{}", base, suffix);
	}

	entries.push_back({ .name = name, .path = canonical });
	write_engines(entries);
	return name;
}

auto gse::ide::project::bind_engine(const std::filesystem::path& manifest_file, const engine_entry& engine) -> void {
	layout_store::submit(
		manifest_file,
		{ .names = { "engine" } },
		std::format("[engine]\nname = {}\nsource = {}\n", engine.name, engine.path.generic_native_encoded_string())
	);
	layout_store::flush();
}

auto gse::ide::project::recent() -> std::vector<std::filesystem::path> {
	std::vector<std::filesystem::path> entries;
	for (const layout_store::section& section : layout_store::parse_sections(read_file(recent_path()))) {
		if (section.name != "recent") {
			continue;
		}
		for (const std::string& value : std::views::values(section.values)) {
			if (!value.empty()) {
				entries.emplace_back(value);
			}
		}
	}
	return entries;
}

auto gse::ide::project::recent_manifest() -> std::filesystem::path {
	std::error_code ec;
	for (const std::filesystem::path& entry : recent()) {
		if (std::filesystem::exists(entry, ec)) {
			return entry;
		}
	}
	return {};
}

auto gse::ide::project::record_recent() -> void {
	const manifest& active = current();
	if (!active.valid) {
		return;
	}

	std::vector<std::filesystem::path> entries = recent();
	std::erase(entries, active.file);
	entries.insert(entries.begin(), active.file);
	if (entries.size() > recent_limit) {
		entries.resize(recent_limit);
	}

	std::string block = "[recent]\n";
	for (const auto& [index, entry] : std::views::enumerate(entries)) {
		block += std::format("{:02} = {}\n", index, entry.generic_native_encoded_string());
	}

	layout_store::submit(recent_path(), { .names = { "recent" } }, std::move(block));
}

auto gse::ide::project::discover() -> std::vector<std::filesystem::path> {
	std::error_code ec;
	std::vector<std::filesystem::path> found;
	for (const auto& entry : std::filesystem::directory_iterator(config::root_dir(), std::filesystem::directory_options::skip_permission_denied, ec)) {
		if (!entry.is_directory(ec)) {
			continue;
		}
		std::error_code child_ec;
		for (const auto& child : std::filesystem::directory_iterator(entry.path(), std::filesystem::directory_options::skip_permission_denied, child_ec)) {
			if (child.path().extension() == manifest_extension) {
				found.push_back(config::generic(child.path()));
			}
		}
	}

	std::ranges::sort(found);
	return found;
}

auto gse::ide::project::known() -> std::vector<std::filesystem::path> {
	std::vector<std::filesystem::path> entries = recent();
	for (const std::filesystem::path& candidate : discover()) {
		if (!std::ranges::contains(entries, candidate)) {
			entries.push_back(candidate);
		}
	}

	std::error_code ec;
	std::erase_if(entries, [&ec](const std::filesystem::path& entry) {
		return !std::filesystem::exists(entry, ec);
	});
	return entries;
}

auto gse::ide::project::dev_default_manifest() -> std::filesystem::path {
	if (config::mode() != config::run_mode::dev) {
		return {};
	}

	const std::vector<std::filesystem::path> found = discover();
	return found.empty() ? std::filesystem::path{} : found.front();
}

auto gse::ide::project::load(const std::filesystem::path& file) -> manifest {
	std::error_code ec;
	if (file.empty() || !std::filesystem::exists(file, ec)) {
		return {};
	}

	manifest out = {
		.valid = true,
		.file = config::generic(file),
		.root = config::generic(file.parent_path())
	};

	std::string source_leaf = "Source";
	std::string assets_leaf = "Assets";
	std::string art_leaf = "Art";
	std::string accent_text;

	for (const layout_store::section& section : layout_store::parse_sections(read_file(file))) {
		if (section.name == "project") {
			if (const auto entry = section.values.find("name"); entry != section.values.end()) {
				out.name = entry->second;
			}
			if (const auto entry = section.values.find("engine_version"); entry != section.values.end()) {
				out.engine_version = entry->second;
			}
			if (const auto entry = section.values.find("source"); entry != section.values.end()) {
				source_leaf = entry->second;
			}
			if (const auto entry = section.values.find("art"); entry != section.values.end()) {
				art_leaf = entry->second;
			}
			if (const auto entry = section.values.find("assets"); entry != section.values.end()) {
				assets_leaf = entry->second;
			}
			if (const auto entry = section.values.find("accent"); entry != section.values.end()) {
				accent_text = entry->second;
			}
		}
		else if (section.name == "engine") {
			if (const auto entry = section.values.find("name"); entry != section.values.end()) {
				out.engine_name = entry->second;
			}
			if (const auto entry = section.values.find("source"); entry != section.values.end()) {
				const std::filesystem::path declared(entry->second);
				out.engine = declared.is_absolute() ? declared : out.root / declared;
			}
		}
		else if (section.name == "targets") {
			out.targets = section.values;
		}
	}

	std::vector<std::filesystem::path> candidates;
	if (!out.engine_name.empty()) {
		if (std::filesystem::path bound = engine_for_name(out.engine_name); !bound.empty()) {
			candidates.push_back(std::move(bound));
		}
	}
	if (!out.engine.empty()) {
		std::error_code engine_ec;
		candidates.push_back(config::generic(std::filesystem::weakly_canonical(out.engine, engine_ec)));
	}

	out.engine.clear();
	for (const std::filesystem::path& candidate : candidates) {
		if (is_engine_tree(candidate)) {
			out.engine = candidate;
			break;
		}
	}

	if (out.engine.empty()) {
		if (!candidates.empty()) {
			out.engine_problem = std::format("no engine tree at {} ({} missing)", candidates.front().generic_display_string(), engine_marker);
		}
		else if (!out.engine_name.empty()) {
			out.engine_problem = std::format("engine '{}' is not registered on this machine", out.engine_name);
		}
	}

	out.source = config::generic(out.root / source_leaf);
	out.assets = config::generic(out.root / assets_leaf);
	out.art = config::generic(out.root / art_leaf);
	out.state = config::generic(out.root / ".gse");

	if (out.name.empty()) {
		out.name = out.root.filename().native_encoded_string();
	}
	out.accent = resolve_accent(accent_text, out.name);
	return out;
}

auto gse::ide::project::accent() -> vec4f {
	const manifest& active = current();
	return active.valid ? active.accent : vec4f{ 0.26f, 0.86f, 0.84f, 1.f };
}

auto gse::ide::project::resolve() -> manifest {
	ensure_engine_registered(config::root_dir());

	const std::array<std::filesystem::path, 3> candidates{ command_line_manifest(), recent_manifest(), dev_default_manifest() };

	for (const auto& [index, candidate] : std::views::enumerate(candidates)) {
		if (manifest found = load(candidate); found.valid) {
			found.requested = index == 0;
			log::println(log::level::info, log::category::general, "[project] opened '{}' at {}", found.name, found.root.generic_display_string());
			if (!found.engine_problem.empty()) {
				log::println(log::level::error, log::category::general, "[project] engine binding failed: {} - falling back to the editor's own engine; fix the [engine] section of {}", found.engine_problem, found.file.generic_display_string());
			}
			return found;
		}
	}

	log::println(log::level::warning, log::category::general, "[project] no .gseproj resolved; falling back to repository roots");
	return {};
}

auto gse::ide::project::current() -> const manifest& {
	static const manifest value = resolve();
	return value;
}

auto gse::ide::project::target(const std::string_view key, const std::string_view fallback) -> std::string {
	const manifest& active = current();
	if (const auto entry = active.targets.find(std::string(key)); entry != active.targets.end()) {
		return entry->second;
	}
	return std::string(fallback);
}
