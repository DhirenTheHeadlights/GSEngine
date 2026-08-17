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

	struct registry {
		std::once_flag seeded;
		std::mutex mutex;
		std::deque<worktree> trees;
		std::deque<browse_root> tree_roots;
		std::deque<browse_root> fixed_roots;
		std::shared_ptr<const std::vector<const worktree*>> tree_index;
		std::shared_ptr<const std::vector<const browse_root*>> root_index;
		const worktree* primary_tree = nullptr;
	};

	auto is_inside(
		const std::filesystem::path& child,
		const std::filesystem::path& parent
	) -> bool;

	auto editor_compile_commands() -> std::filesystem::path;

	auto build_relative() -> std::filesystem::path;

	auto manifest_target(
		const project::manifest& active,
		std::string_view key,
		std::string_view fallback
	) -> std::string;

	auto resolve_worktree(
		const project::manifest& active,
		const std::filesystem::path& fallback_root,
		const std::filesystem::path& engine_build
	) -> worktree;

	auto append_worktree_roots(
		const worktree& tree,
		std::deque<browse_root>& out
	) -> void;

	auto append_fixed_roots(
		const worktree& tree,
		std::deque<browse_root>& out
	) -> void;

	auto republish(
		registry& reg
	) -> void;

	auto seed_registry(
		registry& reg
	) -> void;

	auto state() -> registry&;

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

auto gse::ide::config::build_relative() -> std::filesystem::path {
	const std::filesystem::path relative = gse::config::build_root().lexically_relative(gse::config::root_dir());
	if (relative.empty() || *relative.begin() == "..") {
		return "build";
	}
	return relative;
}

auto gse::ide::config::manifest_target(const project::manifest& active, const std::string_view key, const std::string_view fallback) -> std::string {
	if (const auto entry = active.targets.find(std::string(key)); entry != active.targets.end()) {
		return entry->second;
	}
	return std::string(fallback);
}

auto gse::ide::config::resolve_worktree(const project::manifest& active, const std::filesystem::path& fallback_root, const std::filesystem::path& engine_build) -> worktree {
	const std::filesystem::path engine_root = active.valid && !active.engine.empty() ? active.engine : fallback_root;
	const std::filesystem::path project_root = active.valid ? active.root : fallback_root;
	const std::filesystem::path project_source = active.valid ? active.source : fallback_root / "Game" / "Game";
	const std::filesystem::path project_assets = active.valid ? active.assets : gse::config::project_assets_path();
	const std::filesystem::path project_state = active.valid ? active.state : gse::config::user_config_dir();

	const bool nested = is_inside(project_root, engine_root);
	const std::filesystem::path project_build = nested ? engine_build : project_state / "build" / engine_build.filename();
	const std::filesystem::path project_output = nested ? engine_build / project_root.lexically_relative(engine_root) : project_build;

	const std::string game = manifest_target(active, "game", active.name);

	return {
		.name = active.valid ? active.name : fallback_root.filename().native_encoded_string(),
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

auto gse::ide::config::append_worktree_roots(const worktree& tree, std::deque<browse_root>& out) -> void {
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

auto gse::ide::config::append_fixed_roots(const worktree& tree, std::deque<browse_root>& out) -> void {
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

auto gse::ide::config::republish(registry& reg) -> void {
	auto trees = std::make_shared<std::vector<const worktree*>>();
	trees->reserve(reg.trees.size());
	for (const worktree& tree : reg.trees) {
		trees->push_back(&tree);
	}

	auto roots = std::make_shared<std::vector<const browse_root*>>();
	roots->reserve(reg.tree_roots.size() + reg.fixed_roots.size());
	for (const browse_root& root : reg.tree_roots) {
		roots->push_back(&root);
	}
	for (const browse_root& root : reg.fixed_roots) {
		roots->push_back(&root);
	}

	reg.tree_index = std::move(trees);
	reg.root_index = std::move(roots);
}

auto gse::ide::config::seed_registry(registry& reg) -> void {
	reg.trees.push_back(resolve_worktree(project::current(), gse::config::root_dir(), gse::config::build_root()));
	reg.primary_tree = &reg.trees.front();
	append_worktree_roots(reg.trees.front(), reg.tree_roots);
	append_fixed_roots(reg.trees.front(), reg.fixed_roots);
	republish(reg);
}

auto gse::ide::config::state() -> registry& {
	static registry value;
	std::call_once(value.seeded, seed_registry, std::ref(value));
	return value;
}

auto gse::ide::config::worktrees() -> std::vector<const worktree*> {
	registry& reg = state();
	const std::scoped_lock lock(reg.mutex);
	return *reg.tree_index;
}

auto gse::ide::config::primary() -> const worktree& {
	return *state().primary_tree;
}

auto gse::ide::config::worktree_for(const std::filesystem::path& file) -> const worktree& {
	const std::vector<const worktree*> trees = worktrees();
	const auto owner = std::ranges::find_if(trees, [&file](const worktree* tree) {
		return is_inside(file, tree->project_root) || is_inside(file, tree->engine_root);
	});
	return owner != trees.end() ? **owner : primary();
}

auto gse::ide::config::register_worktree(const std::filesystem::path& manifest_file) -> const worktree& {
	registry& reg = state();
	const std::scoped_lock lock(reg.mutex);

	const project::manifest loaded = project::load(manifest_file);
	const std::filesystem::path fallback_root = manifest_file.parent_path();
	const std::filesystem::path tree_engine = loaded.valid && !loaded.engine.empty() ? loaded.engine : fallback_root;
	worktree tree = resolve_worktree(loaded, fallback_root, tree_engine / build_relative());

	const auto existing = std::ranges::find(reg.trees, tree.project_root, &worktree::project_root);
	if (existing != reg.trees.end()) {
		return *existing;
	}

	reg.trees.push_back(std::move(tree));
	append_worktree_roots(reg.trees.back(), reg.tree_roots);
	republish(reg);
	return reg.trees.back();
}

auto gse::ide::config::browse_root_view::iterator::operator*() const -> const browse_root& {
	return **cursor;
}

auto gse::ide::config::browse_root_view::iterator::operator->() const -> const browse_root* {
	return *cursor;
}

auto gse::ide::config::browse_root_view::iterator::operator++() -> iterator& {
	++cursor;
	return *this;
}

auto gse::ide::config::browse_root_view::iterator::operator++(const int) -> iterator {
	const iterator previous = *this;
	++cursor;
	return previous;
}

auto gse::ide::config::browse_root_view::begin() const -> iterator {
	return { .cursor = generation->data() };
}

auto gse::ide::config::browse_root_view::end() const -> iterator {
	return { .cursor = generation->data() + generation->size() };
}

auto gse::ide::config::browse_root_view::size() const -> std::size_t {
	return generation->size();
}

auto gse::ide::config::browse_root_view::empty() const -> bool {
	return generation->empty();
}

auto gse::ide::config::browse_roots() -> browse_root_view {
	registry& reg = state();
	const std::scoped_lock lock(reg.mutex);
	return { .generation = reg.root_index };
}

auto gse::ide::config::analysis_roots() -> std::vector<std::filesystem::path> {
	const browse_root_view roots = browse_roots();
	std::vector<std::filesystem::path> out;
	out.reserve(roots.size());
	std::ranges::copy(
		roots | std::views::filter(&browse_root::analyzable) | std::views::transform(&browse_root::path),
		std::back_inserter(out)
	);
	return out;
}

auto gse::ide::config::compile_commands_for(const std::filesystem::path& file) -> const std::filesystem::path& {
	const browse_root_view roots = browse_roots();
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
