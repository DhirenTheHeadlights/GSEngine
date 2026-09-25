export module gse.ide.build:package_sdk;

import gse;
import gse.ide.config;
import gse.ide.project;
import gse.win32.environment;
import std;

import :build_runner;
import :inbox;
import :spawn;

export namespace gse::ide {
	namespace package_sdk {
		struct request {};

		struct image_plan {
			std::filesystem::path engine_root;
			std::filesystem::path build_dir;
			std::filesystem::path stage;
			std::filesystem::path compiler_bin;
			std::filesystem::path cmake;
			std::filesystem::path ninja;
			std::string build_type;
			std::filesystem::path game_executable;
		};

		auto plan_for(
			const config::worktree& tree
		) -> image_plan;

		auto stage_image(
			const image_plan& plan,
			spawn::output_stream& stream
		) -> std::expected<void, std::string>;

		auto verify_image(
			const image_plan& plan,
			spawn::output_stream& stream
		) -> std::expected<void, std::string>;
	}

	namespace package_system {
		struct [[= system_state<"Package SDK">{}]] data {
			task::pending<std::expected<std::filesystem::path, std::string>> job;
			std::string inbox_id;
		};

		[[= system_run<>{}]]
		auto run(
			context& ctx,
			data& d,
			channel_read<package_sdk::request> requests_in,
			channel_write<build_runner::stream_opened> events_out,
			shared_view<build_runner::data> build_d
		) -> async::task<>;

		[[= system_shutdown{}]]
		auto shutdown(
			data& d
		) -> void;
	}
}

namespace gse::ide::package_sdk {
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

	auto module_listing(
		const image_plan& plan
	) -> std::expected<std::vector<module_entry>, std::string>;

	auto link_inputs(
		const image_plan& plan
	) -> std::expected<std::string, std::string>;

	auto stage_link_inputs(
		const image_plan& plan,
		spawn::output_stream& stream
	) -> std::expected<void, std::string>;

	auto write_manifest(
		const image_plan& plan,
		spawn::output_stream& stream
	) -> std::expected<void, std::string>;

	auto write_setup(
		const image_plan& plan,
		spawn::output_stream& stream
	) -> std::expected<std::filesystem::path, std::string>;

	auto quoted(
		const std::filesystem::path& path
	) -> std::string;

	auto run_command(
		spawn::output_stream& stream,
		const image_plan& plan,
		const std::string& command,
		const std::filesystem::path& cwd
	) -> int;

	auto owns_request(
		const build_inbox::package_request& incoming
	) -> bool;
}

auto gse::ide::package_sdk::owns_request(const build_inbox::package_request& incoming) -> bool {
	if (!incoming.cwd.empty() && !config::owning_worktree(incoming.cwd)) {
		return false;
	}
	if (incoming.project.empty()) {
		return true;
	}
	std::error_code ec;
	const bool same_project = std::filesystem::equivalent(incoming.project, config::project_root(), ec);
	return !ec && same_project;
}

auto gse::ide::package_sdk::plan_for(const config::worktree& tree) -> image_plan {
	const std::filesystem::path build_dir = build_runner::find_build_dir(tree.project_build);
	return {
		.engine_root = tree.engine_root,
		.build_dir = build_dir,
		.stage = tree.engine_root / "dist" / "engine-sdk" / build_dir.filename(),
		.compiler_bin = build_runner::compiler_bin_dir(tree.project_build),
		.cmake = build_runner::cache_value(build_dir, "CMAKE_COMMAND"),
		.ninja = build_runner::cache_value(build_dir, "CMAKE_MAKE_PROGRAM"),
		.build_type = build_runner::cache_value(build_dir, "CMAKE_BUILD_TYPE"),
		.game_executable = tree.game_executable,
	};
}

