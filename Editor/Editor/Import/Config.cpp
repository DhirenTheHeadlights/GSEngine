module gse.ide.config;

import std;

import gse.config;
import gse.ide.project;

namespace gse::ide::config {
	struct resolved {
		std::string game_target;
		std::filesystem::path source;
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
		std::filesystem::path resources;
		std::filesystem::path build;
		std::filesystem::path token_plugin;
		std::filesystem::path cppref_index;
		std::filesystem::path game_executable;
		std::filesystem::path editor_executable;
		std::filesystem::path editor_layout;
		std::filesystem::path editor_layout_default;
	};

	auto is_inside(
		const std::filesystem::path& child,
		const std::filesystem::path& parent
	) -> bool;

	auto resolve() -> resolved;

	auto table() -> const resolved&;

	auto resolve_browse_roots() -> std::vector<browse_root>;

	auto user_data_roots() -> std::vector<browse_root>;
}

auto gse::ide::config::is_inside(const std::filesystem::path& child, const std::filesystem::path& parent) -> bool {
	const std::filesystem::path relative = child.lexically_relative(parent);
	return !relative.empty() && *relative.begin() != "..";
}

auto gse::ide::config::resolve() -> resolved {
	const std::filesystem::path& root = gse::config::root_dir();
	const std::filesystem::path& build = gse::config::build_root();
	const std::filesystem::path resources = root / "Editor" / "Resources";

	const project::manifest& active = project::current();
	const std::filesystem::path engine_root = active.valid && !active.engine.empty() ? active.engine : root;
	const std::filesystem::path project_root = active.valid ? active.root : root;
	const std::filesystem::path project_source = active.valid ? active.source : root / "Game" / "Game";
	const std::filesystem::path project_assets = active.valid ? active.assets : gse::config::project_assets_path();
	const std::filesystem::path project_state = active.valid ? active.state : gse::config::user_config_dir();

	const bool nested = is_inside(project_root, engine_root);
	const std::filesystem::path project_build = nested ? build : project_state / "build" / build.filename();
	const std::filesystem::path project_output = nested ? build / project_root.lexically_relative(engine_root) : project_build;

	const std::string game = project::target("game", active.name);

	return {
		.game_target = game,
		.source = gse::config::generic(root / "Editor" / "Editor"),
		.engine_root = gse::config::generic(engine_root),
		.engine_source = gse::config::generic(engine_root / "Engine" / "Engine"),
		.project_root = gse::config::generic(project_root),
		.project_source = gse::config::generic(project_source),
		.project_assets = gse::config::generic(project_assets),
		.project_state = gse::config::generic(project_state),
		.project_settings = active.valid ? gse::config::generic(project_root / "Config" / "settings.ini") : std::filesystem::path{},
		.project_build = gse::config::generic(project_build),
		.project_output = gse::config::generic(project_output),
		.project_compile_commands = gse::config::generic(project_build / "compile_commands.json"),
		.resources = gse::config::generic(resources),
		.build = build,
		.token_plugin = gse::config::generic(build / "Editor" / "gse_tokens.dll"),
		.cppref_index = gse::config::generic(resources / "cppref.idx"),
		.game_executable = gse::config::generic(project_output / (game + ".exe")),
		.editor_executable = gse::config::generic(build / "Editor" / (std::string(editor_target) + ".exe")),
		.editor_layout = gse::config::generic((active.valid ? active.state : gse::config::user_config_dir()) / "editor_layout.ini"),
		.editor_layout_default = gse::config::generic(gse::config::user_config_dir() / "editor_layout.ini")
	};
}

auto gse::ide::config::table() -> const resolved& {
	static const resolved value = resolve();
	return value;
}

auto gse::ide::config::user_data_roots() -> std::vector<browse_root> {
	const std::filesystem::path editor_compile_commands = gse::config::generic(build_dir() / "compile_commands.json");
	return {
		{
			.path = gse::config::user_config_dir(),
			.name = "User Config",
			.compile_commands = editor_compile_commands,
			.is_project = false,
			.analyzable = false
		},
		{
			.path = gse::config::user_state_dir(),
			.name = "User Data",
			.compile_commands = editor_compile_commands,
			.is_project = false,
			.analyzable = false
		}
	};
}

