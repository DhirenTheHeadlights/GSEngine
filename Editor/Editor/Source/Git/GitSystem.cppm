export module gse.ide.git:git_system;

import gse;
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
		std::filesystem::path action_error_root;
		git_system::action action_error_kind = git_system::action::initialize;
		git_system::action pending_kind = git_system::action::initialize;
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

auto gse::ide::git_system::build_commit(const action_inputs& inputs, const action_request& request) -> std::optional<git::command> {
	if (request.paths.empty()) {
		return std::nullopt;
	}
	std::unordered_set<std::string> already_staged;
	std::unordered_map<std::string, std::string> rename_sources;
	if (const git::repository_snapshot repository = inputs.status->find(request.root)) {
		for (const git::change& change : repository->changes) {
			std::string relative = change.relative.generic_display_string();
			if (!change.original.empty()) {
				rename_sources.emplace(relative, change.original.generic_display_string());
			}
			if (!change.needs_stage) {
				already_staged.insert(std::move(relative));
			}
		}
	}
	std::string pathspecs;
	std::string committed;
	for (const std::filesystem::path& path : request.paths) {
		std::string relative = path.generic_display_string();
		if (const auto source = rename_sources.find(relative); source != rename_sources.end()) {
			committed += source->second;
			committed.push_back('\0');
		}
		if (!already_staged.contains(relative)) {
			pathspecs += relative;
			pathspecs.push_back('\0');
		}
		committed += std::move(relative);
		committed.push_back('\0');
	}

	const std::filesystem::path message_path = process::temporary_path("git_commit", "txt");
	std::ofstream message_out(message_path, std::ios::binary);
	message_out << request.message;
	message_out.close();
	if (!message_out) {
		return std::nullopt;
	}
	git::command command{
		.root = request.root,
		.scratch = { message_path },
	};
	if (!pathspecs.empty()) {
		const std::filesystem::path pathspec_path = process::temporary_path("git_pathspec", "txt");
		std::ofstream pathspec_out(pathspec_path, std::ios::binary);
		pathspec_out.write(pathspecs.data(), static_cast<std::streamsize>(pathspecs.size()));
		pathspec_out.close();
		if (!pathspec_out) {
			return std::nullopt;
		}
		command.scratch.push_back(pathspec_path);
		command.steps.push_back(std::format("git add -A --pathspec-from-file=\"{}\" --pathspec-file-nul", pathspec_path.generic_display_string()));
	}
	if (!git::partial_commit_allowed(request.root)) {
		log::println(
			log::level::warning,
			log::category::task,
			"git: {} has a merge in progress, so the commit records the whole index rather than the selected files",
			request.root
		);
		command.steps.push_back(std::format("git commit -F \"{}\"", message_path.generic_display_string()));
		return command;
	}
	const std::filesystem::path committed_path = process::temporary_path("git_committed", "txt");
	std::ofstream committed_out(committed_path, std::ios::binary);
	committed_out.write(committed.data(), static_cast<std::streamsize>(committed.size()));
	committed_out.close();
	if (!committed_out) {
		return std::nullopt;
	}
	command.scratch.push_back(committed_path);
	command.steps.push_back(std::format(
		"git commit -F \"{}\" --pathspec-from-file=\"{}\" --pathspec-file-nul",
		message_path.generic_display_string(),
		committed_path.generic_display_string()
	));
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
			d.action_error_root = finished->root;
			d.action_error_kind = d.pending_kind;
			log::println(
				log::level::error,
				log::category::task,
				"git action failed for {}: {}",
				finished->root,
				d.action_error
			);
		}
		else if (d.action_error_root == finished->root && d.action_error_kind == d.pending_kind) {
			d.action_error.clear();
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
		d.pending_kind = request.kind;
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