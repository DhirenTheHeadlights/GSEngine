export module gse.ide.git:git_system;

import gse;
import gse.ide.analysis;
import gse.ide.config;
import std;

import :git_status;

export namespace gse::ide::git_system {
	struct refresh_request {};

	struct action_request;

	struct action_inputs {
		git::status_snapshot status;
		std::span<const std::filesystem::path> rootless;
	};

	auto build_initialize(
		const action_inputs& inputs,
		const action_request& request
	) -> std::optional<git::command>;

	auto build_commit(
		const action_inputs& inputs,
		const action_request& request
	) -> std::optional<git::command>;

	auto build_push(
		const action_inputs& inputs,
		const action_request& request
	) -> std::optional<git::command>;

	struct action_info {
		std::optional<git::command> (*build)(const action_inputs&, const action_request&) = nullptr;
	};

	enum class action {
		initialize [[= action_info{
			.build = build_initialize,
		}]],
		commit [[= action_info{
			.build = build_commit,
		}]],
		push [[= action_info{
			.build = build_push,
		}]],
	};

	struct action_request {
		std::filesystem::path root;
		action kind = action::initialize;
		std::string message;
		std::vector<std::filesystem::path> paths;
	};

	struct [[= system_state<"Git">{}]] data {
		task::pending<std::vector<git::repository_result>> status_query;
		task::pending<git::command_result> action;
		std::string action_error;
		std::vector<std::filesystem::path> repo_roots;
		std::vector<std::filesystem::path> rootless;
		git::status_snapshot status;
		clock refresh_clock;
		file_watcher repo_watcher;
		std::vector<std::filesystem::path> watched_repos;
		bool refresh_requested = true;
	};

	[[= system_run<>{}]]
	auto run(
		context& ctx,
		data& d,
		channel_read<action_request, refresh_request> requests_in,
		channel_write<git::status_updated> status_out
	) -> async::task<>;
}

namespace gse::ide::git_system {
	struct discovery {
		std::vector<std::filesystem::path> repositories;
		std::vector<std::filesystem::path> rootless;
	};

	auto discover_repositories() -> discovery;

	auto apply_results(
		data& d,
		std::vector<git::repository_result> results
	) -> bool;

	auto publish(
		const data& d,
		channel_write<git::status_updated> status_out
	) -> void;
}

auto gse::ide::git_system::discover_repositories() -> discovery {
	discovery found;
	std::unordered_set<id> seen;
	for (const config::browse_root& browse : config::browse_roots()) {
		if (!browse.analyzable) {
			continue;
		}
		const git::repo_discovery repo_root = git::find_repo_root(browse.path);
		if (!repo_root) {
			log::println(
				log::level::warning,
				log::category::task,
				"git repository discovery failed for {}: {}",
				browse.path,
				repo_root.error()
			);
			continue;
		}
		if (!*repo_root) {
			found.rootless.push_back(browse.path);
			continue;
		}
		if (seen.insert(generate_temp_id(**repo_root)).second) {
			found.repositories.push_back(**repo_root);
		}
	}
	return found;
}

auto gse::ide::git_system::apply_results(data& d, std::vector<git::repository_result> results) -> bool {
	git::status_map next = d.status ? *d.status : git::status_map{};
	const std::size_t previous_count = next.repository_count();
	next.retain(d.repo_roots);
	bool changed = next.repository_count() != previous_count;

	for (git::repository_result& result : results) {
		if (result) {
			next.replace(std::move(*result));
			changed = true;
		}
		else {
			log::println(
				log::level::warning,
				log::category::task,
				"git status failed for {}: {}",
				result.error().root,
				result.error().message
			);
		}
	}
	if (!changed) {
		return false;
	}
	d.status = std::make_shared<const git::status_map>(std::move(next));
	return true;
}

auto gse::ide::git_system::publish(const data& d, const channel_write<git::status_updated> status_out) -> void {
	status_out.push<git::status_updated>({
		.status = d.status,
		.rootless = d.rootless,
		.busy = d.action.active(),
		.action_error = d.action_error,
	});
}

auto gse::ide::git_system::build_initialize(const action_inputs& inputs, const action_request& request) -> std::optional<git::command> {
	if (std::ranges::find(inputs.rootless, request.root) == inputs.rootless.end()) {
		return std::nullopt;
	}
	return git::init_command(request.root);
}

