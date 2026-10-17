export module packer:image;

import std;

import gse;

export namespace packer {
	struct arguments {
		std::filesystem::path build_dir;
		std::filesystem::path engine_root;
		std::filesystem::path stage;
		gse::sdk::pack_kind kind = gse::sdk::pack_kind::sdk;
		std::string scenario = "render_stress";
		std::string version;
		std::string feed_url;
		std::string asset_base;
		std::string server;
		bool verify = true;
		bool setup = true;
	};

	struct transcript {
		std::vector<std::string> recorded;
		std::vector<std::string> history;
		bool recording = false;
	};

	struct image_plan {
		gse::sdk::pack_kind kind = gse::sdk::pack_kind::sdk;
		std::filesystem::path engine_root;
		std::filesystem::path build_dir;
		std::filesystem::path stage;
		std::filesystem::path compiler_bin;
		std::filesystem::path cmake;
		std::filesystem::path ninja;
		std::string build_type;
		std::filesystem::path game_executable;
		std::filesystem::path project_assets;
		std::filesystem::path project_baked;
		std::filesystem::path project_config;
		std::string verify_scenario;
		std::string version;
		std::string feed_url;
		std::string asset_base;
		std::string server;
	};

	auto emit(
		transcript& out,
		std::string text
	) -> void;

	auto plan_for(
		const arguments& args
	) -> std::expected<image_plan, std::string>;

	auto stage_image(
		const image_plan& plan,
		transcript& out
	) -> std::expected<void, std::string>;

	auto verify_image(
		const image_plan& plan,
		transcript& out
	) -> std::expected<void, std::string>;

	auto write_setup(
		const image_plan& plan,
		transcript& out
	) -> std::expected<std::filesystem::path, std::string>;

	auto write_transcript(
		const image_plan& plan,
		const transcript& out
	) -> std::filesystem::path;
}

namespace packer {
	constexpr std::string_view listing_name = "gse.modules";
	constexpr std::string_view link_name = "gse.link";
	constexpr std::string_view vcpkg_prefix = "vcpkg_installed/";
	constexpr std::string_view verify_dir = ".verify";
	constexpr std::string_view verify_marker = "sdk ok";
	constexpr std::string_view consumer_dir = "Engine/cmake/SdkConsumer";
	constexpr std::string_view consumer_exe = "SdkConsumer.exe";
	constexpr std::string_view installer_target = "Installer";
	constexpr std::string_view installer_exe = "Installer/Installer.exe";
	constexpr std::string_view debug_artifact_suffix = "_atlas_debug.png";
	constexpr std::string_view setup_target = "Setup";
	constexpr std::string_view setup_exe = "Installer/Setup.exe";
	constexpr std::string_view launcher_target = "Launcher";
	constexpr std::string_view launcher_exe = "Launcher/Launcher.exe";
	constexpr std::string_view sdk_product = "GSEngine SDK";
	constexpr std::string_view game_verify_marker = "frames measured";
	constexpr std::string_view unknown_scenario_marker = "unknown scenario";

	struct module_entry {
		std::string name;
		std::string path;

		auto operator<=>(
			const module_entry&
		) const = default;
	};

	struct tree_copy {
		std::string_view label;
		std::filesystem::path from;
		std::filesystem::path to;
	};

	auto cache_value(
		const std::filesystem::path& build_dir,
		std::string_view key
	) -> std::string;

	auto product_of(
		const image_plan& plan
	) -> std::string;

	auto begin_transcript(
		transcript& out
	) -> void;

	auto take_transcript(
		transcript& out
	) -> std::vector<std::string>;

	auto stage_runtime(
		const image_plan& plan,
		transcript& out
	) -> std::expected<void, std::string>;

	auto stage_sdk_payload(
		const image_plan& plan,
		transcript& out
	) -> std::expected<void, std::string>;

	auto verify_sdk(
		const image_plan& plan,
		transcript& out
	) -> std::expected<void, std::string>;

	auto verify_game(
		const image_plan& plan,
		transcript& out
	) -> std::expected<void, std::string>;

	auto module_listing(
		const image_plan& plan
	) -> std::expected<std::vector<module_entry>, std::string>;

	auto link_inputs(
		const image_plan& plan
	) -> std::expected<std::string, std::string>;

	auto stage_link_inputs(
		const image_plan& plan,
		transcript& out
	) -> std::expected<void, std::string>;

