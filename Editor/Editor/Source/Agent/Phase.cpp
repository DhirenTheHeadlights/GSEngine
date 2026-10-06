module gse.ide.agent:phase_impl;

import gse;
import gse.ide.build;
import gse.ide.config;
import std;

import :blame;
import :chats;
import :layout;
import :model;
import :phase;
import :session;

auto gse::ide::agent::current_phase(const session& s) -> task_phase {
	return s.runs.empty() ? task_phase::apply : s.runs.back().phase;
}

auto gse::ide::agent::task_runs(const session& s) -> std::span<const phase_run> {
	const std::span all(s.runs);
	for (std::size_t at = all.size(); at > 0; --at) {
		if (all[at - 1].phase == task_phase::scope) {
			return all.subspan(at - 1);
		}
	}
	return all;
}

auto gse::ide::agent::phase_timeline(const session& s) -> std::array<phase_step, std::size(phase_order)> {
	const task_phase now = current_phase(s);
	const std::span<const phase_run> task = task_runs(s);

	std::array<phase_step, std::size(phase_order)> steps{};
	for (std::size_t at = 0; at < steps.size(); ++at) {
		const task_phase station = phase_order[at];
		const auto cycles = static_cast<std::uint32_t>(std::ranges::count(task, station, &phase_run::phase));
		steps[at] = {
			.phase = station,
			.cycles = cycles,
			.visited = cycles > 0,
			.current = station == now,
		};
	}
	return steps;
}

auto gse::ide::agent::policy_of(const task_phase phase) -> phase_policy {
	return annotation_from_enum<phase_policy>(phase, {});
}

auto gse::ide::agent::thinking_phrase(const session& s) -> std::string {
	const task_phase phase = current_phase(s);
	return s.runs.empty() || phase == task_phase::settled
		? std::string("thinking")
		: std::string(policy_of(phase).label);
}

auto gse::ide::agent::reviews_spent(const session& s) -> std::uint32_t {
	return static_cast<std::uint32_t>(std::ranges::count(task_runs(s), task_phase::review, &phase_run::phase));
}

auto gse::ide::agent::next_phase(const session& s, const std::uint32_t findings) -> task_phase {
	switch (current_phase(s)) {
		case task_phase::scope:
			return task_phase::apply;
		case task_phase::apply:
			return task_phase::review;
		case task_phase::review:
			return findings == 0 || reviews_spent(s) >= max_review_cycles ? task_phase::summarize : task_phase::revise;
		case task_phase::revise:
			return task_phase::review;
		case task_phase::summarize:
		case task_phase::settled:
			return task_phase::settled;
	}
	return task_phase::settled;
}

auto gse::ide::agent::session_scratch(const session& s) -> std::filesystem::path {
	return config::project_state_dir() / scratch_dir_name / std::format("{}", s.id);
}

auto gse::ide::agent::prompt_path(const session& s) -> std::filesystem::path {
	return session_scratch(s) / "prompt.md";
}

auto gse::ide::agent::scope_path(const session& s) -> std::filesystem::path {
	return session_scratch(s) / "scope.md";
}

auto gse::ide::agent::report_path(const session& s) -> std::filesystem::path {
	return session_scratch(s) / "report.md";
}

auto gse::ide::agent::diff_path(const session& s) -> std::filesystem::path {
	return session_scratch(s) / "diff.patch";
}

auto gse::ide::agent::touched_path(const session& s) -> std::filesystem::path {
	return session_scratch(s) / "touched.txt";
}

auto gse::ide::agent::task_artifacts(const session& s) -> std::array<std::filesystem::path, 4> {
	return { scope_path(s), report_path(s), diff_path(s), touched_path(s) };
}

auto gse::ide::agent::changes_path(const session& s) -> std::filesystem::path {
	return session_scratch(s) / "changes.log";
}

