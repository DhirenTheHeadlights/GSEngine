export module gse.ide.agent:chrome;

import std;
import gse;

import gse.ide.build;

import :canvas;
import :model;
import :phase;

export namespace gse::ide::agent {
	enum class chrome_action : std::uint8_t {
		none,
		answer,
		approve,
		stop,
	};

	auto fade(
		vec4f color,
		float alpha
	) -> vec4f;

	struct chrome_facts {
		chat_kind kind = chat_kind::chat;
		task_phase phase = task_phase::apply;
		agent_state state = agent_state::idle;
		std::vector<phase_step> steps;
		std::string status;
		std::string activity;
		std::string gate_action;
		std::string pending_next;
		std::string failure;
		std::uint32_t findings = 0;
		std::span<const build_inbox::plan_step> plan_steps;
		std::string_view plan_summary;
		std::optional<std::uint32_t> followup_row;
		build_hold hold = build_hold::none;
		std::string blocker;
		std::size_t holding = 0;
		std::string blame;
		std::string unbuilt;
		std::size_t unclaimed = 0;
		std::string awaited_build;
		std::optional<std::uint32_t> awaited_pid;
		std::string awaited_file;
		time wake_in;
		bool capped = false;
		std::string asleep_blocker;
		std::string woke_by;
		std::string wake_task;
		bool busy = false;
		bool build_waiting = false;
		bool has_gate = false;
		bool pending = false;
		bool asking = false;
		bool goal_waiting = false;
		bool hibernating = false;
		bool stale = false;
		bool failed = false;
		bool summary_ready = false;
	};

	auto failure_note(
		const chrome_facts& facts
	) -> std::string;

	auto pending_note(
		const chrome_facts& facts
	) -> std::string;

	auto findings_note(
		const chrome_facts& facts
	) -> std::string;

	auto blame_note(
		const chrome_facts& facts
	) -> std::string;

	auto unclaimed_note(
		const chrome_facts& facts
	) -> std::string;

	auto unbuilt_note(
		const chrome_facts& facts
	) -> std::string;

	auto asleep_note(
		const chrome_facts& facts
	) -> std::string;

	auto woke_note(
		const chrome_facts& facts
	) -> std::string;

	auto build_wait_clause(
		const chrome_facts& facts
	) -> std::string;

	auto process_wait_clause(
		const chrome_facts& facts
	) -> std::string;

	auto file_wait_clause(
		const chrome_facts& facts
	) -> std::string;

	auto deadline_wait_clause(
		const chrome_facts& facts
	) -> std::string;

	struct wait_style {
		std::string (*text)(const chrome_facts&) = nullptr;
	};

	enum class wait_reason : std::uint8_t {
		build [[= wait_style{ .text = &build_wait_clause }]],
		process [[= wait_style{ .text = &process_wait_clause }]],
		file [[= wait_style{ .text = &file_wait_clause }]],
		deadline [[= wait_style{ .text = &deadline_wait_clause }]],
	};

	struct note_style {
		std::string (*text)(const chrome_facts&) = nullptr;
		vec4f gui::style::* color = &gui::style::color_text_secondary;
	};

	enum class chrome_note : std::uint8_t {
		failure [[= note_style{
			.text = &failure_note,
			.color = &gui::style::color_error,
		}]],
		pending [[= note_style{
			.text = &pending_note,
		}]],
		findings [[= note_style{
			.text = &findings_note,
		}]],
		blame [[= note_style{
			.text = &blame_note,
			.color = &gui::style::color_error,
		}]],
		unclaimed [[= note_style{
			.text = &unclaimed_note,
			.color = &gui::style::color_warning,
		}]],
		unbuilt [[= note_style{
			.text = &unbuilt_note,
			.color = &gui::style::color_warning,
		}]],
		asleep [[= note_style{
			.text = &asleep_note,
		}]],
		woke [[= note_style{
			.text = &woke_note,
		}]],
	};

	struct chrome_tab {
		bool dirty = false;
		bool busy = false;
		bool warning = false;
		bool error = false;
		bool dimmed = false;
	};

