export module gse.ide.agent:draft;

import std;
import gse;

import :model;

namespace gse::ide::agent {
	constexpr std::string_view draft_command = "claude -p --output-format text";
	constexpr std::string_view draft_denied_tools = R"( --disallowed-tools "Bash Edit Write NotebookEdit Agent Read Grep Glob WebFetch WebSearch TodoWrite")";
	constexpr std::size_t draft_diff_lines = 4000;
	constexpr std::size_t draft_recent_subjects = 10;
	constexpr std::size_t draft_step_budget = 6000;

	auto draft_instructions() -> std::string_view;

	auto diff_steps(
		const draft_request& request
	) -> std::vector<std::string>;

	auto selected_diff(
		const draft_request& request
	) -> std::expected<std::string, std::string>;

	auto truncated_diff(
		std::string_view diff
	) -> std::string;

	auto draft_context(
		const draft_request& request,
		std::string_view subjects,
		std::string_view diff
	) -> std::string;

	auto attribution_line(
		std::string_view line
	) -> bool;

	auto cleaned_message(
		std::span<const std::string> lines
	) -> std::string;

	auto run_claude(
		const std::filesystem::path& root,
		std::string_view model,
		std::string_view prompt,
		const std::stop_token& cancel
	) -> std::expected<std::vector<std::string>, std::string>;

	auto draft_commit_message(
		const draft_request& request,
		std::string_view model,
		const std::stop_token& cancel
	) -> draft_ready;
}
