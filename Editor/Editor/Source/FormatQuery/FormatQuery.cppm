export module gse.ide.format_query;

import gse;
import gse.format;
import gse.ide.app;
import gse.ide.build;
import gse.ide.config;
import gse.ide.workspace;
import std;

export namespace gse::ide::format_query_system {
	struct [[= system_state<"Format Query">{}]] data {
		interval_timer<> poll{ milliseconds(100.f) };
	};

	[[= system_run<>{}, = runs_after<^^workspace_system::data>{}]]
	auto run(
		context& ctx,
		data& d,
		shared_view<workspace_system::data> workspace_d,
		shared_view<config_system::data> config_d
	) -> async::task<>;
}

namespace gse::ide {
	struct source_text {
		std::vector<std::string> lines;
		bool crlf = false;
	};

	auto read_source(
		const std::filesystem::path& path
	) -> std::optional<source_text>;

	auto write_source(
		const std::filesystem::path& path,
		const source_text& text
	) -> bool;

	auto open_and_unsaved(
		const workspace::data& ws,
		const std::filesystem::path& path
	) -> bool;

	auto answer_format_request(
		const workspace::data& ws,
		const format::options& opts,
		const build_inbox::format_request& request
	) -> std::vector<std::string>;
}