	struct chrome_plan {
		std::vector<phase_step> steps;
		std::string status;
		vec4f gui::style::* status_color = &gui::style::color_text_secondary;
		std::string activity;
		vec4f gui::style::* activity_color = &gui::style::color_accent;
		bool activity_shown = false;
		bool overview_activity = false;
		std::string note;
		vec4f gui::style::* note_color = &gui::style::color_text_secondary;
		std::string footer;
		std::string waiting;
		vec4f gui::style::* waiting_color = &gui::style::color_text_secondary;
		std::string wake_task;
		chrome_action action = chrome_action::none;
		std::string action_label;
		bool stop_enabled = false;
		bool stop_interrupts = false;
		bool question_shown = false;
		bool plan_shown = false;
		std::span<const build_inbox::plan_step> plan_steps;
		std::string_view plan_summary;
		bool plan_split = false;
		std::uint32_t followup_row = 0;
		bool fork_allowed = false;
		bool summary_ready = false;
		bool toggle_shown = false;
		chrome_tab tab;
	};

	auto followup_of(
		const session& s
	) -> std::optional<std::uint32_t>;

	enum class page_kind : std::uint8_t {
		summary,
		steps,
		changes,
		review_loop,
		report,
		errors,
		goal,
		verdict,
		empty,
	};

	struct standing_style {
		char label[32] = "pursuing";
		vec4f gui::style::* color = &gui::style::color_accent;
	};

	enum class goal_standing : std::uint8_t {
		pursuing [[= standing_style{
			.label = "pursuing",
			.color = &gui::style::color_accent,
		}]],
		waiting [[= standing_style{
			.label = "waiting",
			.color = &gui::style::color_file,
		}]],
		held [[= standing_style{
			.label = "waiting for your approval",
			.color = &gui::style::color_folder,
		}]],
		exhausted [[= standing_style{
			.label = "steer budget spent",
			.color = &gui::style::color_warning,
		}]],
		met [[= standing_style{
			.label = "met",
			.color = &gui::style::color_added,
		}]],
	};

	struct ledger_entry {
		std::int64_t started = 0;
		goal_verdict verdict = goal_verdict::task;
		std::string_view summary;
	};

	struct goal_dashboard {
		std::filesystem::path record;
		std::string aim;
		goal_standing standing = goal_standing::pursuing;
		std::uint32_t steers_left = 0;
		std::string waiting;
		std::vector<criterion_report> criteria;
		std::vector<phase_step> route;
		std::vector<ledger_entry> ledger;
	};

	auto goal_dashboard_of(
		const session& s
	) -> std::optional<goal_dashboard>;

	auto goal_head(
		const goal_dashboard& goal
	) -> std::string;

	auto criteria_tally(
		std::span<const criterion_report> criteria
	) -> std::string;

	auto route_line(
		std::span<const phase_step> route
	) -> std::string;

	auto ledger_head(
		const ledger_entry& entry
	) -> std::string;

	struct strip_line {
		std::string text;
		vec4f gui::style::* color = &gui::style::color_error;
	};

	auto error_strip(
		const build_fix* fix
	) -> std::vector<strip_line>;

	auto errors_head(
		const build_fix* fix
	) -> std::string;

	struct review_cycle {
		std::string_view found;
		std::uint32_t findings = 0;
		bool reviewed = false;
		std::string_view revised;
		std::size_t edits = 0;
		bool revise_begun = false;
		bool revise_reported = false;
	};

	struct review_loop {
		std::vector<review_cycle> cycles;
		bool capped = false;
	};

	struct summary_page {
		task_phase phase = task_phase::scope;
		page_kind kind = page_kind::empty;
		const phase_run* run = nullptr;
	};

	struct goal_cycle {
		std::span<const phase_run> runs;
		std::int64_t started = 0;
		std::optional<goal_verdict> verdict;
		std::uint32_t findings = 0;
		std::size_t edits = 0;
	};

	auto goal_cycles(
		const session& s
	) -> std::vector<goal_cycle>;

	struct cycle_paging {
		std::uint32_t position = 0;
		std::uint32_t cycles = 0;
		bool shown = false;
	};

	auto cycle_label(
		const cycle_paging& paging
	) -> std::string;

	struct summary_anchor {
		std::uint32_t position = 0;
		std::uint32_t page = 0;
	};

	struct since_digest {
		std::int64_t since = 0;
		std::size_t stages = 0;
		std::size_t verdicts = 0;
		std::size_t questions = 0;
		std::size_t failures = 0;
		bool waiting = false;
		std::optional<summary_anchor> anchor;
	};

