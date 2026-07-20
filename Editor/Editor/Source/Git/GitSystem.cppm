export module gse.ide.git:git_system;

import std;
import gse;

import :git_status;

export namespace gse::ide::git_system {
	struct refresh_request {};

	struct [[= gse::system_state<"Git">{}]] data {
		std::shared_ptr<git::status_check> pending;
		std::filesystem::path repo_root;
		gse::clock clock;
		bool started = false;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d
	) -> gse::async::task<>;
}

auto gse::ide::git_system::run(gse::context& ctx, data& d) -> gse::async::task<> {
	if (d.pending && d.pending->done.load(std::memory_order_acquire)) {
		ctx.channels.push<git::status_updated>({
			.status = std::move(d.pending->result),
		});
		d.pending.reset();
	}

	bool refresh = !d.started;
	for ([[maybe_unused]] const refresh_request& req : ctx.read_channel<refresh_request>()) {
		refresh = true;
	}
	if (d.clock.elapsed() >= gse::seconds(2.f)) {
		refresh = true;
	}

	if (refresh && !d.pending) {
		if (d.repo_root.empty()) {
			d.repo_root = git::find_repo_root(gse::config::root_dir);
		}
		d.pending = std::make_shared<git::status_check>();
		git::status_runner::start(d.pending, d.repo_root);
		d.started = true;
		d.clock.reset();
	}

	return {};
}
