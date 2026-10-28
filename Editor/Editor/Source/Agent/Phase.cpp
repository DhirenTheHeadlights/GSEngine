module gse.ide.agent:phase_impl;

import gse;
import gse.ide.build;
import gse.ide.config;
import std;

import :blame;
import :canvas;
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
		if (all[at - 1].phase == task_phase::scope || all[at - 1].phase == task_phase::steer) {
			return all.subspan(at - 1);
		}
	}
	return all;
}

auto gse::ide::agent::approved_steps(const session& s) -> std::span<const build_inbox::plan_step> {
	const auto scoped = std::ranges::find_last(task_runs(s), task_phase::scope, &phase_run::phase);
	return scoped.empty() ? std::span<const build_inbox::plan_step>{} : std::span<const build_inbox::plan_step>(scoped.front().steps);
}

auto gse::ide::agent::run_section_of(const session& s, const std::uint32_t row) -> std::optional<std::string> {
	const std::span<const phase_run> runs = task_runs(s);
	const auto owner = std::ranges::find_last_if(runs, [&](const phase_run& run) {
		return run.first_row <= row;
	});
	if (owner.empty() || (owner.front().phase != task_phase::apply && owner.front().phase != task_phase::revise)) {
		return std::nullopt;
	}
	const phase_run& run = owner.front();
	const bool first = std::ranges::none_of(std::span(s.rows).subspan(run.first_row, row - run.first_row), [](const transcript_row& earlier) {
		return shows(row_filter::changes, earlier);
	});
	if (!first) {
		return std::nullopt;
	}
	if (run.phase == task_phase::apply) {
		return std::string("apply");
	}
	const auto upto = std::ranges::subrange(runs.begin(), owner.begin());
	return std::format("revise {}", std::ranges::count(upto, task_phase::revise, &phase_run::phase) + 1);
}

auto gse::ide::agent::record_step_edit(session& s, const transcript_row& row) -> void {
	if (s.runs.empty() || s.runs.back().phase != task_phase::apply) {
		return;
	}
	phase_run& run = s.runs.back();
	const auto current = std::ranges::find(run.progress, step_state::current);
	run.edits.push_back({
		.file = row.file,
		.removed = row.removed,
		.added = row.added,
		.step = current == run.progress.end() ? std::nullopt : std::optional(static_cast<std::uint32_t>(std::distance(run.progress.begin(), current))),
		.stamped = unix_now(),
	});
}

auto gse::ide::agent::phase_timeline(const task_phase now, const bool goal, const std::span<const phase_run> task) -> std::vector<phase_step> {
	std::vector<phase_step> steps;
	for (const task_phase station : phase_order) {
		if (station == task_phase::settled || (station == task_phase::steer && !goal)) {
			continue;
		}
		const auto cycles = static_cast<std::uint32_t>(std::ranges::count(task, station, &phase_run::phase));
		steps.push_back({
			.phase = station,
			.cycles = cycles,
			.visited = cycles > 0,
			.current = station == now,
		});
	}
	return steps;
}

auto gse::ide::agent::phase_timeline(const session& s) -> std::vector<phase_step> {
	return phase_timeline(current_phase(s), s.goal.has_value(), task_runs(s));
}

auto gse::ide::agent::policy_of(const task_phase phase) -> phase_policy {
	return annotation_from_enum<phase_policy>(phase, {});
}

auto gse::ide::agent::thinking_phrase(const session&) -> std::string {
	return "thinking";
}

auto gse::ide::agent::reviews_spent(const session& s) -> std::uint32_t {
	return static_cast<std::uint32_t>(std::ranges::count(task_runs(s), task_phase::review, &phase_run::phase));
}

auto gse::ide::agent::verdict_policy_of(const goal_verdict verdict) -> verdict_policy {
	return annotation_from_enum<verdict_policy>(verdict, {});
}

auto gse::ide::agent::steered_code(const session& s) -> bool {
	const std::size_t begun = std::min<std::size_t>(s.runs.back().first_row, s.rows.size());
	return std::ranges::any_of(std::span(s.rows).subspan(begun), [](const transcript_row& row) {
		return shows(row_filter::changes, row) && (build_runner::is_build_source(row.file) || row.file.extension() == std::filesystem::path(code_shader_extension));
	});
}

auto gse::ide::agent::next_phase(const session& s, const phase_gate& reported) -> task_phase {
	switch (current_phase(s)) {
		case task_phase::scope:
			return task_phase::apply;
		case task_phase::apply:
			return task_phase::review;
		case task_phase::review:
			return reported.findings == 0 || reviews_spent(s) >= max_review_cycles ? task_phase::summarize : task_phase::revise;
		case task_phase::revise:
			return task_phase::review;
		case task_phase::summarize:
			return s.goal ? task_phase::steer : task_phase::settled;
		case task_phase::steer:
			return *reported.verdict != goal_verdict::escalate && steered_code(s) ? task_phase::review : verdict_policy_of(*reported.verdict).next;
		case task_phase::settled:
			return task_phase::settled;
	}
	return task_phase::settled;
}

