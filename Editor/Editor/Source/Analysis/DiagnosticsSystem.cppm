export module gse.ide.analysis:diagnostics_system;

import std;
import gse;

import :diagnostics_runner;

export namespace gse::ide {
	namespace analysis {
		struct diagnostics_request {
			gse::id document_id;
			document_revision revision;
			std::filesystem::path compile_commands;
			std::filesystem::path file;
			std::filesystem::path plugin;
			std::vector<std::filesystem::path> workspace_roots;
			void (*lint_hook)(diagnostics_check&) = nullptr;
		};

		struct diagnostics_completed {
			std::shared_ptr<diagnostics_check> check;
		};
	}

	namespace diagnostics_system {
		struct [[= gse::system_state<"Diagnostics">{}]] data {
			std::shared_ptr<analysis::diagnostics_check> pending;
			std::jthread worker;
		};

		[[= gse::system_run<>{}]]
		auto run(
			gse::context& ctx,
			data& d
		) -> gse::async::task<>;
	}
}

auto gse::ide::diagnostics_system::run(gse::context& ctx, data& d) -> gse::async::task<> {
	if (d.pending && d.pending->done.load(std::memory_order_acquire)) {
		ctx.channels.push<analysis::diagnostics_completed>({
			.check = std::move(d.pending),
		});
	}

	if (d.pending) {
		return {};
	}

	std::optional<analysis::diagnostics_request> request;
	for (const analysis::diagnostics_request& next : ctx.read_channel<analysis::diagnostics_request>()) {
		request = next;
	}
	if (!request) {
		return {};
	}

	d.pending = std::make_shared<analysis::diagnostics_check>();
	d.pending->document_id = request->document_id;
	d.pending->revision = request->revision;
	d.worker = analysis::diagnostics_runner::start(
		d.pending,
		request->compile_commands,
		request->file,
		request->plugin,
		request->workspace_roots,
		request->lint_hook
	);
	return {};
}