auto gse::ide::agent::read_changes(const session& s) -> std::vector<transcript_row> {
	std::ifstream in(changes_path(s), std::ios::binary);
	if (!in) {
		return {};
	}

	std::vector<transcript_row> out;
	for (std::string line; std::getline(in, line); ) {
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}

		if (line.starts_with(change_header)) {
			const std::string_view body = std::string_view(line).substr(change_header.size());
			const std::size_t owner = body.find('\t');
			const std::size_t split = owner == std::string_view::npos ? owner : body.find('\t', owner + 1);
			if (split == std::string_view::npos) {
				continue;
			}
			const std::filesystem::path file(body.substr(split + 1));
			out.push_back({
				.kind = row_kind::tool,
				.text = std::string(body.substr(owner + 1, split - owner - 1)),
				.detail = file.filename().generic_display_string(),
				.agent = std::string(body.substr(0, owner)),
				.file = file,
			});
			continue;
		}
		if (out.empty() || line.empty()) {
			continue;
		}
		if (line.front() == '-') {
			out.back().removed.emplace_back(line.substr(1));
		}
		else if (line.front() == '+') {
			out.back().added.emplace_back(line.substr(1));
		}
	}
	return out;
}

auto gse::ide::agent::docs_root(const session& s) -> std::filesystem::path {
	return config::worktree_for(s.cwd).engine_root / docs_dir_name;
}

auto gse::ide::agent::read_doc(const std::filesystem::path& file) -> std::string {
	std::ifstream in(file, std::ios::binary);
	if (!in) {
		log::println(log::level::warning, log::category::task, "agent: could not read '{}' - the phase prompt is missing that section", file);
		return {};
	}
	return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

auto gse::ide::agent::resume_target(const session& s) -> std::string {
	for (const phase_run& run : std::views::reverse(s.runs)) {
		if (!policy_of(run.phase).side_thread && !run.agent_id.empty()) {
			return run.agent_id;
		}
	}
	return {};
}

auto gse::ide::agent::write_phase_prompt(const session& s) -> std::filesystem::path {
	const std::filesystem::path path = prompt_path(s);
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	if (ec) {
		log::println(log::level::error, log::category::task, "agent: could not create '{}': {} - this chat starts without its workflow rules", path.parent_path(), ec.message());
		return {};
	}

	const task_phase phase = current_phase(s);
	const phase_policy policy = policy_of(phase);
	const std::filesystem::path docs = docs_root(s);
	const std::string workflow = read_doc(docs / workflow_doc_name);
	const std::string charter = read_doc(docs / charter_dir_name / std::format("{}.md", enum_to_string(phase)));

	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out) {
		log::println(log::level::error, log::category::task, "agent: could not open '{}' - this chat starts without its workflow rules", path);
		return {};
	}

	out << workflow;
	out << "\n---\n\n# Current Phase: " << std::string_view(policy.label) << "\n\n";
	out << charter;

	std::string handoff;
	for (const std::filesystem::path& artifact : task_artifacts(s)) {
		if (std::filesystem::exists(artifact, ec)) {
			handoff += std::format("- `{}`\n", artifact.generic_display_string());
		}
	}
	if (!handoff.empty()) {
		out << "\n## Handoff Artifacts\n\nRead these before anything else. They are this task's own record, written by the editor.\n\n" << handoff;
	}

	out << "\n## Build State\n\nAs this phase started, the files this chat has edited were: " << unbuilt_label(s) << ".\n";
	out << "The owner builds independently of you, so a build you did not request may land at any time and this line may already be out of date. Never assert that something is or is not built unless you ran the build yourself and saw the result.\n";

	std::string peers;
	for (const build_inbox::presence& active : build_inbox::take_presence()) {
		if (active.agent != s.info.agent_id) {
			peers += std::format("- {} is working in tree '{}'\n", active.name, active.tree);
		}
	}
	if (!peers.empty()) {
		out << "\n## Other Chats Working Right Now\n\n" << peers;
	}

	if (!out) {
		log::println(log::level::error, log::category::task, "agent: could not write '{}' - this chat starts without its workflow rules", path);
		return {};
	}
	return path;
}

auto gse::ide::agent::track_run_id(session& s) -> void {
	if (s.runs.empty() || s.info.agent_id.empty() || s.runs.back().agent_id == s.info.agent_id) {
		return;
	}
	s.runs.back().agent_id = s.info.agent_id;
}