auto gse::ide::package_sdk::module_listing(const image_plan& plan) -> std::expected<std::vector<module_entry>, std::string> {
	const std::string prefix = plan.build_dir.generic_native_encoded_string() + "/";
	std::vector<module_entry> modules;
	std::error_code ec;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(plan.build_dir / "Engine" / "CMakeFiles", ec)) {
		const std::filesystem::path listing = entry.path() / "CXXModules.json";
		if (!entry.is_directory() || !std::filesystem::exists(listing)) {
			continue;
		}
		const std::expected<json::value, json::parse_error> root = json::parse(fs::read_text(listing));
		if (!root) {
			return std::unexpected(std::format("{} is not readable json: {}", listing.generic_display_string(), json::message(root.error().code)));
		}
		const json::value* entries = root->find("modules");
		if (entries == nullptr || !entries->is_object()) {
			continue;
		}
		for (const std::string& name : entries->keys()) {
			const json::value* bmi = entries->find(name)->find("bmi");
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

auto gse::ide::package_sdk::stage_image(const image_plan& plan, spawn::output_stream& stream) -> std::expected<void, std::string> {
	if (plan.build_dir.empty()) {
		return std::unexpected("the project's build tree is not configured; build the game once first");
	}
	spawn::emit(stream, "build tree: " + plan.build_dir.generic_display_string());
	spawn::emit(stream, "engine:     " + plan.engine_root.generic_display_string());
	spawn::emit(stream, "image:      " + plan.stage.generic_display_string());

	const auto modules = module_listing(plan);
	if (!modules) {
		return std::unexpected(modules.error());
	}

	std::error_code ec;
	std::filesystem::remove_all(plan.stage, ec);
	std::filesystem::create_directories(plan.stage, ec);
	if (ec) {
		return std::unexpected(std::format("could not reset {}: {}", plan.stage.generic_display_string(), ec.message()));
	}

	std::string listing = "$root .\n";
	std::set<std::string> bmi_paths;
	for (const auto& [name, path] : *modules) {
		listing += name + " " + path + "\n";
		bmi_paths.insert(path);
	}
	if (const auto written = fs::write_text(plan.stage / listing_name, listing); !written) {
		return written;
	}
	spawn::emit(stream, std::format("listing:   {} modules -> {}", modules->size(), listing_name));

	for (const std::string& path : bmi_paths) {
		if (const auto copied = fs::copy_file(plan.build_dir / path, plan.stage / path); !copied) {
			return copied;
		}
	}
	spawn::emit(stream, std::format("modules:   {} BMIs", bmi_paths.size()));

	std::size_t archives = 0;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(plan.build_dir / "Engine", ec)) {
		const std::string filename = entry.path().filename().generic_native_encoded_string();
		if (!entry.is_regular_file() || !filename.starts_with("lib") || !filename.ends_with(".a")) {
			continue;
		}
		if (const auto copied = fs::copy_file(entry.path(), plan.stage / "Engine" / filename); !copied) {
			return copied;
		}
		++archives;
	}
	spawn::emit(stream, std::format("libraries: {} archives", archives));

	const std::filesystem::path bin_source = plan.game_executable.parent_path();
	const std::array<tree_copy, 5> trees = {{
		{
			.label = "cmake",
			.from = plan.engine_root / "Engine" / "cmake",
			.to = plan.stage / "Engine" / "cmake",
		},
		{
			.label = "resources",
			.from = plan.engine_root / "Engine" / "Resources",
			.to = plan.stage / "Engine" / "Resources",
		},
		{
			.label = "source",
			.from = plan.engine_root / "Engine" / "Engine",
			.to = plan.stage / "Engine" / "Source",
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
		const auto copied = fs::copy_tree(tree.from, tree.to);
		if (!copied) {
			return std::unexpected(copied.error());
		}
		spawn::emit(stream, std::format("{:<11}{} files", std::string(tree.label) + ":", *copied));
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
	spawn::emit(stream, std::format("baked:     dropped {} debug artifacts", debug_artifacts.size()));

	std::size_t dlls = 0;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(bin_source, ec)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".dll") {
			continue;
		}
		if (const auto copied = fs::copy_file(entry.path(), plan.stage / "Bin" / entry.path().filename()); !copied) {
			return copied;
		}
		++dlls;
	}
	spawn::emit(stream, std::format("bin:       {} dlls", dlls));

	if (const auto linked = stage_link_inputs(plan, stream); !linked) {
		return linked;
	}
	return write_manifest(plan, stream);
}

auto gse::ide::package_sdk::write_manifest(const image_plan& plan, spawn::output_stream& stream) -> std::expected<void, std::string> {
	const auto toolchain_bin = fs::resolve_links(plan.compiler_bin);
	if (!toolchain_bin) {
		return std::unexpected(std::format("could not resolve the toolchain behind {}: {}", plan.compiler_bin.generic_display_string(), toolchain_bin.error()));
	}
	const std::string toolchain = toolchain_bin->parent_path().filename().generic_native_encoded_string();

	spawn::begin_transcript(stream);
	const int code = run_command(stream, plan, "git rev-parse HEAD", plan.engine_root);
	const std::vector<std::string> output = spawn::take_transcript(stream);
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
	const std::string version = commit.substr(0, 12);

	if (plan.build_type.empty()) {
		return std::unexpected("the build tree's CMakeCache.txt names no CMAKE_BUILD_TYPE");
	}

	spawn::emit(stream, std::format("stamp:     version {} toolchain {} engine {} build type {}", version, toolchain, commit, plan.build_type));
	return fs::write_text(plan.stage / "gse.manifest", std::format("mode = installed\nversion = {}\ntoolchain = {}\nengine = {}\nbuild_type = {}\n", version, toolchain, commit, plan.build_type)).and_then([&] -> std::expected<void, std::string> {
		project::register_sdk(version, plan.stage.parent_path());
		spawn::emit(stream, std::format("registered: [engine] version = {} binds {} ({} config)", version, plan.stage.parent_path().generic_display_string(), plan.stage.filename().generic_display_string()));
		return {};
	});
}

auto gse::ide::package_sdk::link_inputs(const image_plan& plan) -> std::expected<std::string, std::string> {
	const std::filesystem::path exe = plan.game_executable.lexically_relative(plan.build_dir);
	const std::string target = "build " + exe.generic_native_encoded_string() + ":";
	const std::string project = exe.begin()->generic_native_encoded_string() + "/";
	std::istringstream ninja(fs::read_text(plan.build_dir / "build.ninja"));
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

auto gse::ide::package_sdk::stage_link_inputs(const image_plan& plan, spawn::output_stream& stream) -> std::expected<void, std::string> {
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
		if (const auto copied = fs::copy_file(plan.build_dir / text, plan.stage / text); !copied) {
			return copied;
		}
		++vendored;
	}
	spawn::emit(stream, std::format("vendored:  {} archives -> {}", vendored, link_name));
	return fs::write_text(plan.stage / link_name, *inputs + "\n");
}

auto gse::ide::package_sdk::quoted(const std::filesystem::path& path) -> std::string {
	return "\"" + path.generic_native_encoded_string() + "\"";
}

auto gse::ide::package_sdk::run_command(spawn::output_stream& stream, const image_plan& plan, const std::string& command, const std::filesystem::path& cwd) -> int {
	spawn::emit(stream, "> " + command);
	return spawn::run_capture(stream, win32::widen(command), cwd.wstring(), plan.compiler_bin);
}

auto gse::ide::package_sdk::verify_image(const image_plan& plan, spawn::output_stream& stream) -> std::expected<void, std::string> {
	if (plan.cmake.empty() || plan.ninja.empty()) {
		return std::unexpected("the build tree's CMakeCache.txt does not name cmake and ninja");
	}
	const std::filesystem::path work = plan.stage / verify_dir;
	const auto _ = make_scope_exit([&work] {
		std::error_code ignored;
		std::filesystem::remove_all(work, ignored);
	});

	spawn::emit(stream, "verify: configuring the SDK consumer against the staged image");
	const std::string configure = std::format("{} -S {} -B {} -G Ninja -DCMAKE_MAKE_PROGRAM={} -DGSE_SDK_DIR={}", quoted(plan.cmake), consumer_dir, verify_dir, quoted(plan.ninja), quoted(plan.stage));
	if (run_command(stream, plan, configure, plan.stage) != 0) {
		return std::unexpected("the SDK consumer did not configure against the image");
	}

	spawn::emit(stream, "verify: building the SDK consumer");
	const std::string build = std::format("{} --build {}", quoted(plan.cmake), verify_dir);
	if (run_command(stream, plan, build, plan.stage) != 0) {
		return std::unexpected("the SDK consumer did not build against the image");
	}

	const std::filesystem::path exe = work / consumer_exe;
	spawn::begin_transcript(stream);
	const int code = run_command(stream, plan, quoted(exe), work);
	const std::vector<std::string> output = spawn::take_transcript(stream);
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

auto gse::ide::package_sdk::write_setup(const image_plan& plan, spawn::output_stream& stream) -> std::expected<std::filesystem::path, std::string> {
	if (plan.cmake.empty()) {
		return std::unexpected("the build tree's CMakeCache.txt does not name cmake");
	}
	spawn::emit(stream, "setup: building the installer and its setup stub");
	const std::string build = std::format("{} --build {} --target {} {}", quoted(plan.cmake), quoted(plan.build_dir), installer_target, setup_target);
	if (run_command(stream, plan, build, plan.build_dir) != 0) {
		return std::unexpected("the installer or its setup stub did not build");
	}
	if (const auto copied = fs::copy_file(plan.build_dir / installer_exe, plan.stage / "Bin" / std::filesystem::path(installer_exe).filename()); !copied) {
		return std::unexpected(copied.error());
	}

	const std::string version = gse::config::manifest_value(fs::read_text(plan.stage / "gse.manifest"), "version");
	const std::string preset = plan.stage.filename().generic_native_encoded_string();
	const std::filesystem::path setup = plan.stage.parent_path() / std::format("GSEngine-{}-{}-setup.exe", version, preset);
	if (const auto copied = fs::copy_file(plan.build_dir / setup_exe, setup); !copied) {
		return std::unexpected(copied.error());
	}
	const auto packed = sdk::append_pack(plan.stage, setup, version, preset);
	if (!packed) {
		return std::unexpected(packed.error());
	}
	std::error_code ec;
	spawn::emit(stream, std::format("setup:     {} files packed -> {} ({:.1f} MB)", packed->entries.size(), setup.generic_display_string(), static_cast<double>(std::filesystem::file_size(setup, ec)) / 1048576.0));
	return setup;
}

auto gse::ide::package_system::run(context& ctx, data& d, const channel_read<package_sdk::request> requests_in, const channel_write<build_runner::stream_opened> events_out, const shared_view<build_runner::data> build_d) -> async::task<> {
	if (const auto finished = d.job.take()) {
		if (*finished) {
			log::println(log::level::info, log::category::task, "engine sdk image staged at {}", (*finished)->generic_display_string());
		}
		else {
			log::println(log::level::error, log::category::task, "engine sdk packaging failed: {}", finished->error());
		}
		if (!d.inbox_id.empty()) {
			build_inbox::publish({
				.id = d.inbox_id,
				.outcome = *finished ? build_inbox::status::ok : build_inbox::status::failed,
				.lines = { *finished ? "image " + (*finished)->generic_display_string() : finished->error() },
			});
			d.inbox_id.clear();
		}
	}

	bool requested = !requests_in.of<package_sdk::request>().empty();
	std::string inbox_id;
	for (const build_inbox::package_request& incoming : build_inbox::peek_package_requests()) {
		if (!package_sdk::owns_request(incoming)) {
			continue;
		}
		build_inbox::consume_package_request(incoming.id);
		if (d.job.active() || build_d.building || !inbox_id.empty()) {
			build_inbox::publish({
				.id = incoming.id,
				.outcome = build_inbox::status::rejected,
				.lines = { d.job.active() ? "an SDK package is already being staged; ask again when it finishes" : "a build is running; package once it finishes" },
			});
			continue;
		}
		inbox_id = incoming.id;
		requested = true;
	}

	if (!requested || d.job.active()) {
		return {};
	}
	if (build_d.building) {
		log::println(log::level::warning, log::category::task, "package sdk: wait for the running build to finish first");
		return {};
	}
	d.inbox_id = std::move(inbox_id);

	const config::worktree& tree = config::primary();
	auto stream = std::make_shared<spawn::output_stream>();
	stream->running.store(true, std::memory_order_release);
	events_out.push<build_runner::stream_opened>({
		.name = "Package SDK (" + tree.name + ")",
		.kind = build_runner::stream_kind::package_sdk,
		.stream = stream,
	});

	d.job.start([plan = package_sdk::plan_for(tree), stream] -> std::expected<std::filesystem::path, std::string> {
		const auto result = package_sdk::stage_image(plan, *stream).and_then([&plan, &stream] {
			return package_sdk::verify_image(plan, *stream);
		}).and_then([&plan, &stream] {
			return package_sdk::write_setup(plan, *stream);
		});
		spawn::emit(*stream, result ? "SDK image ready: " + plan.stage.generic_display_string() + ", setup " + result->generic_display_string() : "SDK packaging failed: " + result.error());
		spawn::close_process(*stream);
		if (!result) {
			return std::unexpected(result.error());
		}
		return plan.stage;
	}, trace_id<"package_sdk::stage">(), task::lane::background);
	return {};
}

auto gse::ide::package_system::shutdown(data& d) -> void {
	d.job.cancel();
	if (!d.inbox_id.empty()) {
		build_inbox::publish({
			.id = d.inbox_id,
			.outcome = build_inbox::status::aborted,
			.lines = { "the editor shut down before the SDK image was staged" },
		});
	}
}
