export module gse.ide.panel_query;

import gse;
import gse.ide.agent;
import gse.ide.alloc;
import gse.ide.analysis;
import gse.ide.app;
import gse.ide.build;
import gse.ide.diagnostic;
import gse.ide.git;
import gse.ide.graph;
import gse.ide.highlight;
import gse.ide.lint_panel;
import gse.ide.problems;
import gse.ide.profile;
import gse.ide.search;
import gse.ide.search_panel;
import gse.ide.source_control;
import gse.ide.terminal;
import gse.ide.workspace;
import std;

export namespace gse::ide {
	struct panel_sources {
		shared_view<editor_app::data> app;
		shared_view<workspace_system::data> workspace;
		shared_view<terminal::data> term;
		shared_view<agent::data> chats;
		shared_view<build_runner::data> build;
		shared_view<profile_system::data> profile;
		shared_view<search_system::data> search;
	};

	struct panel_writer {
		void (*write)(const panel_sources&, std::uint32_t, std::vector<std::string>&) = nullptr;
	};

	struct panel_report {
		std::string_view name;
		std::string_view summary;
		panel_writer writer;
		bool docked = true;
	};

	[[nodiscard]] auto panel_reports() -> std::span<const panel_report>;
}

export namespace gse::ide::panel_query_system {
	struct [[= system_state<"Panel Query">{}]] data {
		interval_timer<> poll{ milliseconds(100.f) };
	};

	[[= system_run<>{}, = runs_after<^^workspace_system::data>{}]]
	auto run(
		context& ctx,
		data& d,
		shared_view<editor_app::data> app_d,
		shared_view<workspace_system::data> workspace_d,
		shared_view<terminal::data> terminal_d,
		shared_view<agent::data> agent_d,
		shared_view<build_runner::data> build_d,
		shared_view<profile_system::data> profile_d,
		shared_view<search_system::data> search_d
	) -> async::task<>;
}

namespace gse::ide {
	constexpr std::uint32_t default_report_rows = 50;

	auto field(
		std::vector<std::string>& out,
		std::string_view key,
		std::string_view value
	) -> void;

	auto count(
		std::vector<std::string>& out,
		std::string_view key,
		std::size_t value
	) -> void;

	auto section(
		std::vector<std::string>& out,
		std::string_view name
	) -> void;

	auto item(
		std::vector<std::string>& out
	) -> void;

	auto report_for(
		std::string_view name
	) -> const panel_report*;

	auto panel_visibility(
		const panel_sources& src,
		const panel_report& entry
	) -> std::string_view;

	auto answer_panel_query(
		const panel_sources& src,
		const build_inbox::panel_query& query
	) -> std::vector<std::string>;

	auto write_catalogue(
		const panel_sources& src,
		std::uint32_t rows,
		std::vector<std::string>& out
	) -> void;

	auto write_explorer(
		const panel_sources& src,
		std::uint32_t rows,
		std::vector<std::string>& out
	) -> void;

	auto write_code(
		const panel_sources& src,
		std::uint32_t rows,
		std::vector<std::string>& out
	) -> void;

	auto write_problems(
		const panel_sources& src,
		std::uint32_t rows,
		std::vector<std::string>& out
	) -> void;

	auto write_search(
		const panel_sources& src,
		std::uint32_t rows,
		std::vector<std::string>& out
	) -> void;

	auto write_lints(
		const panel_sources& src,
		std::uint32_t rows,
		std::vector<std::string>& out
	) -> void;

	auto write_source_control(
		const panel_sources& src,
		std::uint32_t rows,
		std::vector<std::string>& out
	) -> void;

	auto write_terminal(
		const panel_sources& src,
		std::uint32_t rows,
		std::vector<std::string>& out
	) -> void;

	auto write_agent(
		const panel_sources& src,
		std::uint32_t rows,
		std::vector<std::string>& out
	) -> void;

	auto write_game(
		const panel_sources& src,
		std::uint32_t rows,
		std::vector<std::string>& out
	) -> void;

	auto write_profile(
		const panel_sources& src,
		std::uint32_t rows,
		std::vector<std::string>& out
	) -> void;

	auto write_alloc(
		const panel_sources& src,
		std::uint32_t rows,
		std::vector<std::string>& out
	) -> void;

	auto write_graph(
		const panel_sources& src,
		std::uint32_t rows,
		std::vector<std::string>& out
	) -> void;

	auto write_quick_search(
		const panel_sources& src,
		std::uint32_t rows,
		std::vector<std::string>& out
	) -> void;
}
