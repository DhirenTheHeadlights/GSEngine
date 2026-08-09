export module gse.ide.git:git_system;

import std;
import gse;
import gse.ide.config;

import :git_status;

export namespace gse::ide::git_system {
	struct refresh_request {};

	struct init_request {
		std::filesystem::path root;
	};

	struct [[= system_state<"Git">{}]] data {
		std::shared_ptr<git::status_check> pending;
		std::shared_ptr<git::init_check> initializing;
		std::vector<std::filesystem::path> repo_roots;
		std::vector<std::filesystem::path> rootless;
		git::status_snapshot status;
		clock refresh_clock;
		bool refresh_requested = true;
	};

	[[= system_run<>{}]]
	auto run(
		context& ctx,
		data& d,
		channel_read<init_request, refresh_request> requests_in,
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

auto gse::ide::git_system::run(context& ctx, data& d, const channel_read<init_request, refresh_request> requests_in, const channel_write<git::status_updated> status_out) -> async::task<> {
	if (d.pending && d.pending->done.load(std::memory_order_acquire)) {
		if (apply_results(d, std::move(d.pending->results))) {
			status_out.push<git::status_updated>({
				.status = d.status,
				.rootless = d.rootless,
			});
		}
		d.pending.reset();
	}

	if (d.initializing && d.initializing->done.load(std::memory_order_acquire)) {
		if (!d.initializing->result) {
			log::println(
				log::level::error,
				log::category::task,
				"git init failed for {}: {}",
				d.initializing->root,
				d.initializing->result.error()
			);
		}
		d.initializing.reset();
		d.refresh_requested = true;
	}

	for (const init_request& request : requests_in.of<init_request>()) {
		if (!d.initializing && std::ranges::find(d.rootless, request.root) != d.rootless.end()) {
			d.initializing = std::make_shared<git::init_check>();
			git::init_runner::start(d.initializing, request.root);
		}
	}

	for ([[maybe_unused]] const refresh_request& request : requests_in.of<refresh_request>()) {
		d.refresh_requested = true;
	}
	if (d.refresh_clock.elapsed() >= seconds(2.f)) {
		d.refresh_requested = true;
	}

	if (d.refresh_requested && !d.pending) {
		discovery found = discover_repositories();
		const bool rootless_changed = found.rootless != d.rootless;
		d.rootless = std::move(found.rootless);
		d.repo_roots = std::move(found.repositories);

		const bool cleared = d.repo_roots.empty() && d.status && !d.status->empty();
		if (cleared) {
			d.status = std::make_shared<const git::status_map>();
		}
		if (cleared || rootless_changed) {
			status_out.push<git::status_updated>({
				.status = d.status,
				.rootless = d.rootless,
			});
		}
		if (!d.repo_roots.empty()) {
			d.pending = std::make_shared<git::status_check>();
			git::status_runner::start(d.pending, d.repo_roots);
		}
		d.refresh_requested = false;
		d.refresh_clock.reset();
	}

	return {};
}
