export module gse.ide.build:package_sdk;

import gse;
import gse.ide.config;
import gse.win32.environment;
import std;

import :build_runner;
import :inbox;
import :spawn;

export namespace gse::ide {
	namespace package_sdk {
		struct request {};

		struct launch_plan {
			sdk::pack_kind kind = sdk::pack_kind::sdk;
			std::filesystem::path engine_root;
			std::filesystem::path build_dir;
			std::filesystem::path stage;
			std::filesystem::path compiler_bin;
			std::filesystem::path cmake;
			std::string scenario;
		};

		auto plan_for(
			const config::worktree& tree,
			sdk::pack_kind kind,
			std::string_view scenario
		) -> launch_plan;

		auto run_packer(
			const launch_plan& plan,
			spawn::output_stream& stream
		) -> std::expected<std::filesystem::path, std::string>;
	}

	namespace package_system {
		struct [[= system_state<"Package SDK">{}]] data {
			task::pending<std::expected<std::filesystem::path, std::string>> job;
			std::string inbox_id;
			sdk::pack_kind kind = sdk::pack_kind::sdk;
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
	constexpr std::string_view packer_target = "Packer";
	constexpr std::string_view packer_exe = "Packer/Packer.exe";

	auto quoted(
		const std::filesystem::path& path
	) -> std::string;

	auto run_command(
		spawn::output_stream& stream,
		const launch_plan& plan,
		const std::string& command
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

auto gse::ide::package_sdk::plan_for(const config::worktree& tree, const sdk::pack_kind kind, const std::string_view scenario) -> launch_plan {
	const std::filesystem::path build_dir = build_runner::find_build_dir(tree.project_build);
	return {
		.kind = kind,
		.engine_root = tree.engine_root,
		.build_dir = build_dir,
		.stage = tree.engine_root / "dist" / (kind == sdk::pack_kind::sdk ? "engine-sdk" : "game") / build_dir.filename(),
		.compiler_bin = build_runner::compiler_bin_dir(tree.project_build),
		.cmake = build_runner::cache_value(build_dir, "CMAKE_COMMAND"),
		.scenario = std::string(scenario),
	};
}

auto gse::ide::package_sdk::quoted(const std::filesystem::path& path) -> std::string {
	return "\"" + path.generic_native_encoded_string() + "\"";
}

auto gse::ide::package_sdk::run_command(spawn::output_stream& stream, const launch_plan& plan, const std::string& command) -> int {
	spawn::emit(stream, "> " + command);
	return spawn::run_capture(stream, win32::widen(command), plan.build_dir.wstring(), plan.compiler_bin);
}

auto gse::ide::package_sdk::run_packer(const launch_plan& plan, spawn::output_stream& stream) -> std::expected<std::filesystem::path, std::string> {
	if (plan.build_dir.empty()) {
		return std::unexpected("the project's build tree is not configured; build the game once first");
	}
	if (plan.cmake.empty()) {
		return std::unexpected("the build tree's CMakeCache.txt does not name cmake");
	}

	spawn::emit(stream, "packer: building the packaging tool");
	if (run_command(stream, plan, std::format("{} --build {} --target {}", quoted(plan.cmake), quoted(plan.build_dir), packer_target)) != 0) {
		return std::unexpected("the Packer tool did not build");
	}

	std::string command = std::format(
		"{} --build-dir {} --engine-root {} --stage {} --kind {}",
		quoted(plan.build_dir / packer_exe),
		quoted(plan.build_dir),
		quoted(plan.engine_root),
		quoted(plan.stage),
		enum_to_string(plan.kind)
	);
	if (!plan.scenario.empty()) {
		command += " --scenario " + plan.scenario;
	}
	if (run_command(stream, plan, command) != 0) {
		return std::unexpected(std::format("packaging the {} image failed; the transcript above says why", enum_to_string(plan.kind)));
	}
	return plan.stage;
}

auto gse::ide::package_system::run(context& ctx, data& d, const channel_read<package_sdk::request> requests_in, const channel_write<build_runner::stream_opened> events_out, const shared_view<build_runner::data> build_d) -> async::task<> {
	if (const auto finished = d.job.take()) {
		if (*finished) {
			log::println(log::level::info, log::category::task, "{} image staged at {}", enum_to_string(d.kind), (*finished)->generic_display_string());
		}
		else {
			log::println(log::level::error, log::category::task, "{} packaging failed: {}", enum_to_string(d.kind), finished->error());
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

	bool requested = false;
	sdk::pack_kind kind = sdk::pack_kind::sdk;
	std::string scenario;
	for (const auto& _ : requests_in.of<package_sdk::request>()) {
		requested = true;
	}
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
				.lines = { d.job.active() ? "a package is already being staged; ask again when it finishes" : "a build is running; package once it finishes" },
			});
			continue;
		}
		inbox_id = incoming.id;
		kind = incoming.kind;
		scenario = incoming.scenario;
		requested = true;
	}

	if (!requested || d.job.active()) {
		return {};
	}
	if (build_d.building) {
		log::println(log::level::warning, log::category::task, "package: wait for the running build to finish first");
		return {};
	}
	d.inbox_id = std::move(inbox_id);
	d.kind = kind;

	const config::worktree& tree = config::primary();
	auto stream = std::make_shared<spawn::output_stream>();
	stream->running.store(true, std::memory_order_release);
	events_out.push<build_runner::stream_opened>({
		.name = std::format("Package {} ({})", enum_to_string(kind), tree.name),
		.kind = build_runner::stream_kind::package_sdk,
		.stream = stream,
	});

	d.job.start([plan = package_sdk::plan_for(tree, kind, scenario), stream] -> std::expected<std::filesystem::path, std::string> {
		const auto result = package_sdk::run_packer(plan, *stream);
		spawn::emit(*stream, result ? "image ready: " + result->generic_display_string() : "packaging failed: " + result.error());
		spawn::close_process(*stream);
		return result;
	}, trace_id<"package_sdk::stage">(), task::lane::background);
	return {};
}

auto gse::ide::package_system::shutdown(data& d) -> void {
	d.job.cancel();
	if (!d.inbox_id.empty()) {
		build_inbox::publish({
			.id = d.inbox_id,
			.outcome = build_inbox::status::aborted,
			.lines = { "the editor shut down before the image was staged" },
		});
	}
}