auto gse::ide::git_system::build_commit(const action_inputs&, const action_request& request) -> std::optional<git::command> {
	if (request.paths.empty()) {
		return std::nullopt;
	}
	const std::filesystem::path scratch = analysis::process::temporary_path("git_commit", "txt");
	std::ofstream out(scratch, std::ios::binary);
	out << request.message;
	if (!out) {
		return std::nullopt;
	}
	git::command command{
		.root = request.root,
		.scratch = scratch,
	};
	constexpr std::size_t paths_per_step = 64;
	for (std::size_t start = 0; start < request.paths.size(); start += paths_per_step) {
		std::string step = "git add -A --";
		for (const std::filesystem::path& path : std::span(request.paths).subspan(start, std::min(paths_per_step, request.paths.size() - start))) {
			step += std::format(" \"{}\"", path.generic_display_string());
		}
		command.steps.push_back(std::move(step));
	}
	command.steps.push_back(std::format("git commit -F \"{}\"", scratch.generic_display_string()));
	return command;
}

auto gse::ide::git_system::build_push(const action_inputs& inputs, const action_request& request) -> std::optional<git::command> {
	const git::repository_snapshot repository = inputs.status->find(request.root);
	if (!repository) {
		return std::nullopt;
	}
	return git::command{
		.root = request.root,
		.steps = { repository->upstream.empty() ? std::format("git push -u origin {}", repository->branch) : std::string("git push") },
	};
}

auto gse::ide::git_system::run(context& ctx, data& d, const channel_read<action_request, refresh_request> requests_in, const channel_write<git::status_updated> status_out) -> async::task<> {
	if (std::optional<std::vector<git::repository_result>> results = d.status_query.take()) {
		if (apply_results(d, std::move(*results))) {
			publish(d, status_out);
		}
	}

	if (std::optional<git::command_result> finished = d.action.take()) {
		if (!finished->outcome) {
			d.action_error = std::move(finished->outcome.error());
			log::println(
				log::level::error,
				log::category::task,
				"git action failed for {}: {}",
				finished->root,
				d.action_error
			);
		}
		d.refresh_requested = true;
		publish(d, status_out);
	}

	for (const action_request& request : requests_in.of<action_request>()) {
		if (d.action.active()) {
			continue;
		}
		std::optional<git::command> command = annotation_from_enum<action_info>(request.kind, {}).build({
			.status = d.status,
			.rootless = d.rootless,
		}, request);
		if (!command) {
			continue;
		}
		d.action_error.clear();
		d.action.start([job = std::move(*command)]() mutable {
			return git::run_command(std::move(job));
		}, trace_id<"git::command">(), task::lane::background);
		publish(d, status_out);
	}

	for ([[maybe_unused]] const refresh_request& _ : requests_in.of<refresh_request>()) {
		d.refresh_requested = true;
	}
	if (d.repo_watcher.poll() > 0) {
		d.refresh_requested = true;
	}
	if (d.refresh_clock.elapsed() >= seconds(30.f)) {
		d.refresh_requested = true;
	}

	if (d.refresh_requested && !d.status_query.active()) {
		discovery found = discover_repositories();
		const bool rootless_changed = found.rootless != d.rootless;
		d.rootless = std::move(found.rootless);
		d.repo_roots = std::move(found.repositories);

		if (d.watched_repos != d.repo_roots) {
			d.repo_watcher.clear();
			d.watched_repos = d.repo_roots;
			for (const std::filesystem::path& root : d.watched_repos) {
				const std::filesystem::path git_dir = git::git_dir_of(root);
				for (const std::string_view name : { "HEAD", "index" }) {
					d.repo_watcher.watch(git_dir / name, [&d](const std::filesystem::path&) {
						d.refresh_requested = true;
					});
				}
			}
		}

		const bool cleared = d.repo_roots.empty() && d.status && !d.status->empty();
		if (cleared) {
			d.status = std::make_shared<const git::status_map>();
		}
		if (cleared || rootless_changed) {
			publish(d, status_out);
		}
		if (!d.repo_roots.empty()) {
			d.status_query.start([roots = d.repo_roots] {
				return git::query_repositories(roots);
			}, trace_id<"git::status">(), task::lane::background);
		}
		d.refresh_requested = false;
		d.refresh_clock.reset();
	}

	return {};
}