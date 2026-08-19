module gse.ide.config;

import std;

import gse.config;
import gse.ide.project;

namespace gse::ide::config {
	struct editor_paths {
		std::filesystem::path source;
		std::filesystem::path resources;
		std::filesystem::path build;
		std::filesystem::path token_plugin;
		std::filesystem::path cppref_index;
		std::filesystem::path executable;
		std::filesystem::path layout;
		std::filesystem::path layout_default;
	};

	auto is_inside(
		const std::filesystem::path& child,
		const std::filesystem::path& parent
	) -> bool;

	auto editor_compile_commands() -> std::filesystem::path;

	auto manifest_target(
		const project::manifest& active,
		std::string_view key,
		std::string_view fallback
	) -> std::string;

	auto resolve_primary_worktree() -> worktree;

	auto read_git_link(
		const std::filesystem::path& file
	) -> std::filesystem::path;

	auto git_worktree_roots(
		const std::filesystem::path& root
	) -> std::vector<std::filesystem::path>;

	auto retarget(
		const std::filesystem::path& path,
		const std::filesystem::path& from,
		const std::filesystem::path& to
	) -> std::filesystem::path;

	auto retarget_worktree(
		const worktree& source,
		const std::filesystem::path& root
	) -> worktree;

	auto resolve_worktrees() -> std::vector<worktree>;

	auto append_worktree_roots(
		const worktree& tree,
		std::vector<browse_root>& out
	) -> void;

	auto append_fixed_roots(
		const worktree& tree,
		std::vector<browse_root>& out
	) -> void;

	auto resolve_browse_roots() -> std::vector<browse_root>;

	auto resolve_editor() -> editor_paths;

	auto editor() -> const editor_paths&;
}

auto gse::ide::config::is_inside(const std::filesystem::path& child, const std::filesystem::path& parent) -> bool {
	const std::filesystem::path relative = child.lexically_relative(parent);
	return !relative.empty() && *relative.begin() != "..";
}

auto gse::ide::config::editor_compile_commands() -> std::filesystem::path {
	return gse::config::generic(gse::config::build_root() / "compile_commands.json");
}

auto gse::ide::config::manifest_target(const project::manifest& active, const std::string_view key, const std::string_view fallback) -> std::string {
	if (const auto entry = active.targets.find(std::string(key)); entry != active.targets.end()) {
		return entry->second;
	}
	return std::string(fallback);
}

auto gse::ide::config::resolve_primary_worktree() -> worktree {
	const std::filesystem::path& root = gse::config::root_dir();
	const std::filesystem::path& build = gse::config::build_root();

	const project::manifest& active = project::current();
	const std::filesystem::path engine_root = active.valid && !active.engine.empty() ? active.engine : root;
	const std::filesystem::path project_root = active.valid ? active.root : root;
	const std::filesystem::path project_source = active.valid ? active.source : root / "Game" / "Game";
	const std::filesystem::path project_assets = active.valid ? active.assets : gse::config::project_assets_path();
	const std::filesystem::path project_state = active.valid ? active.state : gse::config::user_config_dir();

	const bool nested = is_inside(project_root, engine_root);
	const std::filesystem::path project_build = nested ? build : project_state / "build" / build.filename();
	const std::filesystem::path project_output = nested ? build / project_root.lexically_relative(engine_root) : project_build;

	const std::string game = manifest_target(active, "game", active.name);

	return {
		.name = active.valid ? active.name : root.filename().native_encoded_string(),
		.game_target = game,
		.has_manifest = active.valid,
		.engine_root = gse::config::generic(engine_root),
		.engine_source = gse::config::generic(engine_root / "Engine" / "Engine"),
		.project_root = gse::config::generic(project_root),
		.project_source = gse::config::generic(project_source),
		.project_assets = gse::config::generic(project_assets),
		.project_state = gse::config::generic(project_state),
		.project_settings = active.valid ? gse::config::project_settings_path_for(project_root) : std::filesystem::path{},
		.project_build = gse::config::generic(project_build),
		.project_output = gse::config::generic(project_output),
		.project_compile_commands = gse::config::generic(project_build / "compile_commands.json"),
		.game_executable = gse::config::generic(project_output / (game + ".exe"))
	};
}

