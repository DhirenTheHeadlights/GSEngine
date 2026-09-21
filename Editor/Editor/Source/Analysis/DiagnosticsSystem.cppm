export module gse.ide.analysis:diagnostics_system;

import gse.concurrency;
import gse.core;
import gse.ecs;
import std;

import :diagnostics_runner;

export namespace gse::ide {
	namespace analysis {
		struct diagnostics_completed {
			std::shared_ptr<const diagnostics_result> result;
		};
	}

	namespace diagnostics_system {
		struct [[= system_state<"Diagnostics">{}]] data {
			task::pending<analysis::diagnostics_result> analysis_job;
		};

		[[= system_run<>{}]]
		auto run(
			context& ctx,
			data& d,
			channel_read<analysis::diagnostics_request> requests_in,
			channel_write<analysis::diagnostics_completed> completed_out
		) -> async::task<>;

		[[= system_shutdown{}]]
		auto shutdown(
			data& d
		) -> void;
	}
}

auto gse::ide::diagnostics_system::run(context& ctx, data& d, const channel_read<analysis::diagnostics_request> requests_in, const channel_write<analysis::diagnostics_completed> completed_out) -> async::task<> {
	if (std::optional<analysis::diagnostics_result> finished = d.analysis_job.take()) {
		completed_out.push<analysis::diagnostics_completed>({
			.result = std::make_shared<const analysis::diagnostics_result>(std::move(*finished)),
		});
	}

	if (d.analysis_job.active()) {
		return {};
	}

	std::optional<analysis::diagnostics_request> request;
	for (const analysis::diagnostics_request& next : requests_in.of<analysis::diagnostics_request>()) {
		request = next;
	}
	if (!request) {
		return {};
	}

	d.analysis_job.start([job = std::move(*request)](const std::stop_token& stop) {
		return analysis::run_diagnostics(job, stop);
	}, trace_id<"analysis::diagnostics">(), task::lane::background);
	return {};
}

auto gse::ide::diagnostics_system::shutdown(data& d) -> void {
	d.analysis_job.cancel();
}