	auto since_of(
		const session& s,
		std::int64_t since,
		std::int64_t until
	) -> since_digest;

	auto opening_page(
		const session& s,
		const since_digest& since
	) -> summary_anchor;

	auto digest_line(
		const since_digest& digest
	) -> std::string;

	auto fresh_page(
		const summary_page& page,
		std::int64_t since
	) -> bool;

	auto fork_prompt(
		const session& s,
		const summary_page& page,
		const cycle_paging& cycle,
		std::string_view question
	) -> std::string;

	auto fork_about(
		const summary_page& page,
		const cycle_paging& cycle
	) -> std::string;

	struct plan_paging {
		std::uint32_t page = 1;
		std::uint32_t pages = 1;
		summary_page shown;
	};

	struct step_progress {
		std::vector<step_state> states;
		std::vector<std::size_t> edits;
		std::size_t unattributed = 0;
		bool reported = false;
		bool ended = false;
	};

	struct lifecycle {
		std::string_view summary;
		std::span<const build_inbox::plan_step> steps;
		step_progress progress;
		std::vector<const step_edit*> edits;
		review_loop review;
		std::string_view report;
		const build_fix* fix = nullptr;
		std::optional<goal_dashboard> goal;
		std::optional<ledger_entry> steered;
		cycle_paging cycle;
		std::vector<summary_page> pages;
	};

	auto gate_summary(
		const session& s
	) -> std::string_view;

	auto scope_pages(
		std::string_view summary,
		std::size_t steps
	) -> std::vector<summary_page>;

	auto apply_pages(
		std::size_t steps,
		std::size_t edits
	) -> std::vector<summary_page>;

	auto lifecycle_pages(
		std::span<const phase_step> route,
		std::string_view summary,
		std::size_t steps,
		std::size_t edits,
		std::size_t cycles,
		bool reported,
		bool fixing,
		bool steered
	) -> std::vector<summary_page>;

	auto review_loop_of(
		const session& s,
		std::span<const phase_run> runs
	) -> review_loop;

	auto review_loop_of(
		const session& s
	) -> review_loop;

	auto review_loop_head(
		const review_loop& loop
	) -> std::string;

	auto review_loop_text(
		const review_loop& loop
	) -> std::string;

	auto report_head(
		const step_progress& progress,
		const review_loop& loop
	) -> std::string;

	auto edits_label(
		std::size_t edits
	) -> std::string;

	auto task_edits(
		const lifecycle& life
	) -> std::size_t;

	struct diff_section {
		std::uint32_t row = 0;
		std::string label;
	};

	auto diff_sections_of(
		const session& s
	) -> std::vector<diff_section>;

	auto progress_of(
		std::size_t steps,
		std::span<const step_state> reported,
		std::span<const step_edit* const> edits,
		bool ended
	) -> step_progress;

	auto edits_in_group(
		std::span<const step_edit* const> edits,
		std::size_t steps,
		std::optional<std::size_t> group
	) -> std::vector<const step_edit*>;

	auto lifecycle_at(
		const session& s,
		std::optional<std::uint32_t> position
	) -> lifecycle;

	auto lifecycle_of(
		const session& s
	) -> lifecycle;

	auto shown_goal(
		const session& s
	) -> const goal_state*;

	auto summary_ready(
		const session& s
	) -> bool;

	auto paging_of(
		std::span<const summary_page> pages,
		std::uint32_t chosen
	) -> plan_paging;

	auto facts_of(
		const data& d,
		const session& s
	) -> chrome_facts;

	auto plan_from(
		const chrome_facts& facts
	) -> chrome_plan;

	auto plan_chrome(
		const data& d,
		const session& s
	) -> chrome_plan;

	auto excluded_by(
		chat_kind kind,
		task_phase phase,
		agent_state state
	) -> std::string_view;

	auto draw_phase_pipeline(
		const gui::draw_context& ctx,
		std::span<const phase_step> steps,
		id underline_key,
		const rectf& area,
		const rectf& clip
	) -> float;

	auto draw_activity_line(
		const gui::draw_context& ctx,
		std::string_view activity,
		vec4f gui::style::* color,
		float presence,
		const rectf& area,
		const rectf& clip
	) -> void;

	auto draw_status_row(
		const gui::draw_context& ctx,
		const chrome_plan& plan,
		id key,
		const rectf& area,
		const rectf& clip
	) -> void;
}