	auto write_manifest(
		const image_plan& plan,
		transcript& out
	) -> std::expected<void, std::string>;

	auto write_feed(
		const image_plan& plan,
		const std::filesystem::path& setup,
		transcript& out
	) -> std::expected<void, std::string>;

	auto quoted(
		const std::filesystem::path& path
	) -> std::string;

	auto run_command(
		transcript& out,
		const image_plan& plan,
		const std::string& command,
		const std::filesystem::path& cwd
	) -> int;
}

auto packer::emit(transcript& out, std::string text) -> void {
	if (out.recording) {
		out.recorded.push_back(text);
	}
	std::println("{}", text);
	std::cout.flush();
	out.history.push_back(std::move(text));
}

auto packer::begin_transcript(transcript& out) -> void {
	out.recorded.clear();
	out.recording = true;
}

auto packer::take_transcript(transcript& out) -> std::vector<std::string> {
	out.recording = false;
	return std::move(out.recorded);
}

auto packer::cache_value(const std::filesystem::path& build_dir, const std::string_view key) -> std::string {
	std::ifstream cache(build_dir / "CMakeCache.txt");
	if (!cache) {
		return {};
	}

	const std::string prefix = std::string(key) + ":";
	std::string line;
	while (std::getline(cache, line)) {
		if (!line.starts_with(prefix)) {
			continue;
		}
		const std::size_t equals = line.find('=');
		if (equals == std::string::npos) {
			break;
		}
		return line.substr(equals + 1);
	}
	return {};
}

auto packer::product_of(const image_plan& plan) -> std::string {
	return plan.kind == gse::sdk::pack_kind::sdk ? std::string(sdk_product) : plan.game_executable.stem().generic_native_encoded_string();
}

auto packer::plan_for(const arguments& args) -> std::expected<image_plan, std::string> {
	if (args.build_dir.empty()) {
		return std::unexpected("pass --build-dir, the configured build tree to package from");
	}
	const std::filesystem::path build_dir = std::filesystem::absolute(args.build_dir).lexically_normal();
	std::error_code ec;
	if (!std::filesystem::exists(build_dir / "build.ninja", ec)) {
		return std::unexpected(std::format("{} is not a configured build tree (no build.ninja)", build_dir.generic_display_string()));
	}
	const std::filesystem::path manifest = build_dir / "gse.manifest";
	if (!std::filesystem::exists(manifest, ec)) {
		return std::unexpected(std::format("{} has no gse.manifest; configure the tree first", build_dir.generic_display_string()));
	}

	const std::string text = gse::fs::read_text(manifest);
	const std::filesystem::path engine_root = args.engine_root.empty() ? std::filesystem::path(gse::config::manifest_value(text, "root")) : args.engine_root;
	const std::filesystem::path project_root = gse::config::manifest_value(text, "project");
	if (engine_root.empty() || project_root.empty()) {
		return std::unexpected(std::format("{} names no root and project", manifest.generic_display_string()));
	}

	const std::string project = project_root.filename().generic_native_encoded_string();
	const std::string compiler = cache_value(build_dir, "CMAKE_CXX_COMPILER");
	const std::filesystem::path preset = build_dir.filename();
	const std::filesystem::path stage = args.stage.empty() ? engine_root / "dist" / (args.kind == gse::sdk::pack_kind::sdk ? "engine-sdk" : "game") / preset : args.stage;
	return image_plan{
		.kind = args.kind,
		.engine_root = std::filesystem::absolute(engine_root).lexically_normal(),
		.build_dir = build_dir,
		.stage = std::filesystem::absolute(stage).lexically_normal(),
		.compiler_bin = compiler.empty() ? std::filesystem::path{} : std::filesystem::path(compiler).parent_path(),
		.cmake = cache_value(build_dir, "CMAKE_COMMAND"),
		.ninja = cache_value(build_dir, "CMAKE_MAKE_PROGRAM"),
		.build_type = cache_value(build_dir, "CMAKE_BUILD_TYPE"),
		.game_executable = build_dir / project / (project + ".exe"),
		.project_assets = project_root / gse::config::project_assets_subdir,
		.project_baked = project_root / gse::config::project_baked_subdir,
		.project_config = project_root / gse::config::project_config_subdir,
		.verify_scenario = args.scenario,
		.version = args.version,
		.feed_url = args.feed_url,
		.asset_base = args.asset_base,
		.server = args.server,
	};
}