auto gse::ide::agent::plan_handoff(const session& s, const task_phase from, const std::string_view summary) -> handoff_capture {
	const phase_policy leaving = policy_of(from);

	handoff_capture planned{
		.scratch = session_scratch(s),
		.record = from == task_phase::scope ? scope_path(s) : report_path(s),
		.touched = touched_path(s),
		.diff = diff_path(s),
		.root = config::worktree_for(s.cwd).engine_root,
		.heading = from == task_phase::scope
			? std::string("# Approved Scope")
			: std::format("# Reported Leaving {}", std::string_view(leaving.label)),
		.summary = std::string(summary),
	};

	planned.log = changes_path(s);

	const std::span<const phase_run> task = task_runs(s);
	const std::size_t begun = task.empty() ? 0 : std::min<std::size_t>(task.front().first_row, s.rows.size());
	for (std::size_t at = 0; at < s.rows.size(); ++at) {
		const transcript_row& row = s.rows[at];
		if (!shows(row_filter::changes, row)) {
			continue;
		}
		planned.edits.push_back(row);
		if (at >= begun && !std::ranges::contains(planned.written, row.file)) {
			planned.written.push_back(row.file);
		}
	}
	return planned;
}

auto gse::ide::agent::capture_handoff(const handoff_capture& planned, const std::stop_token& cancel) -> bool {
	std::error_code ec;
	std::filesystem::create_directories(planned.scratch, ec);
	if (ec) {
		log::println(log::level::error, log::category::task, "agent: could not create '{}': {} - the next phase starts without a handoff", planned.scratch, ec.message());
		return false;
	}

	if (std::ofstream out(planned.record, std::ios::binary | std::ios::trunc); out) {
		out << planned.heading << "\n\n" << planned.summary << '\n';
	}

	if (std::ofstream out(planned.touched, std::ios::binary | std::ios::trunc); out) {
		for (const std::filesystem::path& file : planned.written) {
			out << file.generic_display_string() << '\n';
		}
	}

	if (std::ofstream out(planned.log, std::ios::binary | std::ios::trunc); out) {
		for (const transcript_row& edit : planned.edits) {
			out << change_header << edit.agent << '\t' << edit.text << '\t' << edit.file.generic_display_string() << '\n';
			for (const std::string& line : edit.removed) {
				out << '-' << line << '\n';
			}
			for (const std::string& line : edit.added) {
				out << '+' << line << '\n';
			}
		}
	}

	const time capture_limit = seconds(10.f);
	const process::run_outcome captured = process::run_capture({
		.command_line = "git --no-optional-locks diff HEAD",
		.working_dir = planned.root,
		.output_path = planned.diff,
		.limit = capture_limit,
		.cancel = cancel,
	});
	if (!captured) {
		std::filesystem::remove(planned.diff, ec);
		return false;
	}
	return true;
}

auto gse::ide::agent::begin_phase(session& s, const task_phase phase, const phase_gate& handover) -> void {
	const phase_policy policy = policy_of(phase);
	const std::string resumed = policy.fresh_context ? std::string{} : resume_target(s);

	s.runs.push_back({
		.phase = phase,
		.agent_id = resumed,
		.first_row = static_cast<std::uint32_t>(s.rows.size()),
		.started = unix_now(),
	});

	append_row(s, {
		.kind = row_kind::phase,
		.text = std::string(policy.label),
	});

	if (s.published_presence) {
		s.published_presence = false;
		build_inbox::clear_presence(s.info.agent_id);
	}

	s.gate.reset();
	s.info.agent_id = resumed;
	s.info.failure.clear();
	s.retry = {};

	if (phase == task_phase::settled) {
		stop_session(s);
		return;
	}

	restart_session(s);

	const std::string_view opening = policy.opening;
	if (opening.empty()) {
		return;
	}

	std::string message(opening);
	if (!handover.summary.empty()) {
		message += std::format("\n\nWhat the previous phase reported:\n\n{}", handover.summary);
	}
	if (!handover.note.empty()) {
		message += std::format("\n\nThe owner added this when approving, and it takes precedence over the above:\n\n{}", handover.note);
	}
	send_to_session(s, message, {});
}

auto gse::ide::agent::restart_task(session& s) -> void {
	std::error_code ec;
	for (const std::filesystem::path& artifact : task_artifacts(s)) {
		std::filesystem::remove(artifact, ec);
	}

	begin_phase(s, task_phase::scope, {});
}