auto gse::ide::agent::needs_owner(const session& s, const phase_gate& reported) -> bool {
	if (reported.verdict && verdict_policy_of(*reported.verdict).gated) {
		return true;
	}
	if (!s.goal) {
		return policy_of(reported.from).gated && !style_of(kind_of(s)).unattended;
	}
	return next_phase(s, reported) == task_phase::steer && s.goal->steers_left == 0;
}

auto gse::ide::agent::gate_label(const session& s, const phase_gate& gate) -> std::string {
	if (!gate.goal.empty()) {
		return std::string(approve_goal_label);
	}
	return std::string(policy_of(next_phase(s, gate)).enter_label);
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

auto gse::ide::agent::gaps_path(const session& s) -> std::filesystem::path {
	return session_scratch(s) / "gaps.md";
}

auto gse::ide::agent::task_artifacts(const session& s) -> std::array<std::filesystem::path, 5> {
	return { scope_path(s), report_path(s), diff_path(s), touched_path(s), gaps_path(s) };
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

auto gse::ide::agent::phase_charter(const session& s, const task_phase phase) -> std::string {
	return read_doc(docs_root(s) / charter_dir_name / std::format("{}.md", enum_to_string(phase)));
}

auto gse::ide::agent::sync_facts(const sync_request& request) -> std::string {
	std::string facts = std::format(
		"\n---\n\n# This Repository\n\n- root `{}`\n- branch `{}`\n- upstream {}\n",
		request.root.generic_display_string(),
		request.branch,
		request.upstream.empty()
			? std::string("none - this branch has never been pushed")
			: std::format("`{}`, {} ahead, {} behind", request.upstream, request.ahead, request.behind)
	);

	facts += "\n## Selected Files\n\n";
	for (const sync_file& file : request.files) {
		facts += std::format("- `{}` {} +{} -{}\n", file.relative.generic_display_string(), file.code, file.added, file.deleted);
	}

	facts += request.message.empty()
		? std::string("\n## Commit Message\n\nThe owner typed none, so you write it.\n")
		: std::format("\n## Commit Message\n\nThe owner typed this, and it is the message to commit with:\n\n{}\n", request.message);
	return facts;
}

auto gse::ide::agent::write_sync_prompt(const session& s) -> std::filesystem::path {
	const std::filesystem::path path = prompt_path(s);
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	if (ec) {
		log::println(log::level::error, log::category::task, "agent: could not create '{}': {} - this sync chat starts without its rules", path.parent_path(), ec.message());
		return {};
	}

	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out) {
		log::println(log::level::error, log::category::task, "agent: could not open '{}' - this sync chat starts without its rules", path);
		return {};
	}

	out << read_doc(docs_root(s) / charter_dir_name / std::format("{}.md", sync_charter_name));
	out << sync_facts(*s.sync);
	if (!out) {
		log::println(log::level::error, log::category::task, "agent: could not write '{}' - this sync chat starts without its rules", path);
		return {};
	}
	return path;
}

auto gse::ide::agent::write_phase_prompt(const session& s) -> std::filesystem::path {
	if (s.sync) {
		return write_sync_prompt(s);
	}

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
	const std::string charter = phase_charter(s, phase);

	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out) {
		log::println(log::level::error, log::category::task, "agent: could not open '{}' - this chat starts without its workflow rules", path);
		return {};
	}

	out << workflow;
	out << "\n---\n\n# Current Phase: " << std::string_view(policy.label) << "\n\n";
	out << charter;

	if (s.goal) {
		const std::string record = s.goal->record.generic_display_string();
		out << "\n## Goal\n\n";
		if (std::filesystem::exists(s.goal->record, ec)) {
			out << std::format("This chat pursues a goal the owner approved. Its record is `{}`: the aim, the pass criteria, the charter of what may be done without asking, and the ledger. The charter is the owner's standing authorisation - work inside it without asking, and escalate anything outside it. The record as this phase started:\n\n", record);
			out << read_doc(s.goal->record) << '\n';
		}
		else {
			out << std::format("This chat pursues a goal the owner approved, but its record `{}` does not exist yet. Write it first, verbatim from the approved scope, with any note the owner added at approval folded in.\n", record);
		}
		out << std::format("\nSteering runs left before the owner is asked to continue: {}.\n", s.goal->steers_left);
	}

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
	const config::worktree& tree = config::worktree_for(s.cwd);

	handoff_capture planned{
		.scratch = session_scratch(s),
		.record = from == task_phase::scope ? scope_path(s) : report_path(s),
		.touched = touched_path(s),
		.diff = diff_path(s),
		.roots = { tree.engine_root },
		.root = tree.engine_root,
		.gaps = gaps_path(s),
		.heading = from == task_phase::scope
			? std::string("# Approved Scope")
			: std::format("# Reported Leaving {}", std::string_view(leaving.label)),
		.summary = std::string(summary),
		.replace_record = from == task_phase::scope,
	};

	planned.log = changes_path(s);
	if (tree.project_root != tree.engine_root) {
		planned.roots.push_back(tree.project_root);
	}

	const std::span<const phase_run> task = task_runs(s);
	const std::size_t begun = task.empty() ? 0 : std::min<std::size_t>(task.front().first_row, s.rows.size());
	for (std::size_t at = 0; at < s.rows.size(); ++at) {
		const transcript_row& row = s.rows[at];
		if (at >= begun && row.kind == row_kind::gap) {
			planned.shortfalls.push_back(row);
		}
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

	if (std::ofstream out(planned.record, std::ios::binary | (planned.replace_record ? std::ios::trunc : std::ios::app)); out) {
		out << planned.heading << "\n\n" << planned.summary << '\n';
	}

	if (planned.shortfalls.empty()) {
		std::filesystem::remove(planned.gaps, ec);
	}
	else if (std::ofstream out(planned.gaps, std::ios::binary | std::ios::trunc); out) {
		out << "# Recorded Gaps\n\nTooling shortfalls the chats on this task reported as they hit them.\n\n";
		for (const transcript_row& shortfall : planned.shortfalls) {
			out << "- " << shortfall.text << '\n';
			if (!shortfall.detail.empty()) {
				out << "  " << shortfall.detail << '\n';
			}
		}
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
	std::string combined;
	for (const std::filesystem::path& root : planned.roots) {
		const process::run_outcome captured = process::run_capture({
			.command_line = "git --no-optional-locks diff HEAD",
			.working_dir = root,
			.output_path = planned.diff,
			.limit = capture_limit,
			.cancel = cancel,
		});
		if (!captured) {
			std::filesystem::remove(planned.diff, ec);
			return false;
		}
		combined += std::format("# {}\n", root.generic_display_string());
		if (std::ifstream in(planned.diff, std::ios::binary); in) {
			combined.append(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		}
	}
	if (std::ofstream out(planned.diff, std::ios::binary | std::ios::trunc); out) {
		out << combined;
	}
	return true;
}

auto gse::ide::agent::begin_phase(session& s, const task_phase phase, const phase_gate& handover) -> void {
	const phase_policy policy = policy_of(phase);
	const std::string resumed = policy.fresh_context ? std::string{} : resume_target(s);

	std::error_code ec;
	if (phase == task_phase::scope) {
		for (const std::filesystem::path& artifact : task_artifacts(s)) {
			std::filesystem::remove(artifact, ec);
		}
	}
	else if (phase == task_phase::steer) {
		std::filesystem::remove(scope_path(s), ec);
	}

	if (s.goal) {
		s.goal->waiting.reset();
		if (phase == task_phase::steer) {
			--s.goal->steers_left;
			s.goal->aim = aim_of(s.goal->record);
		}
	}

	std::vector<step_state> carried;
	if (phase == task_phase::apply) {
		const auto applied = std::ranges::find_last(task_runs(s), task_phase::apply, &phase_run::phase);
		if (!applied.empty()) {
			carried = applied.front().progress;
		}
	}

	s.runs.push_back({
		.phase = phase,
		.agent_id = resumed,
		.first_row = static_cast<std::uint32_t>(s.rows.size()),
		.started = unix_now(),
		.progress = std::move(carried),
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
	s.nudges = 0;

	if (phase == task_phase::settled) {
		stop_session(s);
		return;
	}

	restart_session(s);

	const std::string_view opening = std::string_view(policy.opening).empty() && !handover.summary.empty()
		? steered_task_opening
		: std::string_view(policy.opening);
	if (opening.empty()) {
		return;
	}

	std::string message;
	if (!policy.fresh_context) {
		if (const std::string charter = phase_charter(s, phase); !charter.empty()) {
			message += std::format(
				"# Current Phase: {}\n\nThis chat was resumed into a new phase, so the phase section of your system prompt is the one you were started under and is out of date. These are your instructions now, and they replace it.\n\n{}\n---\n\n",
				std::string_view(policy.label),
				charter
			);
		}
	}

	message += opening;
	if (!handover.summary.empty()) {
		message += std::format("\n\nWhat the previous phase reported:\n\n{}", handover.summary);
	}
	if (!handover.note.empty()) {
		message += std::format("\n\nThe owner added this when approving, and it takes precedence over the above:\n\n{}", handover.note);
	}
	send_to_session(s, message, {}, true);
}

auto gse::ide::agent::restart_task(session& s) -> void {
	s.met_goal.reset();
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

auto gse::ide::agent::service_phase(session& s, const std::uint32_t steer_budget) -> bool {
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
		const task_phase to = next_phase(s, reported);
		const bool approved = needs_owner(s, reported);

		if (!reported.goal.empty()) {
			s.goal = goal_state{
				.record = reported.goal,
				.steers_left = steer_budget,
			};
			s.met_goal.reset();
			log::println(log::level::info, log::category::task, "agent: chat '{}' now pursues the goal recorded in '{}'", s.name, reported.goal);
			append_row(s, {
				.kind = row_kind::phase,
				.text = std::format("pursuing goal {}", reported.goal.generic_display_string()),
			});
		}
		else if (to == task_phase::steer && approved) {
			s.goal->steers_left = steer_budget;
		}

		if (reported.verdict == goal_verdict::met) {
			log::println(log::level::info, log::category::task, "agent: chat '{}' met its goal '{}'", s.name, s.goal->record);
			append_row(s, {
				.kind = row_kind::phase,
				.text = "goal met",
				.detail = reported.summary,
			});
			s.met_goal = std::move(s.goal);
			s.goal.reset();
		}

		begin_phase(s, to, reported);
		return true;
	}

	const phase_gate& reported = *s.pending_transition;
	if (reported.from == task_phase::review && reported.findings > 0 && next_phase(s, reported) == task_phase::summarize) {
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

		if (found->sync) {
			log::println(log::level::info, log::category::task, "agent: sync chat for {} reported done - {}", found->sync->root, incoming.summary);
			append_row(*found, {
				.kind = row_kind::phase,
				.text = "sync reported done",
				.detail = incoming.summary,
			});
			found->sync_report = incoming.summary;
			recorded = true;
			build_inbox::publish({
				.id = incoming.id,
				.outcome = build_inbox::status::ok,
				.lines = { "recorded - END YOUR TURN NOW; the editor closes this conversation and hands the panel back" },
			});
			continue;
		}

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

		const auto refuse = [&incoming](std::string reason) {
			build_inbox::publish({
				.id = incoming.id,
				.outcome = build_inbox::status::rejected,
				.lines = { std::move(reason) },
			});
		};

		phase_gate reported{
			.from = from,
			.summary = incoming.summary,
			.findings = incoming.findings,
			.steps = std::move(incoming.steps),
		};

		if (!reported.steps.empty() && from != task_phase::scope) {
			refuse("`steps` lay out a plan for the owner to approve, so they belong to scoping - omit them in this phase and report again");
			continue;
		}

		if (incoming.diagnosis && (found->kind != chat_kind::build_error || from != task_phase::apply)) {
			refuse("`diagnosis` explains a build failure on a build-error chat's summary page, so only a build-error chat sends it, while applying - omit it and report again");
			continue;
		}

		if (!incoming.criteria.empty() && (from != task_phase::steer || !found->goal)) {
			refuse("`criteria` say where a goal's pass criteria stand, so only a steering run sends them - omit them in this phase and report again");
			continue;
		}

		std::vector<criterion_report> criteria;
		const auto unknown = std::ranges::find_if(incoming.criteria, [&criteria](const build_inbox::reported_criterion& line) {
			criterion_state state{};
			if (!enum_from_string(line.state, state) || line.id.empty()) {
				return true;
			}
			criteria.push_back({ .id = line.id, .state = state, .note = line.note });
			return false;
		});
		if (unknown != incoming.criteria.end()) {
			refuse(std::format("criteria[{}]: expected an id and a state of met, owner or open, got '{} {}' - nothing was recorded, report again", unknown - incoming.criteria.begin(), unknown->id, unknown->state));
			continue;
		}

		if (!incoming.next.empty()) {
			goal_verdict verdict{};
			if (!enum_from_string(incoming.next, verdict)) {
				refuse(std::format("`next` must be task, met or escalate, not '{}' - nothing was recorded, report again", incoming.next));
				continue;
			}
			reported.verdict = verdict;
		}

		if (from == task_phase::steer && !reported.verdict) {
			refuse("steering must say what follows: pass `next` as task, met or escalate - nothing was recorded, report again");
			continue;
		}
		if (reported.verdict && from != task_phase::steer && !(from == task_phase::scope && found->goal && reported.verdict == goal_verdict::escalate)) {
			refuse("`next` is for steering a goal, or `escalate` while scoping a task under one - omit it in this phase and report again");
			continue;
		}

		if (!incoming.goal.empty()) {
			if (from != task_phase::scope) {
				refuse("a goal is proposed while scoping, so the owner approves its text with the plan - omit `goal` in this phase and report again");
				continue;
			}
			if (found->goal) {
				refuse(std::format("this chat already pursues the goal recorded in '{}' - propose changes to it with `next: escalate` from a steering run instead", found->goal->record.generic_display_string()));
				continue;
			}
			reported.goal = incoming.goal.is_absolute()
				? incoming.goal.lexically_normal()
				: (config::worktree_for(found->cwd).project_root / incoming.goal).lexically_normal();
		}

		const phase_policy policy = policy_of(from);
		const task_phase to = next_phase(*found, reported);
		const phase_policy arriving = policy_of(to);
		const bool gated = needs_owner(*found, reported);

		std::string outcome = incoming.findings > 0
			? std::format("{} reported {} finding(s)", std::string_view(policy.label), incoming.findings)
			: std::format("{} reported done", std::string_view(policy.label));
		if (reported.verdict) {
			outcome += std::format(" - next: {}", enum_to_string(*reported.verdict));
		}
		if (!reported.goal.empty()) {
			outcome += std::format(" - proposes goal {}", reported.goal.generic_display_string());
		}

		append_row(*found, {
			.kind = row_kind::phase,
			.text = gated ? outcome + " - waiting for approval" : outcome,
			.detail = incoming.summary,
			.phase = from,
		});

		if (!found->runs.empty()) {
			found->runs.back().summary = incoming.summary;
			found->runs.back().findings = incoming.findings;
			found->runs.back().steps = reported.steps;
			found->runs.back().verdict = reported.verdict;
		}
		if (!criteria.empty()) {
			found->goal->criteria = std::move(criteria);
		}
		if (incoming.diagnosis) {
			if (!found->fix) {
				found->fix.emplace();
			}
			found->fix->diagnosis = std::move(*incoming.diagnosis);
		}

		recorded = true;

		if (gated) {
			reported.first_row = found->gate && found->gate->from == from
				? std::min(found->gate->first_row, static_cast<std::uint32_t>(found->rows.size()))
				: static_cast<std::uint32_t>(found->rows.size());
			found->gate = std::move(reported);
			found->plan_selected.reset();
			found->plan_page = 0;
			found->plan_summary = {};
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

		found->pending_transition = std::move(reported);
	}

	return recorded;
}

auto gse::ide::agent::stamp_of(const std::filesystem::path& file) -> file_stamp {
	std::error_code ec;
	const std::uintmax_t size = std::filesystem::file_size(file, ec);
	if (ec) {
		return {};
	}
	const std::filesystem::file_time_type written = std::filesystem::last_write_time(file, ec);
	return {
		.exists = true,
		.size = size,
		.written = ec ? 0 : static_cast<std::int64_t>(written.time_since_epoch().count()),
	};
}

auto gse::ide::agent::await_label(const await_condition& waiting) -> std::string {
	std::string out;
	if (waiting.process) {
		out += std::format("pid {} to exit, ", waiting.process->pid);
	}
	if (waiting.file) {
		out += std::format("'{}' to change, ", waiting.file->path.filename().generic_display_string());
	}
	const time remaining = wake_in(waiting);
	const float whole_hours = std::floor(remaining.as<hours>());
	return out + std::format("at most {:.0f}h {:.0f}m", whole_hours, std::floor((remaining - hours(whole_hours)).as<minutes>()));
}

auto gse::ide::agent::wake_in(const await_condition& waiting) -> time {
	return seconds(static_cast<float>(std::max<std::int64_t>(waiting.deadline - unix_now(), 0)));
}

auto gse::ide::agent::fired_condition(await_condition& waiting) -> std::optional<std::string> {
	if (waiting.process) {
		awaited_process& process = *waiting.process;
		if (!process.handle.valid()) {
			std::optional<os::watched_process> reopened = os::watch_process(process.pid);
			if (!reopened || reopened->created != process.created) {
				return std::format("process {} exited while the editor was closed", process.pid);
			}
			process.handle = std::move(reopened->process);
		}
		if (const std::optional<std::uint32_t> code = os::exit_code_of(process.handle)) {
			return std::format("process {} exited with code {:#x}", process.pid, *code);
		}
	}
	if (waiting.file && stamp_of(waiting.file->path) != waiting.file->baseline) {
		return std::format("'{}' changed", waiting.file->path.generic_display_string());
	}
	if (unix_now() >= waiting.deadline) {
		return std::string("the deadline you set passed");
	}
	return std::nullopt;
}

auto gse::ide::agent::accept_awaits(data& d) -> bool {
	bool recorded = false;

	for (build_inbox::await_request& incoming : build_inbox::peek_awaits()) {
		session* found = claimed_session(d, incoming.agent, incoming.cwd);
		if (!found) {
			continue;
		}
		build_inbox::consume_await(incoming.id);

		const auto refuse = [&incoming](std::string reason) {
			build_inbox::publish({
				.id = incoming.id,
				.outcome = build_inbox::status::rejected,
				.lines = { std::move(reason) },
			});
		};

		if (!found->goal || current_phase(*found) != task_phase::steer) {
			refuse("only a goal's steering run can wait on a condition - to wait for a build, call gse_hibernate");
			continue;
		}
		if (found->pending_transition) {
			refuse("this task is already moving to its next phase - END YOUR TURN NOW");
			continue;
		}
		if (incoming.wait <= time{}) {
			refuse("`minutes` must be positive: every wait needs a deadline");
			continue;
		}

		await_condition waiting{
			.then = std::move(incoming.then),
			.deadline = unix_now() + static_cast<std::int64_t>(std::min(incoming.wait, longest_await).as<seconds>()),
			.capped = incoming.wait > longest_await,
		};

		if (incoming.pid) {
			std::optional<os::watched_process> watched = os::watch_process(*incoming.pid);
			if (!watched) {
				refuse(std::format("no process {} is running - it has already exited or the pid is wrong, so read its results instead of waiting", *incoming.pid));
				continue;
			}
			waiting.process = awaited_process{
				.pid = *incoming.pid,
				.created = watched->created,
				.handle = std::move(watched->process),
			};
		}

		if (!incoming.file.empty()) {
			const std::filesystem::path file = incoming.file.is_absolute()
				? incoming.file.lexically_normal()
				: (found->cwd / incoming.file).lexically_normal();
			waiting.file = awaited_file{
				.path = file,
				.baseline = stamp_of(file),
			};
		}

		const std::string label = await_label(waiting);
		found->goal->waiting = std::move(waiting);
		recorded = true;

		log::println(log::level::info, log::category::task, "agent: chat '{}' is waiting on its goal - {}", found->name, label);
		append_row(*found, {
			.kind = row_kind::note,
			.text = std::format("waiting on the goal: {}", label),
		});

		build_inbox::publish({
			.id = incoming.id,
			.outcome = build_inbox::status::ok,
			.lines = { std::format("waiting on {} - END YOUR TURN NOW. The editor stops this run and starts a fresh steering run when the first condition fires; it carries your `then` and nothing else, so anything worth keeping belongs in the goal record before you stop", label) },
		});
	}

	return recorded;
}

auto gse::ide::agent::accept_questions(data& d) -> bool {
	bool recorded = false;

	for (build_inbox::ask_request& incoming : build_inbox::peek_ask_requests()) {
		session* found = claimed_session(d, incoming.agent, incoming.cwd);
		if (!found) {
			continue;
		}
		build_inbox::consume_ask_request(incoming.id);

		if (found->question) {
			build_inbox::publish({
				.id = incoming.id,
				.outcome = build_inbox::status::rejected,
				.lines = { "you already have a question waiting on the owner, and they answer one at a time - nothing was asked. End your turn and wait for the one already in front of them" },
			});
			continue;
		}

		pending_question asked{
			.question = std::move(incoming.question),
			.context = std::move(incoming.context),
			.multi = incoming.multi,
		};
		for (std::string& label : incoming.options) {
			asked.options.push_back({ .label = std::move(label) });
		}

		std::string listed = asked.context;
		for (const question_option& option : asked.options) {
			if (!listed.empty()) {
				listed += '\n';
			}
			listed += "- " + option.label;
		}

		log::println(log::level::info, log::category::task, "agent: chat '{}' is asking the owner - {}", found->name, asked.question);
		append_row(*found, {
			.kind = row_kind::question,
			.text = std::format("asking you: {}", asked.question),
			.detail = listed,
		});

		const std::size_t offered = asked.options.size();
		found->question = std::move(asked);
		recorded = true;

		build_inbox::publish({
			.id = incoming.id,
			.outcome = build_inbox::status::ok,
			.lines = { std::format("asked - the owner has your {} option(s) in the agents panel, with a box beside them to steer you. END YOUR TURN NOW; their answer arrives as your next message", offered) },
		});
	}

	return recorded;
}

auto gse::ide::agent::accept_draws(data& d) -> bool {
	bool drew = false;

	for (build_inbox::draw_request& incoming : build_inbox::peek_draw_requests()) {
		session* caller = claimed_session(d, incoming.agent, incoming.cwd);
		if (!caller) {
			continue;
		}
		build_inbox::consume_draw_request(incoming.id);

		if (incoming.list) {
			std::vector<std::string> lines;
			for (const bool sketches : { false, true }) {
				lines.emplace_back(sketches ? "section sketches" : "section drawables");
				for (const drawable_entry& entry : drawable_catalogue() | std::views::filter([sketches](const drawable_entry& e) { return e.sketch == sketches; })) {
					lines.emplace_back("item");
					lines.push_back(std::format("field name {}", entry.name));
					lines.push_back(std::format("field fields {}", entry.fields));
					if (sketches) {
						lines.push_back(std::format("field summary {}", entry.summary));
					}
				}
			}
			build_inbox::publish({
				.id = incoming.id,
				.outcome = build_inbox::status::ok,
				.lines = std::move(lines),
			});
			continue;
		}

		const auto reject = [&](std::string reason) {
			build_inbox::publish({
				.id = incoming.id,
				.outcome = build_inbox::status::rejected,
				.lines = { std::format("error {} - nothing was drawn, and any canvas already named '{}' is unchanged", reason, incoming.name) },
			});
		};

		session* target = caller;
		if (incoming.chat) {
			const auto found = std::ranges::find(d.sessions, *incoming.chat, &session::id);
			if (found == d.sessions.end()) {
				reject(std::format("chat: no chat with id {} (gse_panel_query Agent lists them)", *incoming.chat));
				continue;
			}
			target = &*found;
		}

		if (incoming.size[0] <= 0.f || incoming.size[1] <= 0.f || incoming.size[0] > max_canvas_extent || incoming.size[1] > max_canvas_extent) {
			reject(std::format("size: {} is outside 1..{} logical pixels per side", incoming.size, max_canvas_extent));
			continue;
		}

		std::expected<std::vector<canvas_step>, std::string> steps = parse_canvas(incoming.payload);
		if (!steps) {
			reject(std::move(steps.error()));
			continue;
		}

		hydrate_session(*target);
		const std::size_t commands = steps->size();
		const auto row_of = [target, &incoming] {
			return std::ranges::find_if(target->rows, [&incoming](const transcript_row& row) {
				return row.kind == row_kind::canvas && row.text == incoming.name;
			});
		};

		if (const auto existing = std::ranges::find(target->canvases, incoming.name, &canvas_record::name); existing != target->canvases.end()) {
			existing->source = std::move(incoming.payload);
			existing->size = incoming.size;
			existing->steps = std::move(*steps);
			if (const auto row = row_of(); row != target->rows.end()) {
				for (transcript_view* v : views_of(*target)) {
					relayout_from(*v, static_cast<std::uint32_t>(std::distance(target->rows.begin(), row)));
				}
			}
		}
		else {
			const std::int64_t stamped = unix_now();
			target->canvases.push_back({
				.name = incoming.name,
				.source = std::move(incoming.payload),
				.size = incoming.size,
				.stamped = stamped,
				.steps = std::move(*steps),
			});
			append_row(*target, {
				.kind = row_kind::canvas,
				.text = incoming.name,
				.stamped = stamped,
			});
		}

		drew = true;
		build_inbox::publish({
			.id = incoming.id,
			.outcome = build_inbox::status::ok,
			.lines = {
				std::format("field drawn {}", incoming.name),
				std::format("field chat {}", target->id),
				std::format("count commands {}", commands),
				std::format("count row {}", std::distance(target->rows.begin(), row_of())),
			},
		});
	}

	return drew;
}

auto gse::ide::agent::accept_step_reports(data& d) -> bool {
	bool recorded = false;

	for (build_inbox::step_request& incoming : build_inbox::peek_step_requests()) {
		session* found = claimed_session(d, incoming.agent, incoming.cwd);
		if (!found) {
			continue;
		}
		build_inbox::consume_step_request(incoming.id);

		const auto reject = [&](const std::string_view reason) {
			build_inbox::publish({
				.id = incoming.id,
				.outcome = build_inbox::status::rejected,
				.lines = { std::format("error {} - no step changed", reason) },
			});
		};

		const std::span<const build_inbox::plan_step> steps = approved_steps(*found);
		if (current_phase(*found) != task_phase::apply) {
			reject(std::format("steps are reported from apply, and this chat is in {}", std::string_view(policy_of(current_phase(*found)).label)));
			continue;
		}
		if (steps.empty()) {
			reject("the approved scope has no steps to report against");
			continue;
		}
		if (incoming.index >= steps.size()) {
			reject(std::format("index: {} is out of range - the approved scope has {} step(s), numbered from 0", incoming.index, steps.size()));
			continue;
		}
		if (incoming.state != "begun" && incoming.state != "done") {
			reject(std::format("state: '{}' is neither 'begun' nor 'done'", incoming.state));
			continue;
		}

		phase_run& run = found->runs.back();
		run.progress.resize(steps.size(), step_state::remaining);
		if (incoming.state == "begun") {
			std::ranges::replace(run.progress, step_state::current, step_state::remaining);
			run.progress[incoming.index] = step_state::current;
		}
		else {
			run.progress[incoming.index] = step_state::done;
		}

		const auto done = std::ranges::count(run.progress, step_state::done);
		log::println(log::level::info, log::category::task, "agent: chat '{}' {} step {} of {}", found->name, incoming.state, incoming.index + 1, steps.size());
		recorded = true;

		build_inbox::publish({
			.id = incoming.id,
			.outcome = build_inbox::status::ok,
			.lines = {
				std::format("field step {}. {}", incoming.index + 1, steps[incoming.index].text),
				std::format("field state {}", std::string_view(annotation_from_enum<step_style>(run.progress[incoming.index], {}).label)),
				std::format("field done {}/{}", done, steps.size()),
			},
		});
	}

	return recorded;
}

auto gse::ide::agent::wake_goal(session& s, const std::string_view fired) -> void {
	const std::string then = std::move(s.goal->waiting->then);
	s.goal->waiting.reset();

	const std::string summary = std::format("The condition you waited on fired: {}.\n\nWhat you said to do then:\n\n{}", fired, then);
	log::println(log::level::info, log::category::task, "agent: chat '{}' woke on its goal - {}", s.name, fired);

	if (s.goal->steers_left == 0) {
		append_row(s, {
			.kind = row_kind::phase,
			.text = "steering budget spent - waiting for approval",
			.detail = summary,
			.phase = task_phase::steer,
		});
		s.gate = phase_gate{
			.from = task_phase::steer,
			.summary = summary,
			.verdict = goal_verdict::escalate,
		};
		return;
	}

	begin_phase(s, task_phase::steer, { .from = task_phase::steer, .summary = summary });
	s.woke_by.assign(fired);
}

auto gse::ide::agent::service_awaits(data& d) -> bool {
	bool woke = false;

	for (session& s : d.sessions) {
		if (!s.goal || !s.goal->waiting || s.pending_transition) {
			continue;
		}
		if (s.running && s.think_clock) {
			continue;
		}
		if (s.running) {
			stop_session(s);
		}

		if (const std::optional<std::string> fired = fired_condition(*s.goal->waiting)) {
			wake_goal(s, *fired);
			woke = true;
		}
	}

	return woke;
}

auto gse::ide::agent::aim_of(const std::filesystem::path& record) -> std::string {
	std::ifstream in(record, std::ios::binary);
	std::string line;
	bool under_aim = false;
	while (in && std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		if (line.starts_with("## ")) {
			under_aim = line == "## Aim";
			continue;
		}
		if (under_aim && !line.empty()) {
			return line;
		}
	}
	return {};
}

auto gse::ide::agent::pause_goal(session& s) -> void {
	const std::string label = await_label(*s.goal->waiting);
	s.goal->waiting.reset();

	const std::string summary = std::format("The owner paused this goal while it was waiting on {}.", label);
	append_row(s, {
		.kind = row_kind::phase,
		.text = "goal paused - waiting for approval",
		.detail = summary,
		.phase = task_phase::steer,
	});
	s.gate = phase_gate{
		.from = task_phase::steer,
		.summary = summary,
		.verdict = goal_verdict::escalate,
	};
}

auto gse::ide::agent::awaiting_answer(const session& s) -> bool {
	return s.question && !s.question->answered;
}

auto gse::ide::agent::answer_question(session& s, const std::string_view note) -> void {
	pending_question& asked = *s.question;
	asked.answered = true;
	asked.note.assign(note);
}

auto gse::ide::agent::service_questions(data& d) -> bool {
	bool delivered = false;

	for (session& s : d.sessions) {
		if (!s.question || !s.question->answered || is_busy(s)) {
			continue;
		}

		const pending_question asked = *std::exchange(s.question, std::nullopt);

		std::string chosen;
		for (const question_option& option : asked.options) {
			if (!option.chosen) {
				continue;
			}
			if (!chosen.empty()) {
				chosen += ", ";
			}
			chosen += option.label;
		}

		std::string answer;
		if (chosen.empty()) {
			answer = std::format(
				"The owner read your question - \"{}\" - and picked none of the options you offered. They answered in their own words instead:\n\n{}",
				asked.question,
				asked.note
			);
			answer += "\n\nThat is steering, not a verdict. Follow what they actually said: if they are asking you to go deeper on an option, answer that - with gse_ask again once you have better options, or in a plain reply - rather than picking for them. Do not treat it as a decision already made.";
		}
		else {
			answer = std::format("The owner answered your question - \"{}\" - and chose: {}", asked.question, chosen);
			if (!asked.note.empty()) {
				answer += "\n\nThey also said:\n\n" + asked.note;
			}
			answer += "\n\nThat is the decision. Act on it now and carry on with the work - do not ask it again, and do not relitigate it.";
		}

		log::println(log::level::info, log::category::task, "agent: delivering the owner's answer to chat '{}'", s.name);
		resume_with(s, std::move(answer), transcript_row{
			.kind = row_kind::answer,
			.text = asked.question,
			.detail = asked.note,
			.options = asked.options,
		});
		delivered = true;
	}

	return delivered;
}

auto gse::ide::agent::accept_gap_reports(data& d) -> bool {
	bool recorded = false;

	for (build_inbox::gap_report& incoming : build_inbox::peek_gap_reports()) {
		session* found = claimed_session(d, incoming.agent, incoming.cwd);
		if (!found) {
			continue;
		}
		build_inbox::consume_gap_report(incoming.id);

		log::println(log::level::info, log::category::task, "agent: chat '{}' reported a {} gap - {}", found->name, incoming.kind, incoming.need);
		append_row(*found, {
			.kind = row_kind::gap,
			.text = std::format("{} gap: {}", incoming.kind, incoming.need),
			.detail = incoming.tried,
		});
		recorded = true;
	}

	return recorded;
}