auto packer::module_listing(const image_plan& plan) -> std::expected<std::vector<module_entry>, std::string> {
	const std::string prefix = plan.build_dir.generic_native_encoded_string() + "/";
	std::vector<module_entry> modules;
	std::error_code ec;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(plan.build_dir / "Engine" / "CMakeFiles", ec)) {
		const std::filesystem::path listing = entry.path() / "CXXModules.json";
		if (!entry.is_directory() || !std::filesystem::exists(listing)) {
			continue;
		}
		const std::expected<gse::json::value, gse::json::parse_error> root = gse::json::parse(gse::fs::read_text(listing));
		if (!root) {
			return std::unexpected(std::format("{} is not readable json: {}", listing.generic_display_string(), gse::json::message(root.error().code)));
		}
		const gse::json::value* entries = root->find("modules");
		if (entries == nullptr || !entries->is_object()) {
			continue;
		}
		for (const std::string& name : entries->keys()) {
			const gse::json::value* bmi = entries->find(name)->find("bmi");
			const std::string path = bmi ? std::string(bmi->text()) : std::string{};
			if (!path.starts_with(prefix)) {
				return std::unexpected(std::format("{}: BMI {} is outside the build tree", name, path));
			}
			modules.push_back({
				.name = name,
				.path = path.substr(prefix.size()),
			});
		}
	}
	if (modules.empty()) {
		return std::unexpected(std::format("no CXXModules.json under {}; build the engine first", plan.build_dir.generic_display_string()));
	}
	std::ranges::sort(modules);
	return modules;
}

auto packer::stage_image(const image_plan& plan, transcript& out) -> std::expected<void, std::string> {
	emit(out, "build tree: " + plan.build_dir.generic_display_string());
	emit(out, "engine:     " + plan.engine_root.generic_display_string());
	emit(out, "image:      " + plan.stage.generic_display_string());

	std::error_code ec;
	std::filesystem::remove_all(plan.stage, ec);
	std::filesystem::create_directories(plan.stage, ec);
	if (ec) {
		return std::unexpected(std::format("could not reset {}: {}", plan.stage.generic_display_string(), ec.message()));
	}

	if (const auto staged = stage_runtime(plan, out); !staged) {
		return staged;
	}
	if (plan.kind == gse::sdk::pack_kind::sdk) {
		if (const auto staged = stage_sdk_payload(plan, out); !staged) {
			return staged;
		}
	}
	return write_manifest(plan, out);
}

auto packer::stage_runtime(const image_plan& plan, transcript& out) -> std::expected<void, std::string> {
	std::error_code ec;
	const std::filesystem::path bin_source = plan.game_executable.parent_path();
	const std::array<tree_copy, 3> trees = {{
		{
			.label = "resources",
			.from = plan.engine_root / "Engine" / "Resources",
			.to = plan.stage / "Engine" / "Resources",
		},
		{
			.label = "baked",
			.from = plan.build_dir / "Engine" / "Resources",
			.to = plan.stage / "Engine" / "Baked",
		},
		{
			.label = "d3d12",
			.from = bin_source / "D3D12",
			.to = plan.stage / "Bin" / "D3D12",
		},
	}};
	for (const tree_copy& tree : trees) {
		const auto copied = gse::fs::copy_tree(tree.from, tree.to);
		if (!copied) {
			return std::unexpected(copied.error());
		}
		emit(out, std::format("{:<11}{} files", std::string(tree.label) + ":", *copied));
	}

	std::vector<std::filesystem::path> debug_artifacts;
	for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(plan.stage / "Engine" / "Baked", ec)) {
		if (entry.is_regular_file() && entry.path().filename().generic_native_encoded_string().ends_with(debug_artifact_suffix)) {
			debug_artifacts.push_back(entry.path());
		}
	}
	for (const std::filesystem::path& artifact : debug_artifacts) {
		std::filesystem::remove(artifact, ec);
	}
	emit(out, std::format("baked:     dropped {} debug artifacts", debug_artifacts.size()));

	std::size_t dlls = 0;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(bin_source, ec)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".dll") {
			continue;
		}
		if (const auto copied = gse::fs::copy_file(entry.path(), plan.stage / "Bin" / entry.path().filename()); !copied) {
			return copied;
		}
		++dlls;
	}
	emit(out, std::format("bin:       {} dlls", dlls));

	if (plan.kind == gse::sdk::pack_kind::sdk) {
		return {};
	}

	const std::array<tree_copy, 3> project_trees = {{
		{
			.label = "assets",
			.from = plan.project_assets,
			.to = plan.stage / gse::config::project_assets_subdir,
		},
		{
			.label = "pbaked",
			.from = plan.project_baked,
			.to = plan.stage / gse::config::project_baked_subdir,
		},
		{
			.label = "pconfig",
			.from = plan.project_config,
			.to = plan.stage / gse::config::project_config_subdir,
		},
	}};
	for (const tree_copy& tree : project_trees) {
		if (!std::filesystem::exists(tree.from, ec)) {
			emit(out, std::format("{:<11}none at {}", std::string(tree.label) + ":", tree.from.generic_display_string()));
			continue;
		}
		const auto copied = gse::fs::copy_tree(tree.from, tree.to);
		if (!copied) {
			return std::unexpected(copied.error());
		}
		emit(out, std::format("{:<11}{} files", std::string(tree.label) + ":", *copied));
	}

	if (!std::filesystem::exists(plan.game_executable, ec)) {
		return std::unexpected(std::format("{} does not exist; build the game before packaging it", plan.game_executable.generic_display_string()));
	}
	if (const auto copied = gse::fs::copy_file(plan.game_executable, plan.stage / "Bin" / plan.game_executable.filename()); !copied) {
		return copied;
	}
	emit(out, "game:      " + plan.game_executable.filename().generic_display_string());
	return {};
}

