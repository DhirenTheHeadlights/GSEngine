export module gse.ide.agent:phase;

import std;
import gse;

import :model;

namespace gse::ide::agent {
	constexpr std::string_view docs_dir_name = "docs";
	constexpr std::string_view workflow_doc_name = "AGENT_WORKFLOW.md";
	constexpr std::string_view charter_dir_name = "agent";
	constexpr std::string_view scratch_dir_name = "agent";
	constexpr std::string_view sync_charter_name = "sync";
	constexpr std::string_view sync_opening = "The owner pressed Sync in the source control panel. Read the repository's real state and carry the work out now, following your instructions.";

	struct phase_step {
		task_phase phase = task_phase::scope;
		std::uint32_t cycles = 0;
		bool visited = false;
		bool current = false;
	};

	constexpr task_phase phase_order[] = {
		task_phase::scope,
		task_phase::apply,
		task_phase::review,
		task_phase::revise,
		task_phase::summarize,
		task_phase::steer,
		task_phase::settled,
	};

	constexpr std::string_view approve_goal_label = "Approve goal";
	constexpr std::string_view code_shader_extension = ".slang";
	constexpr std::string_view steered_task_opening = "The goal's steering run chose the next task. Scope it now, following your phase instructions.";

	auto current_phase(
		const session& s
	) -> task_phase;

	auto task_runs(
		const session& s
	) -> std::span<const phase_run>;

	auto approved_steps(
		const session& s
	) -> std::span<const build_inbox::plan_step>;

	auto run_section_of(
		const session& s,
		std::uint32_t row
	) -> std::optional<std::string>;

	auto record_step_edit(
		session& s,
		const transcript_row& row
	) -> void;

	auto phase_timeline(
		task_phase now,
		bool goal,
		std::span<const phase_run> task
	) -> std::vector<phase_step>;

	auto phase_timeline(
		const session& s
	) -> std::vector<phase_step>;

	auto policy_of(
		task_phase phase
	) -> phase_policy;

	auto thinking_phrase(
		const session& s
	) -> std::string;

	constexpr std::uint32_t max_review_cycles = 3;

	auto reviews_spent(
		const session& s
	) -> std::uint32_t;

	auto verdict_policy_of(
		goal_verdict verdict
	) -> verdict_policy;

	auto steered_code(
		const session& s
	) -> bool;

	auto next_phase(
		const session& s,
		const phase_gate& reported
	) -> task_phase;

	auto needs_owner(
		const session& s,
		const phase_gate& reported
	) -> bool;

	auto gate_label(
		const session& s,
		const phase_gate& gate
	) -> std::string;

	auto session_scratch(
		const session& s
	) -> std::filesystem::path;

	auto prompt_path(
		const session& s
	) -> std::filesystem::path;

	auto scope_path(
		const session& s
	) -> std::filesystem::path;

	auto report_path(
		const session& s
	) -> std::filesystem::path;

	auto diff_path(
		const session& s
	) -> std::filesystem::path;

	auto touched_path(
		const session& s
	) -> std::filesystem::path;

	auto gaps_path(
		const session& s
	) -> std::filesystem::path;

	auto task_artifacts(
		const session& s
	) -> std::array<std::filesystem::path, 5>;

	constexpr std::string_view change_header = "> ";

	auto changes_path(
		const session& s
	) -> std::filesystem::path;

	auto read_changes(
		const session& s
	) -> std::vector<transcript_row>;

	auto docs_root(
		const session& s
	) -> std::filesystem::path;

	auto read_doc(
		const std::filesystem::path& file
	) -> std::string;

	auto resume_target(
		const session& s
	) -> std::string;

	auto phase_charter(
		const session& s,
		task_phase phase
	) -> std::string;

	auto sync_facts(
		const sync_request& request
	) -> std::string;

	auto write_sync_prompt(
		const session& s
	) -> std::filesystem::path;

	auto write_phase_prompt(
		const session& s
	) -> std::filesystem::path;

	auto track_run_id(
		session& s
	) -> void;

	auto begin_phase(
		session& s,
		task_phase phase,
		const phase_gate& handover
	) -> void;

	auto restart_task(
		session& s
	) -> void;

	auto plan_handoff(
		const session& s,
		task_phase from,
		std::string_view summary
	) -> handoff_capture;

	auto capture_handoff(
		const handoff_capture& planned,
		const std::stop_token& cancel
	) -> bool;

	auto advance_phase(
		session& s,
		std::string_view note
	) -> void;

	auto service_phase(
		session& s,
		std::uint32_t steer_budget
	) -> bool;

	auto accept_phase_reports(
		data& d
	) -> bool;

	constexpr time longest_await = hours(48.f);

	auto stamp_of(
		const std::filesystem::path& file
	) -> file_stamp;

	auto wake_in(
		const await_condition& waiting
	) -> time;

	auto await_label(
		const await_condition& waiting
	) -> std::string;

	auto aim_of(
		const std::filesystem::path& record
	) -> std::string;

	auto fired_condition(
		await_condition& waiting
	) -> std::optional<std::string>;

	auto accept_awaits(
		data& d
	) -> bool;

	auto wake_goal(
		session& s,
		std::string_view fired
	) -> void;

	auto service_awaits(
		data& d
	) -> bool;

	auto pause_goal(
		session& s
	) -> void;
	auto accept_gap_reports(
		data& d
	) -> bool;

	auto accept_questions(
		data& d
	) -> bool;

	auto accept_draws(
		data& d
	) -> bool;

	auto accept_step_reports(
		data& d
	) -> bool;

	auto awaiting_answer(
		const session& s
	) -> bool;

	auto answer_question(
		session& s,
		std::string_view note
	) -> void;

	auto service_questions(
		data& d
	) -> bool;
}