auto gse::ide::config::resolve_browse_roots() -> std::vector<browse_root> {
	const project::manifest& active = project::current();
	const std::filesystem::path editor_compile_commands = gse::config::generic(build_dir() / "compile_commands.json");

	std::vector<browse_root> roots;
	if (!active.valid) {
		roots.push_back({
			.path = gse::config::root_dir(),
			.name = gse::config::root_dir().filename().native_encoded_string(),
			.compile_commands = editor_compile_commands,
			.is_project = false
		});
	}
	else {
		roots.push_back({
			.path = project_root(),
			.name = active.name,
			.compile_commands = project_compile_commands(),
			.is_project = true
		});
		roots.push_back({
			.path = gse::config::generic(engine_root() / "Engine"),
			.name = "Engine",
			.compile_commands = editor_compile_commands,
			.is_project = false
		});
		roots.push_back({
			.path = gse::config::generic(gse::config::root_dir() / "Editor"),
			.name = "Editor",
			.compile_commands = editor_compile_commands,
			.is_project = false
		});
	}

	std::ranges::move(user_data_roots(), std::back_inserter(roots));
	return roots;
}

auto gse::ide::config::browse_roots() -> std::span<const browse_root> {
	static const std::vector<browse_root> value = resolve_browse_roots();
	return value;
}

auto gse::ide::config::analysis_roots() -> std::vector<std::filesystem::path> {
	const std::span<const browse_root> roots = browse_roots();
	std::vector<std::filesystem::path> out;
	out.reserve(roots.size());
	std::ranges::copy(
		roots | std::views::filter(&browse_root::analyzable) | std::views::transform(&browse_root::path),
		std::back_inserter(out)
	);
	return out;
}

auto gse::ide::config::source_dir() -> const std::filesystem::path& {
	return table().source;
}

auto gse::ide::config::engine_root() -> const std::filesystem::path& {
	return table().engine_root;
}

auto gse::ide::config::engine_source_dir() -> const std::filesystem::path& {
	return table().engine_source;
}

auto gse::ide::config::project_root() -> const std::filesystem::path& {
	return table().project_root;
}

auto gse::ide::config::project_source_dir() -> const std::filesystem::path& {
	return table().project_source;
}

auto gse::ide::config::project_assets_dir() -> const std::filesystem::path& {
	return table().project_assets;
}

auto gse::ide::config::project_state_dir() -> const std::filesystem::path& {
	return table().project_state;
}

auto gse::ide::config::project_settings() -> const std::filesystem::path& {
	return table().project_settings;
}

auto gse::ide::config::resource_path() -> const std::filesystem::path& {
	return table().resources;
}

auto gse::ide::config::game_target() -> std::string_view {
	return table().game_target;
}

auto gse::ide::config::project_build_dir() -> const std::filesystem::path& {
	return table().project_build;
}

auto gse::ide::config::project_output_dir() -> const std::filesystem::path& {
	return table().project_output;
}

auto gse::ide::config::project_compile_commands() -> const std::filesystem::path& {
	return table().project_compile_commands;
}

auto gse::ide::config::build_dir() -> const std::filesystem::path& {
	return table().build;
}

auto gse::ide::config::token_plugin() -> const std::filesystem::path& {
	return table().token_plugin;
}

auto gse::ide::config::compile_commands_for(const std::filesystem::path& file) -> const std::filesystem::path& {
	const std::span<const browse_root> roots = browse_roots();
	const auto owner = std::ranges::find_if(roots, [&file](const browse_root& root) {
		return is_inside(file, root.path);
	});
	return owner != roots.end() ? owner->compile_commands : project_compile_commands();
}

auto gse::ide::config::cppref_index() -> const std::filesystem::path& {
	return table().cppref_index;
}

auto gse::ide::config::game_executable() -> const std::filesystem::path& {
	return table().game_executable;
}

auto gse::ide::config::editor_executable() -> const std::filesystem::path& {
	return table().editor_executable;
}

auto gse::ide::config::editor_layout() -> const std::filesystem::path& {
	return table().editor_layout;
}

auto gse::ide::config::editor_layout_default() -> const std::filesystem::path& {
	return table().editor_layout_default;
}

auto gse::ide::config::seed_editor_layout() -> void {
	const std::filesystem::path& active = editor_layout();
	const std::filesystem::path& fallback = editor_layout_default();
	if (active == fallback) {
		return;
	}

	std::error_code ec;
	if (std::filesystem::exists(active, ec) || !std::filesystem::exists(fallback, ec)) {
		return;
	}

	std::filesystem::create_directories(active.parent_path(), ec);
	std::filesystem::copy_file(fallback, active, ec);
}