auto packer::stage_sdk_payload(const image_plan& plan, transcript& out) -> std::expected<void, std::string> {
	const auto modules = module_listing(plan);
	if (!modules) {
		return std::unexpected(modules.error());
	}

	std::error_code ec;
	std::string listing = "$root .\n";
	std::set<std::string> bmi_paths;
	for (const auto& [name, path] : *modules) {
		listing += name + " " + path + "\n";
		bmi_paths.insert(path);
	}
	if (const auto written = gse::fs::write_text(plan.stage / listing_name, listing); !written) {
		return written;
	}
	emit(out, std::format("listing:   {} modules -> {}", modules->size(), listing_name));

	for (const std::string& path : bmi_paths) {
		if (const auto copied = gse::fs::copy_file(plan.build_dir / path, plan.stage / path); !copied) {
			return copied;
		}
	}
	emit(out, std::format("modules:   {} BMIs", bmi_paths.size()));

	std::size_t archives = 0;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(plan.build_dir / "Engine", ec)) {
		const std::string filename = entry.path().filename().generic_native_encoded_string();
		if (!entry.is_regular_file() || !filename.starts_with("lib") || !filename.ends_with(".a")) {
			continue;
		}
		if (const auto copied = gse::fs::copy_file(entry.path(), plan.stage / "Engine" / filename); !copied) {
			return copied;
		}
		++archives;
	}
	emit(out, std::format("libraries: {} archives", archives));

	const std::array<tree_copy, 2> trees = {{
		{
			.label = "cmake",
			.from = plan.engine_root / "Engine" / "cmake",
			.to = plan.stage / "Engine" / "cmake",
		},
		{
			.label = "source",
			.from = plan.engine_root / "Engine" / "Engine",
			.to = plan.stage / "Engine" / "Source",
		},
	}};
	for (const tree_copy& tree : trees) {
		const auto copied = gse::fs::copy_tree(tree.from, tree.to);
		if (!copied) {
			return std::unexpected(copied.error());
		}
		emit(out, std::format("{:<11}{} files", std::string(tree.label) + ":", *copied));
	}

	return stage_link_inputs(plan, out);
}

