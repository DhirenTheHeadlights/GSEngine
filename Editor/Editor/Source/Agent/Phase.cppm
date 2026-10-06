export module gse.ide.agent:phase;

import std;
import gse;

import :model;

namespace gse::ide::agent {
	constexpr std::string_view docs_dir_name = "docs";
	constexpr std::string_view workflow_doc_name = "AGENT_WORKFLOW.md";
	constexpr std::string_view charter_dir_name = "agent";
	constexpr std::string_view scratch_dir_name = "agent";

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
		task_phase::settled,
	};

	auto current_phase(
		const session& s
	) -> task_phase;

	auto task_runs(
		const session& s
	) -> std::span<const phase_run>;

	auto phase_timeline(
		const session& s
	) -> std::array<phase_step, std::size(phase_order)>;

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

	auto next_phase(
		const session& s,
		std::uint32_t findings
	) -> task_phase;

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

	auto task_artifacts(
		const session& s
	) -> std::array<std::filesystem::path, 4>;

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
		session& s
	) -> bool;

	auto accept_phase_reports(
		data& d
	) -> bool;
}