auto gse::ide::config::read_git_link(const std::filesystem::path& file) -> std::filesystem::path {
	std::ifstream in(file);
	std::string line;
	if (!std::getline(in, line)) {
		return {};
	}

	constexpr std::string_view prefix = "gitdir: ";
	if (line.starts_with(prefix)) {
		line.erase(0, prefix.size());
	}
	while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
		line.pop_back();
	}
	return line.empty() ? std::filesystem::path{} : std::filesystem::path(line);
}

auto gse::ide::config::git_worktree_roots(const std::filesystem::path& root) -> std::vector<std::filesystem::path> {
	std::error_code ec;
	const std::filesystem::path marker = root / ".git";
	std::filesystem::path git_dir = marker;
	if (std::filesystem::is_regular_file(marker, ec)) {
		git_dir = read_git_link(marker).parent_path().parent_path();
	}

	std::vector<std::filesystem::path> roots;
	if (git_dir.empty()) {
		return roots;
	}

	for (const auto& entry : std::filesystem::directory_iterator(git_dir / "worktrees", std::filesystem::directory_options::skip_permission_denied, ec)) {
		const std::filesystem::path tree = read_git_link(entry.path() / "gitdir").parent_path();
		if (!tree.empty() && tree != root && std::filesystem::is_directory(tree, ec)) {
			roots.push_back(gse::config::generic(tree));
		}
	}
	return roots;
}

auto gse::ide::config::retarget(const std::filesystem::path& path, const std::filesystem::path& from, const std::filesystem::path& to) -> std::filesystem::path {
	if (path.empty() || !is_inside(path, from)) {
		return path;
	}
	return gse::config::generic(to / path.lexically_relative(from));
}

auto gse::ide::config::retarget_worktree(const worktree& source, const std::filesystem::path& root) -> worktree {
	const std::filesystem::path& from = source.engine_root;
	return {
		.name = root.filename().native_encoded_string(),
		.game_target = source.game_target,
		.has_manifest = source.has_manifest,
		.engine_root = gse::config::generic(root),
		.engine_source = retarget(source.engine_source, from, root),
		.project_root = retarget(source.project_root, from, root),
		.project_source = retarget(source.project_source, from, root),
		.project_assets = retarget(source.project_assets, from, root),
		.project_state = retarget(source.project_state, from, root),
		.project_settings = retarget(source.project_settings, from, root),
		.project_build = retarget(source.project_build, from, root),
		.project_output = retarget(source.project_output, from, root),
		.project_compile_commands = retarget(source.project_compile_commands, from, root),
		.game_executable = retarget(source.game_executable, from, root)
	};
}

auto gse::ide::config::resolve_worktrees() -> std::vector<worktree> {
	const worktree base = resolve_primary_worktree();

	std::vector<worktree> trees;
	trees.push_back(base);
	for (const std::filesystem::path& root : git_worktree_roots(base.engine_root)) {
		trees.push_back(retarget_worktree(base, root));
	}
	return trees;
}

auto gse::ide::config::worktrees() -> std::span<const worktree> {
	static const std::vector<worktree> value = resolve_worktrees();
	return value;
}

auto gse::ide::config::primary() -> const worktree& {
	return worktrees().front();
}

auto gse::ide::config::worktree_for(const std::filesystem::path& file) -> const worktree& {
	const std::span<const worktree> trees = worktrees();
	const auto owner = std::ranges::find_if(trees, [&file](const worktree& tree) {
		return is_inside(file, tree.project_root) || is_inside(file, tree.engine_root);
	});
	return owner != trees.end() ? *owner : primary();
}

auto gse::ide::config::append_worktree_roots(const worktree& tree, std::vector<browse_root>& out) -> void {
	if (!tree.has_manifest) {
		out.push_back({
			.path = tree.project_root,
			.name = tree.name,
			.compile_commands = editor_compile_commands(),
			.is_project = false
		});
		return;
	}

	out.push_back({
		.path = tree.project_root,
		.name = tree.name,
		.compile_commands = tree.project_compile_commands,
		.is_project = true
	});
	out.push_back({
		.path = gse::config::generic(tree.engine_root / "Engine"),
		.name = "Engine",
		.compile_commands = editor_compile_commands(),
		.is_project = false
	});
}