auto packer::write_manifest(const image_plan& plan, transcript& out) -> std::expected<void, std::string> {
	const auto toolchain_bin = gse::fs::resolve_links(plan.compiler_bin);
	if (!toolchain_bin) {
		return std::unexpected(std::format("could not resolve the toolchain behind {}: {}", plan.compiler_bin.generic_display_string(), toolchain_bin.error()));
	}
	const std::string toolchain = toolchain_bin->parent_path().filename().generic_native_encoded_string();

	begin_transcript(out);
	const int code = run_command(out, plan, "git rev-parse HEAD", plan.engine_root);
	const std::vector<std::string> output = take_transcript(out);
	const auto is_commit = [](const std::string& line) {
		return line.size() == 40 && std::ranges::all_of(line, [](const char c) {
			return std::isxdigit(static_cast<unsigned char>(c)) != 0;
		});
	};
	const auto found = std::ranges::find_if(output, is_commit);
	if (code != 0 || found == output.end()) {
		return std::unexpected(std::format("git rev-parse HEAD failed in {}", plan.engine_root.generic_display_string()));
	}
	const std::string commit = *found;
	const std::string version = plan.version.empty() ? commit.substr(0, 12) : plan.version;

	if (plan.build_type.empty()) {
		return std::unexpected("the build tree's CMakeCache.txt names no CMAKE_BUILD_TYPE");
	}

	const std::string product = product_of(plan);
	emit(out, std::format("stamp:     {} version {} toolchain {} engine {} build type {}", product, version, toolchain, commit, plan.build_type));
	std::string manifest_text = std::format("mode = installed\nkind = {}\nproduct = {}\nversion = {}\ntoolchain = {}\nengine = {}\nbuild_type = {}\n", gse::enum_to_string(plan.kind), product, version, toolchain, commit, plan.build_type);
	if (!plan.feed_url.empty()) {
		std::format_to(std::back_inserter(manifest_text), "feed = {}\n", plan.feed_url);
		emit(out, "feed:      " + plan.feed_url);
	}
	return gse::fs::write_text(plan.stage / "gse.manifest", manifest_text).and_then([&] -> std::expected<void, std::string> {
		if (!gse::sdk::traits_of(plan.kind).registers_image) {
			return {};
		}
		gse::sdk::register_image(version, plan.stage.parent_path());
		emit(out, std::format("registered: [engine] version = {} binds {} ({} config)", version, plan.stage.parent_path().generic_display_string(), plan.stage.filename().generic_display_string()));
		return {};
	});
}

auto packer::link_inputs(const image_plan& plan) -> std::expected<std::string, std::string> {
	const std::filesystem::path exe = plan.game_executable.lexically_relative(plan.build_dir);
	const std::string target = "build " + exe.generic_native_encoded_string() + ":";
	const std::string project = exe.begin()->generic_native_encoded_string() + "/";
	std::istringstream ninja(gse::fs::read_text(plan.build_dir / "build.ninja"));
	std::string line;
	bool inside = false;
	std::string libraries;
	std::string flags;
	while (std::getline(ninja, line)) {
		if (line.ends_with('\r')) {
			line.pop_back();
		}
		if (line.starts_with(target)) {
			inside = true;
			continue;
		}
		if (!inside) {
			continue;
		}
		if (!line.starts_with("  ")) {
			break;
		}
		if (line.starts_with("  LINK_LIBRARIES = ")) {
			libraries = line.substr(19);
		}
		else if (line.starts_with("  LINK_FLAGS = ")) {
			flags = line.substr(15);
		}
	}
	if (libraries.empty()) {
		return std::unexpected(std::format("no link stanza for {} in build.ninja", exe.generic_display_string()));
	}

	std::string inputs = flags;
	for (const auto token : std::views::split(libraries, ' ')) {
		const std::string_view text(token);
		if (text.empty() || text.starts_with(project)) {
			continue;
		}
		inputs += " ";
		inputs += text;
	}
	return inputs;
}

auto packer::stage_link_inputs(const image_plan& plan, transcript& out) -> std::expected<void, std::string> {
	const auto inputs = link_inputs(plan);
	if (!inputs) {
		return std::unexpected(inputs.error());
	}
	std::size_t vendored = 0;
	for (const auto token : std::views::split(*inputs, ' ')) {
		const std::string_view text(token);
		if (!text.starts_with(vcpkg_prefix)) {
			continue;
		}
		if (const auto copied = gse::fs::copy_file(plan.build_dir / text, plan.stage / text); !copied) {
			return copied;
		}
		++vendored;
	}
	emit(out, std::format("vendored:  {} archives -> {}", vendored, link_name));
	return gse::fs::write_text(plan.stage / link_name, *inputs + "\n");
}

auto packer::quoted(const std::filesystem::path& path) -> std::string {
	return "\"" + path.generic_native_encoded_string() + "\"";
}