auto gse::ide::agent::advance_phase(session& s, const std::string_view note) -> void {
	if (s.pending_transition) {
		return;
	}

	phase_gate reported = s.gate.value_or(phase_gate{});
	reported.from = current_phase(s);
	reported.note.assign(note);
	s.pending_transition = std::move(reported);
	s.gate.reset();

	interrupt_session(s);
}

auto gse::ide::agent::service_phase(session& s) -> bool {
	if (!s.pending_transition || s.think_clock) {
		return false;
	}

	if (s.handoff.active()) {
		const std::optional<bool> captured = s.handoff.take();
		if (!captured) {
			return false;
		}
		if (!*captured) {
			log::println(log::level::warning, log::category::task, "agent: could not capture the handoff for chat '{}' - the next phase reads the tree itself", s.name);
		}

		const phase_gate reported = *std::exchange(s.pending_transition, std::nullopt);
		begin_phase(s, next_phase(s, reported.findings), reported);
		return true;
	}

	const phase_gate& reported = *s.pending_transition;
	if (reported.findings > 0 && next_phase(s, reported.findings) == task_phase::settled) {
		log::println(log::level::warning, log::category::task, "agent: chat '{}' hit the {}-review limit with {} finding(s) still open", s.name, max_review_cycles, reported.findings);
		append_row(s, {
			.kind = row_kind::phase,
			.text = std::format("review limit reached - {} finding(s) left open", reported.findings),
			.detail = reported.summary,
		});
	}

	hydrate_session(s);

	s.handoff.start([planned = plan_handoff(s, reported.from, reported.summary)](const std::stop_token& cancel) {
		return capture_handoff(planned, cancel);
	}, trace_id<"agent::handoff">(), task::lane::background);
	return false;
}

auto gse::ide::agent::accept_phase_reports(data& d) -> bool {
	bool recorded = false;

	for (build_inbox::phase_report& incoming : build_inbox::peek_phase_reports()) {
		session* found = claimed_session(d, incoming.agent, incoming.cwd);
		if (!found) {
			continue;
		}
		build_inbox::consume_phase_report(incoming.id);

		const task_phase from = current_phase(*found);
		if (from == task_phase::settled) {
			build_inbox::publish({
				.id = incoming.id,
				.outcome = build_inbox::status::rejected,
				.lines = { "this task is already settled - nothing follows it" },
			});
			continue;
		}
		if (found->pending_transition) {
			build_inbox::publish({
				.id = incoming.id,
				.outcome = build_inbox::status::rejected,
				.lines = { "this task is already moving to its next phase - the editor is restarting this chat, so END YOUR TURN NOW" },
			});
			continue;
		}

		const phase_policy policy = policy_of(from);
		const task_phase to = next_phase(*found, incoming.findings);
		const phase_policy arriving = policy_of(to);

		const std::string outcome = incoming.findings > 0
			? std::format("{} reported {} finding(s)", std::string_view(policy.label), incoming.findings)
			: std::format("{} reported done", std::string_view(policy.label));

		append_row(*found, {
			.kind = row_kind::phase,
			.text = policy.gated ? outcome + " - waiting for approval" : outcome,
			.detail = incoming.summary,
			.phase = from,
		});

		if (!found->runs.empty()) {
			found->runs.back().summary = incoming.summary;
			found->runs.back().findings = incoming.findings;
		}

		recorded = true;

		if (policy.gated) {
			found->gate = {
				.from = from,
				.summary = incoming.summary,
				.findings = incoming.findings,
			};
			build_inbox::publish({
				.id = incoming.id,
				.outcome = build_inbox::status::ok,
				.lines = { std::format("recorded - the editor is waiting for approval before moving to {}. END YOUR TURN NOW; you will be prompted when it is granted", std::string_view(arriving.label)) },
			});
			continue;
		}

		log::println(log::level::info, log::category::task, "agent: chat '{}' moves from {} to {}", found->name, enum_to_string(from), enum_to_string(to));
		build_inbox::publish({
			.id = incoming.id,
			.outcome = build_inbox::status::ok,
			.lines = { std::format("recorded - moving to {}. END YOUR TURN NOW; the editor restarts this chat in the next phase", std::string_view(arriving.label)) },
		});

		found->pending_transition = {
			.from = from,
			.summary = incoming.summary,
			.findings = incoming.findings,
		};
	}

	return recorded;
}