auto gse::ide::config::append_fixed_roots(const worktree& tree, std::vector<browse_root>& out) -> void {
	if (tree.has_manifest) {
		out.push_back({
			.path = gse::config::generic(gse::config::root_dir() / "Editor"),
			.name = "Editor",
			.compile_commands = editor_compile_commands(),
			.is_project = false
		});
	}

	out.push_back({
		.path = gse::config::user_config_dir(),
		.name = "User Config",
		.compile_commands = editor_compile_commands(),
		.is_project = false,
		.analyzable = false
	});
	out.push_back({
		.path = gse::config::user_state_dir(),
		.name = "User Data",
		.compile_commands = editor_compile_commands(),
		.is_project = false,
		.analyzable = false
	});
}

auto gse::ide::config::resolve_browse_roots() -> std::vector<browse_root> {
	std::vector<browse_root> roots;
	for (const worktree& tree : worktrees()) {
		append_worktree_roots(tree, roots);
	}
	append_fixed_roots(primary(), roots);
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

auto gse::ide::config::compile_commands_for(const std::filesystem::path& file) -> const std::filesystem::path& {
	const std::span<const browse_root> roots = browse_roots();
	const auto owner = std::ranges::find_if(roots, [&file](const browse_root& root) {
		return is_inside(file, root.path);
	});
	return owner != roots.end() ? owner->compile_commands : project_compile_commands();
}

auto gse::ide::config::resolve_editor() -> editor_paths {
	const std::filesystem::path& root = gse::config::root_dir();
	const std::filesystem::path& build = gse::config::build_root();
	const std::filesystem::path resources = root / "Editor" / "Resources";

	return {
		.source = gse::config::generic(root / "Editor" / "Editor"),
		.resources = gse::config::generic(resources),
		.build = build,
		.token_plugin = gse::config::generic(build / "Editor" / "gse_tokens.dll"),
		.cppref_index = gse::config::generic(resources / "cppref.idx"),
		.executable = gse::config::generic(build / "Editor" / (std::string(editor_target) + ".exe")),
		.layout = gse::config::generic(primary().project_state / "editor_layout.ini"),
		.layout_default = gse::config::generic(gse::config::user_config_dir() / "editor_layout.ini")
	};
}

auto gse::ide::config::editor() -> const editor_paths& {
	static const editor_paths value = resolve_editor();
	return value;
}

auto gse::ide::config::game_target() -> std::string_view {
	return primary().game_target;
}

auto gse::ide::config::engine_root() -> const std::filesystem::path& {
	return primary().engine_root;
}

auto gse::ide::config::engine_source_dir() -> const std::filesystem::path& {
	return primary().engine_source;
}

auto gse::ide::config::project_root() -> const std::filesystem::path& {
	return primary().project_root;
}

auto gse::ide::config::project_source_dir() -> const std::filesystem::path& {
	return primary().project_source;
}

auto gse::ide::config::project_assets_dir() -> const std::filesystem::path& {
	return primary().project_assets;
}

auto gse::ide::config::project_state_dir() -> const std::filesystem::path& {
	return primary().project_state;
}

auto gse::ide::config::project_settings() -> const std::filesystem::path& {
	return primary().project_settings;
}

auto gse::ide::config::project_build_dir() -> const std::filesystem::path& {
	return primary().project_build;
}

auto gse::ide::config::project_output_dir() -> const std::filesystem::path& {
	return primary().project_output;
}

auto gse::ide::config::project_compile_commands() -> const std::filesystem::path& {
	return primary().project_compile_commands;
}

auto gse::ide::config::game_executable() -> const std::filesystem::path& {
	return primary().game_executable;
}

auto gse::ide::config::source_dir() -> const std::filesystem::path& {
	return editor().source;
}

auto gse::ide::config::resource_path() -> const std::filesystem::path& {
	return editor().resources;
}

auto gse::ide::config::build_dir() -> const std::filesystem::path& {
	return editor().build;
}

auto gse::ide::config::token_plugin() -> const std::filesystem::path& {
	return editor().token_plugin;
}

auto gse::ide::config::cppref_index() -> const std::filesystem::path& {
	return editor().cppref_index;
}

auto gse::ide::config::editor_executable() -> const std::filesystem::path& {
	return editor().executable;
}

auto gse::ide::config::editor_layout() -> const std::filesystem::path& {
	return editor().layout;
}

auto gse::ide::config::editor_layout_default() -> const std::filesystem::path& {
	return editor().layout_default;
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