auto packer::run_command(transcript& out, const image_plan& plan, const std::string& command, const std::filesystem::path& cwd) -> int {
	emit(out, "> " + command);
	const std::filesystem::path captured = gse::process::temporary_path("packer", "log");
	const auto _ = gse::make_scope_exit([&captured] {
		std::error_code ignored;
		std::filesystem::remove(captured, ignored);
	});
	const gse::process::run_outcome outcome = gse::process::run_capture({
		.command_line = command,
		.working_dir = cwd,
		.output_path = captured,
		.path_prefix = plan.compiler_bin,
		.limit = gse::seconds(3600.f),
	});

	std::ifstream log(captured);
	for (std::string line; std::getline(log, line); ) {
		if (line.ends_with('\r')) {
			line.pop_back();
		}
		emit(out, std::move(line));
	}
	if (!outcome) {
		emit(out, std::format("command did not complete: {}", gse::enum_to_string(outcome.error())));
		return -1;
	}
	return *outcome;
}

auto packer::verify_image(const image_plan& plan, transcript& out) -> std::expected<void, std::string> {
	return plan.kind == gse::sdk::pack_kind::sdk ? verify_sdk(plan, out) : verify_game(plan, out);
}

auto packer::verify_game(const image_plan& plan, transcript& out) -> std::expected<void, std::string> {
	if (plan.verify_scenario.empty()) {
		return std::unexpected("no verify scenario was given for the game pack; pass --scenario");
	}
	const std::filesystem::path exe = plan.stage / "Bin" / plan.game_executable.filename();
	emit(out, std::format("verify: running {} in the staged image", plan.verify_scenario));
	begin_transcript(out);
	const int code = run_command(out, plan, std::format("{} --engine-bench-scenario {}", quoted(exe), plan.verify_scenario), plan.stage);
	const std::vector<std::string> output = take_transcript(out);
	const bool unknown = std::ranges::any_of(output, [](const std::string& text) {
		return text.starts_with(unknown_scenario_marker);
	});
	const bool completed = std::ranges::any_of(output, [](const std::string& text) {
		return text.contains(game_verify_marker);
	});
	if (unknown) {
		return std::unexpected(std::format("'{}' is not a scenario {} knows; name one from the project's catalogue with --scenario", plan.verify_scenario, plan.game_executable.filename().generic_display_string()));
	}
	if (code != 0) {
		return std::unexpected(std::format("the packed game exited with {} during the {} scenario", code, plan.verify_scenario));
	}
	if (!completed) {
		return std::unexpected(std::format("the packed game exited cleanly but never reported '{}' for the {} scenario", game_verify_marker, plan.verify_scenario));
	}
	return {};
}

auto packer::verify_sdk(const image_plan& plan, transcript& out) -> std::expected<void, std::string> {
	if (plan.cmake.empty() || plan.ninja.empty()) {
		return std::unexpected("the build tree's CMakeCache.txt does not name cmake and ninja");
	}
	const std::filesystem::path work = plan.stage / verify_dir;
	const auto _ = gse::make_scope_exit([&work] {
		std::error_code ignored;
		std::filesystem::remove_all(work, ignored);
	});

	emit(out, "verify: configuring the SDK consumer against the staged image");
	const std::string configure = std::format("{} -S {} -B {} -G Ninja -DCMAKE_MAKE_PROGRAM={} -DGSE_SDK_DIR={}", quoted(plan.cmake), consumer_dir, verify_dir, quoted(plan.ninja), quoted(plan.stage));
	if (run_command(out, plan, configure, plan.stage) != 0) {
		return std::unexpected("the SDK consumer did not configure against the image");
	}

	emit(out, "verify: building the SDK consumer");
	const std::string build = std::format("{} --build {}", quoted(plan.cmake), verify_dir);
	if (run_command(out, plan, build, plan.stage) != 0) {
		return std::unexpected("the SDK consumer did not build against the image");
	}

	const std::filesystem::path exe = work / consumer_exe;
	begin_transcript(out);
	const int code = run_command(out, plan, quoted(exe), work);
	const std::vector<std::string> output = take_transcript(out);
	const bool reported = std::ranges::any_of(output, [](const std::string& text) {
		return text.starts_with(verify_marker);
	});
	const std::string expected_root = "engine root: " + plan.stage.generic_display_string();
	const bool rooted = std::ranges::any_of(output, [&expected_root](const std::string& text) {
		return text == expected_root;
	});
	if (code != 0 || !reported) {
		return std::unexpected(std::format("the SDK consumer exited with {} without reporting '{}'", code, verify_marker));
	}
	if (!rooted) {
		return std::unexpected(std::format("the SDK consumer resolved its engine root outside the image (expected '{}')", expected_root));
	}
	return {};
}

auto packer::write_setup(const image_plan& plan, transcript& out) -> std::expected<std::filesystem::path, std::string> {
	if (plan.cmake.empty()) {
		return std::unexpected("the build tree's CMakeCache.txt does not name cmake");
	}
	const bool game = plan.kind == gse::sdk::pack_kind::game;
	emit(out, "setup: building the installer and its setup stub");
	const std::string build = std::format("{} --build {} --target {} {}{}", quoted(plan.cmake), quoted(plan.build_dir), installer_target, setup_target, game ? std::format(" {}", launcher_target) : std::string{});
	if (run_command(out, plan, build, plan.build_dir) != 0) {
		return std::unexpected("the installer or its setup stub did not build");
	}
	if (const auto copied = gse::fs::copy_file(plan.build_dir / installer_exe, plan.stage / "Bin" / std::filesystem::path(installer_exe).filename()); !copied) {
		return std::unexpected(copied.error());
	}
	if (game) {
		if (const auto copied = gse::fs::copy_file(plan.build_dir / launcher_exe, plan.stage / "Bin" / std::filesystem::path(launcher_exe).filename()); !copied) {
			return std::unexpected(copied.error());
		}
		emit(out, "launcher:  " + std::filesystem::path(launcher_exe).filename().generic_display_string());
	}

	const std::string version = gse::config::manifest_value(gse::fs::read_text(plan.stage / "gse.manifest"), "version");
	const std::string preset = plan.stage.filename().generic_native_encoded_string();
	const std::string product = product_of(plan);
	const std::filesystem::path setup = plan.stage.parent_path() / std::format("{}-{}-{}-setup.exe", plan.kind == gse::sdk::pack_kind::sdk ? "GSEngine" : product, version, preset);
	if (const auto copied = gse::fs::copy_file(plan.build_dir / setup_exe, setup); !copied) {
		return std::unexpected(copied.error());
	}
	const auto packed = gse::sdk::append_pack(plan.stage, setup, {
		.version = version,
		.preset = preset,
		.product = product,
		.kind = plan.kind,
	});
	if (!packed) {
		return std::unexpected(packed.error());
	}
	std::error_code ec;
	emit(out, std::format("setup:     {} files packed -> {} ({:.1f} MB)", packed->entries.size(), setup.generic_display_string(), static_cast<double>(std::filesystem::file_size(setup, ec)) / 1048576.0));
	if (const auto published = write_feed(plan, setup, out); !published) {
		return std::unexpected(published.error());
	}
	return setup;
}

auto packer::write_transcript(const image_plan& plan, const transcript& out) -> std::filesystem::path {
	const std::filesystem::path path = plan.stage.parent_path() / std::format("packer-{}.log", gse::enum_to_string(plan.kind));
	std::string text;
	for (const std::string& line : out.history) {
		text += line;
		text += '\n';
	}
	return gse::fs::write_text(path, text) ? path : std::filesystem::path{};
}

auto packer::write_feed(const image_plan& plan, const std::filesystem::path& setup, transcript& out) -> std::expected<void, std::string> {
	if (plan.asset_base.empty()) {
		return {};
	}
	const auto digest = gse::sdk::digest_of(setup);
	if (!digest) {
		return std::unexpected(digest.error());
	}
	std::error_code ec;
	const std::string asset = setup.filename().generic_native_encoded_string();
	const gse::sdk::feed entry{
		.product = product_of(plan),
		.version = gse::config::manifest_value(gse::fs::read_text(plan.stage / "gse.manifest"), "version"),
		.preset = plan.stage.filename().generic_native_encoded_string(),
		.asset = asset,
		.url = plan.asset_base + "/" + asset,
		.sha256 = *digest,
		.size = std::filesystem::file_size(setup, ec),
		.server = plan.server,
	};
	const std::filesystem::path path = setup.parent_path() / gse::sdk::feed_name;
	if (const auto written = gse::fs::write_text(path, gse::sdk::write_feed(entry)); !written) {
		return written;
	}
	emit(out, std::format("feed:      {} -> {}", path.generic_display_string(), entry.url));
	return {};
}
