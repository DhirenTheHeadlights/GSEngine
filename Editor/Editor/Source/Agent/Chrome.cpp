module gse.ide.agent:chrome_impl;

import std;
import gse;
import gse.ide.build;
import gse.ide.config;

import :blame;
import :canvas;
import :chats;
import :chrome;
import :layout;
import :model;
import :panel;
import :phase;
import :session;

namespace gse::ide::agent {
	constexpr std::string_view answer_label = "Answer";
	constexpr std::uint32_t gallery_findings = 3;
	constexpr std::string_view gallery_review_clean = "Read the whole diff; nothing to fix.";
	constexpr std::string_view gallery_review_found = "- null deref when the gate is empty\n- split ratio not clamped\n- the doc misses an event";
	constexpr std::string_view gallery_revise_did = "Guarded the empty gate, clamped the ratio, added the event row.";
	constexpr std::size_t gallery_revise_edits = 4;
	constexpr std::size_t gallery_crowded_options = 12;
	constexpr float gallery_open_room = 40.f;
	constexpr float gallery_tight_room = 12.f;
	constexpr std::string_view gallery_elapsed = "3.0s";
	constexpr std::string_view gallery_failure = "process exited with code 1";
	constexpr std::string_view gallery_blocker = "wasm";
	constexpr std::string_view gallery_blame = "Panel.cpp:1624 +2";
	constexpr std::string_view gallery_unbuilt = "NOT in the exe: Chrome.cpp";
	constexpr std::size_t gallery_unclaimed = 3;
	constexpr std::size_t gallery_holding = 2;
	constexpr std::string_view gallery_awaited_build = "Chrome.cpp";
	constexpr std::string_view gallery_red_blocker = "blocked on a red tree - wasm";
	constexpr std::uint32_t gallery_pid = 4812;
	constexpr std::string_view gallery_file = "steer.log";
	constexpr std::string_view gallery_fired = "'steer.log' changed";
	constexpr time gallery_wake_in = minutes(42.f);
	constexpr float gallery_plan_room = 40.f;
	constexpr float gallery_changes_room = 16.f;
	constexpr float gallery_goal_room = 18.f;
	constexpr std::uint32_t gallery_steers_left = 6;
	constexpr std::int64_t gallery_steered_at = 1793059200;
	constexpr std::string_view gallery_aim = "agents can draw, UI can animate, and the agent window shows what the moment is about";
	constexpr std::string_view gallery_record = "Sandbox/docs/agent_chrome_goal.md";
	constexpr std::string_view gallery_ledger_task = "Owner accepted the build-error page. Next: the goal screen, sketched first.";
	constexpr std::string_view gallery_ledger_escalate = "Build-error page built; **criterion 18** needs the owner's eye.";
	constexpr std::size_t gallery_plan_focus = 0;
	constexpr std::string_view gallery_report = "## What landed\n\n- The summarize stage has its own **report** page\n\n## Decisions\n\n- Edits jump to `Diffs`\n\n## Open items\n\n- Not seen on screen";
	constexpr std::string_view gallery_plan_summary = "## Plan: two pages\n\nPage 1 shows **this summary** as markdown, page 2 the `steps`.\n\n- Navigation sits top right\n- Revise and Approve stay below";
	constexpr std::array<std::pair<std::string_view, vec4f gui::style::*>, 3> gallery_followup = { {
		{ "you: render the summary as markdown", &gui::style::color_text },
		{ "agent: revised the plan", &gui::style::color_accent },
		{ "agent: summary and step 2 updated", &gui::style::color_accent },
	} };

	auto gallery_facts(
		chat_kind kind,
		task_phase phase,
		agent_state state
	) -> chrome_facts;

	auto action_name(
		const chrome_plan& plan
	) -> std::string;

	auto question_samples() -> std::vector<std::pair<pending_question, float>>;

	auto plan_sample() -> std::vector<build_inbox::plan_step>;

	auto step_edit_sample() -> std::vector<step_edit>;

	auto answer_samples() -> std::vector<transcript_row>;

	auto gallery_fix() -> build_fix;

	auto gallery_criteria() -> std::vector<criterion_report>;

	auto build_status(
		const chrome_facts& facts
	) -> std::string;
}

auto gse::ide::agent::fade(const vec4f color, const float alpha) -> vec4f {
	return { color[0], color[1], color[2], color[3] * alpha };
}

auto gse::ide::agent::facts_of(const data& d, const session& s) -> chrome_facts {
	const bool busy = is_busy(s);
	const bool waiting = build_wait_for(d, s) != build_wait::none;

	chrome_facts facts{
		.kind = kind_of(s),
		.phase = current_phase(s),
		.state = state_of(d, s),
		.steps = phase_timeline(s),
		.status = status_label(d, s),
		.failure = s.info.failure,
		.findings = s.gate ? s.gate->findings : 0,
		.plan_steps = s.gate ? std::span<const build_inbox::plan_step>(s.gate->steps) : std::span<const build_inbox::plan_step>{},
		.plan_summary = gate_summary(s),
		.holding = holding_builds(d, s),
		.blame = s.blame.empty() ? std::string{} : blame_label(s),
		.unbuilt = s.stale ? exe_label(s) : std::string{},
		.busy = busy,
		.build_waiting = waiting,
		.has_gate = s.gate.has_value(),
		.pending = s.pending_transition.has_value(),
		.asking = awaiting_answer(s),
		.goal_waiting = s.goal && s.goal->waiting,
		.hibernating = s.hibernating,
		.stale = s.stale,
		.failed = !s.blame.empty() || !s.info.failure.empty(),
		.summary_ready = summary_ready(s),
	};

	if (const queued_build* queued = queued_for(d, s)) {
		facts.hold = queued->reported;
		facts.blocker = blocker_name(d, queued->blocker);
	}
	if (!d.unclaimed.empty() && is_chat(s) && config::worktree_for(s.cwd).name == d.unclaimed_tree) {
		facts.unclaimed = d.unclaimed.size();
	}
	if (s.hibernating) {
		facts.awaited_build = s.unbuilt.empty() ? std::string{} : unbuilt_label(s);
		facts.asleep_blocker = s.asleep_blocker;
		if (!s.wake_prompt.empty()) {
			facts.wake_task = "on wake: " + std::string(first_line(s.wake_prompt));
		}
	}
	if (s.goal && s.goal->waiting) {
		const await_condition& waiting = *s.goal->waiting;
		if (waiting.process) {
			facts.awaited_pid = waiting.process->pid;
		}
		if (waiting.file) {
			facts.awaited_file = waiting.file->path.filename().generic_display_string();
		}
		facts.wake_in = wake_in(waiting);
		facts.capped = waiting.capped;
		facts.wake_task = "on wake: " + std::string(first_line(waiting.then));
	}
	facts.woke_by = s.woke_by;
	facts.status = build_status(facts);

	if (busy) {
		const time subsecond_limit = seconds(10.f);
		const time elapsed = s.think_clock->elapsed();
		const std::string& action = waiting ? facts.status : s.action;
		facts.activity = elapsed < subsecond_limit
			? std::format("{} \xC2\xB7 {:.1f:s}", action, elapsed)
			: std::format("{} \xC2\xB7 {:.0f:s}", action, elapsed);
	}
	if (s.gate && !s.pending_transition) {
		facts.gate_action = gate_label(s, *s.gate);
	}
	if (s.pending_transition) {
		facts.pending_next = policy_of(next_phase(s, *s.pending_transition)).label;
	}
	facts.followup_row = followup_of(s);
	return facts;
}

auto gse::ide::agent::followup_of(const session& s) -> std::optional<std::uint32_t> {
	if (!s.gate || s.gate->from != task_phase::scope || s.pending_transition || (s.gate->steps.empty() && s.gate->summary.empty())) {
		return std::nullopt;
	}
	if (!std::ranges::contains(s.rows | std::views::drop(s.gate->first_row), row_kind::user, &transcript_row::kind)) {
		return std::nullopt;
	}
	return s.gate->first_row;
}

auto gse::ide::agent::gate_summary(const session& s) -> std::string_view {
	return s.gate && s.gate->from == task_phase::scope ? std::string_view(s.gate->summary) : std::string_view{};
}

auto gse::ide::agent::scope_pages(const std::string_view summary, const std::size_t steps) -> std::vector<summary_page> {
	std::vector<summary_page> pages;
	if (!summary.empty()) {
		pages.push_back({ .phase = task_phase::scope, .kind = page_kind::summary });
	}
	if (steps > 0) {
		pages.push_back({ .phase = task_phase::scope, .kind = page_kind::steps });
	}
	return pages;
}

auto gse::ide::agent::apply_pages(const std::size_t steps, const std::size_t edits) -> std::vector<summary_page> {
	std::vector<summary_page> pages;
	if (steps > 0 || edits > 0) {
		pages.push_back({ .phase = task_phase::apply, .kind = page_kind::changes });
	}
	return pages;
}

auto gse::ide::agent::lifecycle_pages(const std::span<const phase_step> route, const std::string_view summary, const std::size_t steps, const std::size_t edits, const std::size_t cycles, const bool reported, const bool fixing, const bool steered) -> std::vector<summary_page> {
	std::vector<summary_page> pages;
	for (const phase_step& station : route) {
		if (station.phase == task_phase::revise && cycles > 0) {
			continue;
		}
		std::vector<summary_page> screens = station.phase == task_phase::scope ? scope_pages(summary, steps)
			: station.phase == task_phase::apply ? apply_pages(steps, edits)
			: station.phase == task_phase::review && cycles > 0 ? std::vector<summary_page>{ { .phase = task_phase::review, .kind = page_kind::review_loop } }
			: station.phase == task_phase::summarize && reported ? std::vector<summary_page>{ { .phase = task_phase::summarize, .kind = page_kind::report } }
			: station.phase == task_phase::steer && steered ? std::vector<summary_page>{ { .phase = task_phase::steer, .kind = page_kind::verdict } }
			: std::vector<summary_page>{};
		if (station.phase == task_phase::apply && fixing) {
			screens.insert(screens.begin(), { .phase = task_phase::apply, .kind = page_kind::errors });
		}
		if (screens.empty()) {
			screens.push_back({ .phase = station.phase });
		}
		pages.append_range(screens);
	}
	return pages;
}

auto gse::ide::agent::goal_cycles(const session& s) -> std::vector<goal_cycle> {
	std::vector<goal_cycle> out;
	const std::span all(s.runs);
	std::size_t begun = 0;
	for (std::size_t at = 0; at < all.size(); ++at) {
		const phase_run& run = all[at];
		if (out.empty() || run.phase == task_phase::scope) {
			out.push_back({ .started = run.started });
			begun = at;
		}
		goal_cycle& cycle = out.back();
		cycle.runs = all.subspan(begun, at + 1 - begun);
		if (run.phase == task_phase::steer && run.verdict) {
			cycle.verdict = run.verdict;
		}
		if (run.phase == task_phase::review) {
			cycle.findings += run.findings;
		}
		cycle.edits += run.edits.size();
	}
	return out;
}

auto gse::ide::agent::cycle_label(const cycle_paging& paging) -> std::string {
	return paging.position == 0 ? std::string("goal") : std::format("cycle {} of {}", paging.position, paging.cycles);
}

namespace gse::ide::agent {
	auto lifecycle_on(const session& s, const std::span<const phase_run> runs, const bool latest, const std::span<const phase_step> route) -> lifecycle {
		lifecycle out;
		const auto scoped = std::ranges::find_last(runs, task_phase::scope, &phase_run::phase);
		if (latest && s.gate && s.gate->from == task_phase::scope) {
			out.summary = s.gate->summary;
			out.steps = s.gate->steps;
		}
		else if (!scoped.empty()) {
			out.summary = scoped.front().summary;
			out.steps = scoped.front().steps;
		}
		for (const phase_run& run : runs | std::views::filter([](const phase_run& r) { return r.phase == task_phase::apply; })) {
			for (const step_edit& edit : run.edits) {
				out.edits.push_back(&edit);
			}
		}
		const auto applied = std::ranges::find_last(runs, task_phase::apply, &phase_run::phase);
		const bool ended = !applied.empty() && (!latest || &applied.front() != &runs.back());
		out.progress = progress_of(out.steps.size(), applied.empty() ? std::span<const step_state>{} : std::span<const step_state>(applied.front().progress), out.edits, ended);
		out.review = review_loop_of(s, runs);
		if (const auto summarized = std::ranges::find_last(runs, task_phase::summarize, &phase_run::phase); !summarized.empty()) {
			out.report = summarized.front().summary;
		}
		if (const auto steered = std::ranges::find_last_if(runs, [](const phase_run& run) { return run.phase == task_phase::steer && run.verdict; }); !steered.empty()) {
			out.steered = ledger_entry{ .started = steered.front().started, .verdict = *steered.front().verdict, .summary = steered.front().summary };
		}
		out.fix = latest && s.fix ? &*s.fix : nullptr;
		out.pages = lifecycle_pages(route, out.summary, out.steps.size(), out.edits.size(), out.review.cycles.size(), !out.report.empty(), latest && s.kind == chat_kind::build_error, out.steered.has_value());
		for (summary_page& page : out.pages) {
			if (const auto found = std::ranges::find_last(runs, page.phase, &phase_run::phase); !found.empty()) {
				page.run = &found.front();
			}
		}
		return out;
	}
}

auto gse::ide::agent::shown_goal(const session& s) -> const goal_state* {
	if (s.goal) {
		return &*s.goal;
	}
	return s.met_goal ? &*s.met_goal : nullptr;
}

auto gse::ide::agent::lifecycle_at(const session& s, const std::optional<std::uint32_t> position) -> lifecycle {
	if (!shown_goal(s)) {
		return lifecycle_on(s, task_runs(s), true, phase_timeline(s));
	}
	const std::vector<goal_cycle> cycles = goal_cycles(s);
	const auto count = static_cast<std::uint32_t>(cycles.size());
	const std::uint32_t at = std::min(position.value_or(count), count);
	if (at == 0) {
		lifecycle out;
		out.goal = goal_dashboard_of(s);
		out.cycle = { .position = 0, .cycles = count, .shown = true };
		out.pages = { { .phase = task_phase::scope, .kind = page_kind::goal } };
		return out;
	}
	const bool latest = at == count;
	const std::span<const phase_run> runs = cycles[at - 1].runs;
	const std::vector<phase_step> route = phase_timeline(latest ? current_phase(s) : task_phase::settled, true, runs);
	lifecycle out = lifecycle_on(s, runs, latest, route);
	out.cycle = { .position = at, .cycles = count, .shown = true };
	return out;
}

auto gse::ide::agent::lifecycle_of(const session& s) -> lifecycle {
	return lifecycle_at(s, s.lifecycle_cycle);
}

auto gse::ide::agent::summary_ready(const session& s) -> bool {
	if (kind_of(s) == chat_kind::sync) {
		return false;
	}
	if (shown_goal(s)) {
		return true;
	}
	return std::ranges::any_of(lifecycle_at(s, std::nullopt).pages, [](const summary_page& page) { return page.kind != page_kind::empty; });
}

auto gse::ide::agent::fresh_page(const summary_page& page, const std::int64_t since) -> bool {
	return since > 0 && page.run && page.run->started > since;
}

auto gse::ide::agent::since_of(const session& s, const std::int64_t since, const std::int64_t until) -> since_digest {
	since_digest out{ .since = since };
	if (since <= 0) {
		return out;
	}
	const auto unseen = [since](const phase_run& run) {
		return run.started > since;
	};
	const auto away = [since, until](const std::int64_t stamp) {
		return stamp > since && (until <= 0 || stamp <= until);
	};
	out.stages = static_cast<std::size_t>(std::ranges::count_if(s.runs, [&](const phase_run& run) { return away(run.started); }));
	out.verdicts = static_cast<std::size_t>(std::ranges::count_if(s.runs, [&](const phase_run& run) {
		return away(run.started) && run.phase == task_phase::steer && run.verdict;
	}));
	const bool open_question = s.question && !s.question->answered;
	for (const transcript_row& row : s.rows | std::views::filter([&](const transcript_row& r) { return away(r.stamped); })) {
		out.questions = row.kind == row_kind::question && open_question ? 1 : out.questions;
		out.failures += row.kind == row_kind::failure ? 1 : 0;
	}
	out.waiting = s.gate.has_value() || open_question;
	if (!shown_goal(s)) {
		return out;
	}
	const std::vector<goal_cycle> cycles = goal_cycles(s);
	for (std::size_t at = 0; at < cycles.size(); ++at) {
		const auto first = std::ranges::find_if(cycles[at].runs, unseen);
		if (first == cycles[at].runs.end()) {
			continue;
		}
		const auto position = static_cast<std::uint32_t>(at + 1);
		const lifecycle life = lifecycle_at(s, position);
		const task_phase stage = first->phase == task_phase::revise ? task_phase::review : first->phase;
		const auto page = std::ranges::find(life.pages, stage, &summary_page::phase);
		out.anchor = summary_anchor{
			.position = position,
			.page = page == life.pages.end() ? 0u : static_cast<std::uint32_t>(std::distance(life.pages.begin(), page)),
		};
		break;
	}
	return out;
}

auto gse::ide::agent::opening_page(const session& s, const since_digest& since) -> summary_anchor {
	if (since.anchor && !since.waiting) {
		return *since.anchor;
	}
	const lifecycle life = lifecycle_at(s, std::nullopt);
	const task_phase phase = s.gate ? s.gate->from : current_phase(s);
	const task_phase stage = phase == task_phase::settled ? task_phase::summarize
		: phase == task_phase::revise ? task_phase::review
		: phase;
	const auto page = std::ranges::find(life.pages, stage, &summary_page::phase);
	const std::size_t at = page != life.pages.end() ? static_cast<std::size_t>(std::distance(life.pages.begin(), page))
		: life.pages.empty() ? 0 : life.pages.size() - 1;
	return { .position = life.cycle.position, .page = static_cast<std::uint32_t>(at) };
}

auto gse::ide::agent::digest_line(const since_digest& digest) -> std::string {
	if (digest.stages == 0 && digest.questions == 0 && digest.failures == 0) {
		return {};
	}
	std::vector<std::string> parts;
	parts.push_back(std::format("{} stage{} run", digest.stages, digest.stages == 1 ? "" : "s"));
	if (digest.verdicts > 0) {
		parts.push_back(std::format("{} steering verdict{}", digest.verdicts, digest.verdicts == 1 ? "" : "s"));
	}
	if (digest.questions > 0) {
		parts.push_back(std::format("{} question{} waiting", digest.questions, digest.questions == 1 ? "" : "s"));
	}
	if (digest.failures > 0) {
		parts.push_back(std::format("{} failure{}", digest.failures, digest.failures == 1 ? "" : "s"));
	}
	if (digest.waiting) {
		parts.emplace_back("waiting on you");
	}
	const auto at = std::chrono::floor<std::chrono::minutes>(std::chrono::sys_seconds(std::chrono::seconds(digest.since)));
	return std::format("since you were here ({:%Y-%m-%d %H:%M} UTC): {}", at, parts | std::views::join_with(std::string_view(" \xC2\xB7 ")) | std::ranges::to<std::string>());
}

auto gse::ide::agent::fork_about(const summary_page& page, const cycle_paging& cycle) -> std::string {
	const std::string_view stage = policy_of(page.phase).label;
	return cycle.shown ? std::format("{} \xC2\xB7 {}", cycle_label(cycle), stage) : std::string(stage);
}

auto gse::ide::agent::fork_prompt(const session& s, const summary_page& page, const cycle_paging& cycle, const std::string_view question) -> std::string {
	std::string out = std::format(
		"The owner is reading a past phase of the chat '{}' and has a question about it. You are a read-only fork of that chat: answer from the conversation and by reading files, and change nothing.\n\nThe phase: {}.",
		s.name, fork_about(page, cycle));
	if (page.run) {
		const auto at = std::chrono::floor<std::chrono::minutes>(std::chrono::sys_seconds(std::chrono::seconds(page.run->started)));
		out += std::format(" It began {:%Y-%m-%d %H:%M} UTC.", at);
		if (!page.run->summary.empty()) {
			out += std::format("\n\nWhat that phase reported:\n\n{}", page.run->summary);
		}
	}
	out += std::format("\n\nThe owner's question:\n\n{}", question);
	return out;
}

auto gse::ide::agent::error_strip(const build_fix* fix) -> std::vector<strip_line> {
	if (!fix || fix->errors.empty()) {
		return { { .text = "errors not recorded - this chat predates this page; Transcript has the prompt that lists them", .color = &gui::style::color_text_secondary } };
	}
	std::vector<strip_line> out;
	for (const blamed_error& error : fix->errors) {
		if (error.file.empty()) {
			out.push_back({ .text = error.message });
			for (const std::string& line : error.output) {
				out.push_back({ .text = "  " + line, .color = &gui::style::color_text_secondary });
			}
			continue;
		}
		out.push_back({ .text = std::format("{}:{}  {}", error.file.generic_display_string(), error.line, error.message) });
		for (const std::string& note : error.notes) {
			out.push_back({ .text = "  " + note, .color = &gui::style::color_text_secondary });
		}
	}
	if (!fix->denied.empty()) {
		out.push_back({
			.text = std::format("asked and denied: {}", fix->denied | std::views::join_with(std::string_view(", ")) | std::ranges::to<std::string>()),
			.color = &gui::style::color_text_secondary,
		});
	}
	return out;
}

auto gse::ide::agent::errors_head(const build_fix* fix) -> std::string {
	const std::size_t count = fix ? fix->errors.size() : 0;
	const std::string_view diagnosed = fix && !fix->diagnosis.empty() ? "diagnosed" : "no diagnosis yet";
	return std::format("apply \xC2\xB7 build error \xC2\xB7 {} error{} \xC2\xB7 {}", count, count == 1 ? "" : "s", diagnosed);
}

auto gse::ide::agent::goal_dashboard_of(const session& s) -> std::optional<goal_dashboard> {
	const goal_state* shown = shown_goal(s);
	if (!shown) {
		return std::nullopt;
	}
	goal_dashboard out{
		.record = shown->record,
		.aim = shown->aim,
		.steers_left = shown->steers_left,
		.criteria = shown->criteria,
		.route = phase_timeline(s),
	};
	for (const phase_run& run : s.runs | std::views::reverse) {
		if (run.phase == task_phase::steer && run.verdict) {
			out.ledger.push_back({ .started = run.started, .verdict = *run.verdict, .summary = run.summary });
		}
	}
	const bool held = s.gate && s.gate->from == task_phase::steer;
	if (const std::optional<goal_verdict> moving = s.pending_transition ? s.pending_transition->verdict : std::nullopt; !s.goal || moving == goal_verdict::met) {
		out.standing = goal_standing::met;
	}
	else if (held && s.goal->steers_left == 0) {
		out.standing = goal_standing::exhausted;
	}
	else if (held) {
		out.standing = goal_standing::held;
	}
	else if (s.goal->waiting) {
		out.standing = goal_standing::waiting;
		out.waiting = await_label(*s.goal->waiting);
	}
	return out;
}

auto gse::ide::agent::goal_head(const goal_dashboard& goal) -> std::string {
	const std::string_view standing = annotation_from_enum<standing_style>(goal.standing, {}).label;
	std::string out = std::format("goal \xC2\xB7 {}", goal.waiting.empty() ? std::string(standing) : std::format("{} on {}", standing, goal.waiting));
	if (goal.standing != goal_standing::met) {
		out += goal.steers_left == 0 ? std::string(" \xC2\xB7 no steers left") : std::format(" \xC2\xB7 {} steer{} left", goal.steers_left, goal.steers_left == 1 ? "" : "s");
	}
	out += std::format(" \xC2\xB7 {}", goal.record.empty() ? std::string("record unknown") : goal.record.generic_display_string());
	return out;
}

auto gse::ide::agent::criteria_tally(const std::span<const criterion_report> criteria) -> std::string {
	if (criteria.empty()) {
		return "criteria not reported - the next steering run sends them with gse_phase_done";
	}
	std::string out = "criteria";
	for (const criterion_state state : enum_values<criterion_state>()) {
		out += std::format(" \xC2\xB7 {} {}", std::ranges::count(criteria, state, &criterion_report::state), std::string_view(annotation_from_enum<criterion_style>(state, {}).label));
	}
	return out;
}

auto gse::ide::agent::route_line(const std::span<const phase_step> route) -> std::string {
	std::string out = "task";
	for (const phase_step& station : route) {
		const std::string_view label = policy_of(station.phase).label;
		out += station.current ? std::format(" \xE2\x80\xBA [{}]", label)
			: station.visited ? std::format(" \xE2\x80\xBA {} \xE2\x9C\x93", label)
			: std::format(" \xE2\x80\xBA {}", label);
	}
	return out;
}

auto gse::ide::agent::ledger_head(const ledger_entry& entry) -> std::string {
	const auto day = std::chrono::floor<std::chrono::days>(std::chrono::sys_seconds(std::chrono::seconds(entry.started)));
	return std::format("{:%Y-%m-%d} \xC2\xB7 {}", day, enum_to_string(entry.verdict));
}

auto gse::ide::agent::review_loop_of(const session& s) -> review_loop {
	return review_loop_of(s, task_runs(s));
}

auto gse::ide::agent::review_loop_of(const session& s, const std::span<const phase_run> runs) -> review_loop {
	review_loop out;
	for (std::size_t at = 0; at < runs.size(); ++at) {
		const phase_run& run = runs[at];
		const bool reported = !run.summary.empty() || run.findings > 0;
		if (run.phase == task_phase::review) {
			out.cycles.push_back({
				.found = run.summary,
				.findings = run.findings,
				.reviewed = reported,
			});
			continue;
		}
		if (run.phase != task_phase::revise || out.cycles.empty()) {
			continue;
		}
		const std::size_t begun = std::min<std::size_t>(run.first_row, s.rows.size());
		const auto next = static_cast<std::size_t>(&run - s.runs.data()) + 1;
		const std::size_t ended = next < s.runs.size() ? std::clamp<std::size_t>(s.runs[next].first_row, begun, s.rows.size()) : s.rows.size();
		review_cycle& cycle = out.cycles.back();
		cycle.revised = run.summary;
		cycle.revise_begun = true;
		cycle.revise_reported = reported;
		cycle.edits = static_cast<std::size_t>(std::ranges::count_if(std::span(s.rows).subspan(begun, ended - begun), [](const transcript_row& row) {
			return shows(row_filter::changes, row);
		}));
	}
	if (!out.cycles.empty()) {
		const review_cycle& last = out.cycles.back();
		out.capped = last.reviewed && last.findings > 0 && !last.revise_begun && out.cycles.size() >= max_review_cycles;
	}
	return out;
}

auto gse::ide::agent::diff_sections_of(const session& s) -> std::vector<diff_section> {
	std::vector<diff_section> out;
	for (std::size_t at = 0; at < s.rows.size(); ++at) {
		if (!shows(row_filter::changes, s.rows[at])) {
			continue;
		}
		if (std::optional<std::string> label = run_section_of(s, static_cast<std::uint32_t>(at))) {
			out.push_back({ .row = static_cast<std::uint32_t>(at), .label = std::move(*label) });
		}
	}
	return out;
}

auto gse::ide::agent::review_loop_head(const review_loop& loop) -> std::string {
	const std::uint32_t open = loop.cycles.empty() ? 0 : loop.cycles.back().findings;
	return std::format("review loop \xC2\xB7 cycle {} of {} \xC2\xB7 {} open", loop.cycles.size(), max_review_cycles, open);
}

auto gse::ide::agent::review_loop_text(const review_loop& loop) -> std::string {
	std::string out;
	for (std::size_t at = 0; at < loop.cycles.size(); ++at) {
		const review_cycle& cycle = loop.cycles[at];
		const std::size_t number = at + 1;
		const std::string_view next = !cycle.reviewed ? "reviewing"
			: cycle.findings == 0 ? "clean"
			: cycle.revise_begun ? "sent to revise"
			: loop.capped && at + 1 == loop.cycles.size() ? "left open at the cap"
			: "awaiting revise";
		out += std::format("### review {} \xC2\xB7 {} finding{} \xC2\xB7 {}\n\n", number, cycle.findings, cycle.findings == 1 ? "" : "s", next);
		if (!cycle.found.empty()) {
			out += std::format("{}\n\n", cycle.found);
		}
		if (cycle.revise_begun) {
			out += std::format("### revise {} \xC2\xB7 {} edit{} \xC2\xB7 {}\n\n", number, cycle.edits, cycle.edits == 1 ? "" : "s", cycle.revise_reported ? std::format("Diffs \xE2\x80\xBA revise {}", number) : std::string("in progress"));
			if (!cycle.revised.empty()) {
				out += std::format("{}\n\n", cycle.revised);
			}
		}
	}
	if (loop.capped) {
		out += std::format("review limit reached - {} finding(s) left open\n", loop.cycles.back().findings);
	}
	return out;
}

auto gse::ide::agent::report_head(const step_progress& progress, const review_loop& loop) -> std::string {
	std::string out = "summarize \xC2\xB7 report";
	if (!progress.states.empty()) {
		out += progress.reported
			? std::format(" \xC2\xB7 steps {}/{}", std::ranges::count(progress.states, step_state::done), progress.states.size())
			: std::format(" \xC2\xB7 steps unreported");
	}
	if (!loop.cycles.empty()) {
		const std::uint32_t open = loop.capped ? loop.cycles.back().findings : 0;
		out += std::format(" \xC2\xB7 review {} cycle{}, {} open", loop.cycles.size(), loop.cycles.size() == 1 ? "" : "s", open);
	}
	return out;
}

auto gse::ide::agent::edits_label(const std::size_t edits) -> std::string {
	return std::format("{} edit{}", edits, edits == 1 ? "" : "s");
}

auto gse::ide::agent::task_edits(const lifecycle& life) -> std::size_t {
	return std::ranges::fold_left(life.review.cycles | std::views::transform(&review_cycle::edits), life.edits.size(), std::plus{});
}

auto gse::ide::agent::progress_of(const std::size_t steps, const std::span<const step_state> reported, const std::span<const step_edit* const> edits, const bool ended) -> step_progress {
	step_progress out{
		.states = std::vector(steps, step_state::remaining),
		.edits = std::vector<std::size_t>(steps, 0),
		.reported = !reported.empty(),
		.ended = ended,
	};
	std::ranges::copy(reported | std::views::take(steps), out.states.begin());
	if (ended) {
		std::ranges::replace_if(out.states, [](const step_state state) { return state != step_state::done; }, step_state::unreported);
	}
	for (const step_edit* edit : edits) {
		if (edit->step && *edit->step < steps) {
			++out.edits[*edit->step];
		}
		else {
			++out.unattributed;
		}
	}
	return out;
}

auto gse::ide::agent::edits_in_group(const std::span<const step_edit* const> edits, const std::size_t steps, const std::optional<std::size_t> group) -> std::vector<const step_edit*> {
	const auto in_group = [&](const step_edit* edit) {
		if (!group) {
			return true;
		}
		const bool attributed = edit->step && *edit->step < steps;
		return *group == steps ? !attributed : attributed && *edit->step == *group;
	};
	return edits | std::views::filter(in_group) | std::ranges::to<std::vector>();
}

auto gse::ide::agent::paging_of(const std::span<const summary_page> pages, const std::uint32_t chosen) -> plan_paging {
	if (pages.empty()) {
		return {};
	}
	const auto at = std::min<std::size_t>(chosen, pages.size() - 1);
	return {
		.page = static_cast<std::uint32_t>(at + 1),
		.pages = static_cast<std::uint32_t>(pages.size()),
		.shown = pages[at],
	};
}

auto gse::ide::agent::plan_from(const chrome_facts& facts) -> chrome_plan {
	chrome_plan plan{
		.steps = facts.kind == chat_kind::sync ? std::vector<phase_step>{} : facts.steps,
		.status = facts.status,
		.status_color = style_of(facts.state).color,
		.activity = facts.activity,
		.activity_color = style_of(facts.state).activity,
		.activity_shown = facts.busy,
		.overview_activity = facts.busy && !facts.build_waiting,
		.stop_enabled = facts.busy || facts.goal_waiting,
		.stop_interrupts = facts.busy,
		.question_shown = facts.asking,
		.fork_allowed = !facts.busy,
		.summary_ready = facts.summary_ready,
		.toggle_shown = facts.summary_ready,
		.tab = {
			.dirty = facts.hibernating || facts.has_gate || facts.asking || facts.build_waiting,
			.busy = facts.busy,
			.warning = facts.stale,
			.error = facts.failed,
			.dimmed = facts.state == agent_state::failed,
		},
	};

	for (const chrome_note note : enum_values<chrome_note>()) {
		const note_style style = annotation_from_enum<note_style>(note, {});
		if (std::string text = style.text(facts); !text.empty()) {
			plan.note = std::move(text);
			plan.note_color = style.color;
			break;
		}
	}

	plan.footer = facts.unbuilt;
	plan.wake_task = facts.wake_task;
	if (facts.hibernating || facts.build_waiting || facts.goal_waiting || facts.asking || (facts.has_gate && !facts.pending)) {
		const std::string asleep = asleep_note(facts);
		plan.waiting = asleep.empty() ? facts.status : std::format("{} · {}", facts.status, asleep);
		plan.waiting_color = plan.status_color;
	}
	if (!facts.blame.empty()) {
		plan.footer += plan.footer.empty() ? "broke " + facts.blame : " · broke " + facts.blame;
	}

	if (facts.asking) {
		plan.action = chrome_action::answer;
		plan.action_label = answer_label;
	}
	else if (facts.has_gate && !facts.pending) {
		plan.action = chrome_action::approve;
		plan.action_label = facts.gate_action;
		plan.plan_shown = !facts.plan_steps.empty() || !facts.plan_summary.empty();
		plan.plan_steps = facts.plan_steps;
		plan.plan_summary = facts.plan_summary;
		plan.plan_split = plan.plan_shown && facts.followup_row.has_value();
		plan.followup_row = facts.followup_row.value_or(0);
	}
	else {
		plan.action = chrome_action::stop;
	}
	return plan;
}

auto gse::ide::agent::build_status(const chrome_facts& facts) -> std::string {
	std::string out = facts.status;
	if (facts.state == agent_state::build_queued && facts.hold != build_hold::none) {
		const hold_style style = annotation_from_enum<hold_style>(facts.hold, {});
		out += style.names_blocker
			? std::format(" {} '{}'", std::string_view(style.brief), facts.blocker)
			: std::format(" {}", std::string_view(style.brief));
	}
	for (const wait_reason reason : enum_values<wait_reason>()) {
		if (const std::string clause = annotation_from_enum<wait_style>(reason, {}).text(facts); !clause.empty()) {
			out += " · " + clause;
		}
	}
	if (facts.holding > 0) {
		out += std::format(" · holding {} build{}", facts.holding, facts.holding == 1 ? "" : "s");
	}
	return out;
}

auto gse::ide::agent::failure_note(const chrome_facts& facts) -> std::string {
	return facts.state == agent_state::failed ? facts.failure : std::string{};
}

auto gse::ide::agent::pending_note(const chrome_facts& facts) -> std::string {
	return facts.pending ? std::format("approved · finishing the turn before {}", facts.pending_next) : std::string{};
}

auto gse::ide::agent::findings_note(const chrome_facts& facts) -> std::string {
	if (!facts.has_gate) {
		return {};
	}
	return facts.findings > 0 ? std::format("reported {} finding(s)", facts.findings) : std::string("reported done");
}

auto gse::ide::agent::blame_note(const chrome_facts& facts) -> std::string {
	return facts.blame.empty() ? std::string{} : "broke " + facts.blame;
}

auto gse::ide::agent::unclaimed_note(const chrome_facts& facts) -> std::string {
	return facts.unclaimed > 0 ? std::format("build red · {} error(s) nobody claims", facts.unclaimed) : std::string{};
}

auto gse::ide::agent::unbuilt_note(const chrome_facts& facts) -> std::string {
	return facts.unbuilt;
}

auto gse::ide::agent::asleep_note(const chrome_facts& facts) -> std::string {
	return facts.hibernating ? facts.asleep_blocker : std::string{};
}

auto gse::ide::agent::woke_note(const chrome_facts& facts) -> std::string {
	return facts.busy && !facts.woke_by.empty() ? "woke: " + facts.woke_by : std::string{};
}

auto gse::ide::agent::build_wait_clause(const chrome_facts& facts) -> std::string {
	if (!facts.hibernating) {
		return {};
	}
	return facts.awaited_build.empty() ? std::string("until the next build") : std::format("until {} is built", facts.awaited_build);
}

auto gse::ide::agent::process_wait_clause(const chrome_facts& facts) -> std::string {
	return facts.goal_waiting && facts.awaited_pid ? std::format("pid {} to exit", *facts.awaited_pid) : std::string{};
}

auto gse::ide::agent::file_wait_clause(const chrome_facts& facts) -> std::string {
	return facts.goal_waiting && !facts.awaited_file.empty() ? std::format("'{}' to change", facts.awaited_file) : std::string{};
}

auto gse::ide::agent::deadline_wait_clause(const chrome_facts& facts) -> std::string {
	if (!facts.goal_waiting) {
		return {};
	}
	const std::string within = std::format("wakes within {:.0f:min}", facts.wake_in);
	return facts.capped ? std::format("{} · capped at {:.0f:h}", within, longest_await) : within;
}

auto gse::ide::agent::plan_chrome(const data& d, const session& s) -> chrome_plan {
	return plan_from(facts_of(d, s));
}

auto gse::ide::agent::excluded_by(const chat_kind kind, const task_phase phase, const agent_state state) -> std::string_view {
	if (kind == chat_kind::sync && phase != task_phase::apply) {
		return "a sync chat never begins a phase, so current_phase reads apply for it";
	}
	if (phase == task_phase::steer && kind != chat_kind::goal) {
		return "steer is entered only by a chat with a goal";
	}
	if (state == agent_state::awaiting_goal && kind != chat_kind::goal) {
		return "awaiting_goal needs a goal's await";
	}
	if (state == agent_state::editing && policy_of(phase).read_only) {
		return "a read-only phase cannot write, so editing cannot occur";
	}
	if (state == agent_state::awaiting_approval) {
		if (kind == chat_kind::goal && phase != task_phase::steer && phase != task_phase::summarize) {
			return "a goal chat gates only on an escalation, a pause or a spent steer budget, all reported from steer or summarize";
		}
		if (kind != chat_kind::goal && (!policy_of(phase).gated || style_of(kind).unattended)) {
			return "without a goal only a gated phase (scope) of an attended chat waits for approval";
		}
	}
	return {};
}

auto gse::ide::agent::draw_phase_pipeline(const gui::draw_context& ctx, const std::span<const phase_step> steps, const id underline_key, const rectf& area, const rectf& clip) -> float {
	const gui::style& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float fs = sty.font_size;
	const float baseline = area.center().y() + text_view->vertical_center_offset(fs);
	constexpr std::string_view separator = " \xC2\xB7 ";

	float x = area.left();
	bool first = true;
	std::optional<rectf> underline;
	vec4f underline_color = sty.color_text_secondary;
	for (const phase_step& step : steps) {
		if (!first) {
			ctx.queue_text({
				.font = ctx.fonts.text,
				.text = separator,
				.position = { x, baseline },
				.scale = fs,
				.color = sty.color_text_disabled,
				.clip_rect = clip,
			});
			x += text_view->width(separator, fs);
		}
		first = false;

		const phase_policy policy = policy_of(step.phase);
		const std::string label = step.cycles > 1
			? std::format("{} x{}", std::string_view(policy.label), step.cycles)
			: std::string(policy.label);
		const float label_w = text_view->width(label, fs);

		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = label,
			.position = { x, baseline },
			.scale = fs,
			.color = step.current ? sty.*policy.color : step.visited ? sty.color_text_secondary : sty.color_text_disabled,
			.clip_rect = clip,
		});

		if (step.current) {
			underline = rectf::from_position_size(
				{ x, area.center().y() - fs * 0.5f },
				{ label_w, std::max(1.f, std::round(sty.scale_factor)) }
			);
			underline_color = sty.*policy.color;
		}
		x += label_w;
	}

	if (underline) {
		ctx.queue_sprite({
			.rect = ctx.animated_rect(underline_key, *underline),
			.color = underline_color,
			.texture = ctx.blank_texture,
			.clip_rect = clip,
		});
	}

	return x - area.left();
}

auto gse::ide::agent::draw_activity_line(const gui::draw_context& ctx, const std::string_view activity, vec4f gui::style::* const color, const float presence, const rectf& area, const rectf& clip) -> void {
	const gui::style& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const float spin_extent = sty.font_size;

	const rectf spin = rectf::from_position_size(
		{ area.left(), area.center().y() + spin_extent * 0.5f },
		{ spin_extent, spin_extent }
	);
	gui::symbol::spinner(ctx, spin, {
		.color = fade(sty.*color, presence),
		.extent = sty.icon_extent,
		.clip_rect = clip,
	});

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = activity,
		.position = { spin.right() + pad * 0.5f, area.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = fade(sty.*color, presence),
		.clip_rect = clip,
	});
}

auto gse::ide::agent::draw_status_row(const gui::draw_context& ctx, const chrome_plan& plan, const id key, const rectf& area, const rectf& clip) -> void {
	const gui::style& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const bool waiting = !plan.waiting.empty();

	ctx.queue_sprite({
		.rect = area,
		.color = ctx.animated_color(generate_temp_id(hash_of(key.number(), "wait")), fade(sty.*plan.waiting_color, waiting ? error_strip_fill : 0.f)),
		.texture = ctx.blank_texture,
		.clip_rect = clip,
	});

	const float used = draw_phase_pipeline(ctx, plan.steps, generate_temp_id(hash_of(key.number(), "underline")), area, clip);
	float x = area.left() + used + pad * 2.f;

	if (!plan.note.empty()) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = plan.note,
			.position = { x, area.center().y() + text_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = sty.*plan.note_color,
			.clip_rect = clip,
		});
		x += text_view->width(plan.note, fs) + pad * 2.f;
	}

	if (waiting) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = ctx.intern(plan.waiting),
			.position = { x, area.center().y() + text_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = sty.*plan.waiting_color,
			.clip_rect = clip,
		});
	}

	const float shown = ctx.presence(generate_temp_id(hash_of(key.number(), "activity")), plan.activity_shown && !waiting);
	if (shown <= 0.f) {
		return;
	}
	draw_activity_line(ctx, plan.activity, plan.activity_color, shown, rectf::from_position_size(
		{ x, area.top() },
		{ std::max(0.f, area.right() - x), area.height() }
	), clip);
}

auto gse::ide::agent::gallery_facts(const chat_kind kind, const task_phase phase, const agent_state state) -> chrome_facts {
	const bool busy = state == agent_state::working || state == agent_state::editing;
	const bool gated = state == agent_state::awaiting_approval;
	const std::string status = style_of(state).label;

	return {
		.kind = kind,
		.phase = phase,
		.state = state,
		.steps = phase_timeline(phase, kind == chat_kind::goal, {}),
		.status = status,
		.activity = busy ? std::format("{} \xC2\xB7 {}", std::string_view(annotation_from_enum<tool_flavor>(state == agent_state::editing ? tool_kind::edit : tool_kind::read, {}).verb), gallery_elapsed) : std::string{},
		.gate_action = gated ? std::string(policy_of(phase == task_phase::scope ? task_phase::apply : task_phase::steer).enter_label) : std::string{},
		.failure = state == agent_state::failed ? std::string(gallery_failure) : std::string{},
		.busy = busy,
		.build_waiting = state == agent_state::build_queued || state == agent_state::compiling,
		.has_gate = gated,
		.asking = state == agent_state::awaiting_answer,
		.goal_waiting = state == agent_state::awaiting_goal,
		.hibernating = state == agent_state::hibernating,
		.failed = state == agent_state::failed,
	};
}

auto gse::ide::agent::action_name(const chrome_plan& plan) -> std::string {
	switch (plan.action) {
		case chrome_action::answer:
		case chrome_action::approve:
			return std::format("[{}]", plan.action_label);
		case chrome_action::stop:
			return plan.stop_enabled ? std::string(plan.stop_interrupts ? "[stop]" : "[pause]") : std::string("[stop, disabled]");
		case chrome_action::none:
			break;
	}
	return {};
}

auto gse::ide::agent::draw_chrome_gallery(const canvas_frame& frame, const drawables::chrome_gallery& gallery) -> void {
	const gui::draw_context& ctx = frame.ctx;
	const gui::style& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float fs = sty.font_size;
	const float pad = sty.padding;
	const float row_h = fs * 1.75f;
	const float label_w = fs * 16.f;
	const float action_w = fs * 9.f;

	const vec2f origin = { frame.area.left() + gallery.position[0] * frame.scale, frame.area.top() - gallery.position[1] * frame.scale };
	const float width = gallery.width * frame.scale;
	std::size_t cell = 0;

	const auto draw_cell = [&](const std::string& name, const chrome_plan& plan) {
		const rectf row = rectf::from_position_size(
			{ origin[0], origin[1] - row_h * static_cast<float>(cell) },
			{ width, row_h }
		);
		const rectf clip = frame.clip.intersection(row);
		const float baseline = row.center().y() + text_view->vertical_center_offset(fs);

		ctx.queue_sprite({
			.rect = row,
			.color = cell % 2 == 0 ? sty.panel_alt.fill : sty.list_item.surfaces.idle.fill,
			.texture = ctx.blank_texture,
			.clip_rect = frame.clip,
		});
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = ctx.intern(name),
			.position = { row.left() + pad, baseline },
			.scale = fs,
			.color = sty.color_text_secondary,
			.clip_rect = clip.intersection(rectf::from_position_size(row.top_left(), { label_w, row_h })),
		});
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = ctx.intern(plan.status),
			.position = { row.left() + label_w + pad, baseline },
			.scale = fs,
			.color = sty.*plan.status_color,
			.clip_rect = clip,
		});

		const float status_right = row.left() + label_w + pad * 2.f + text_view->width(plan.status, fs);
		const rectf status_area = rectf::from_position_size(
			{ status_right + pad, row.top() },
			{ std::max(0.f, row.right() - action_w - status_right - pad * 2.f), row_h }
		);
		draw_status_row(ctx, plan, generate_temp_id(hash_of(frame.canvas.number(), cell)), status_area, clip.intersection(status_area));

		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = ctx.intern(action_name(plan)),
			.position = { row.right() - action_w, baseline },
			.scale = fs,
			.color = sty.color_text,
			.clip_rect = clip,
		});
		++cell;
	};

	for (const chat_kind kind : enum_values<chat_kind>()) {
		if (!gallery.kind.empty() && enum_to_string(kind) != gallery.kind) {
			continue;
		}
		for (const task_phase phase : phase_order) {
			for (const agent_state state : enum_values<agent_state>()) {
				if (excluded_by(kind, phase, state).empty()) {
					draw_cell(std::format("{} \xC2\xB7 {} \xC2\xB7 {}", enum_to_string(kind), enum_to_string(phase), enum_to_string(state)), plan_from(gallery_facts(kind, phase, state)));
				}
			}
		}
	}

	chrome_facts findings = gallery_facts(chat_kind::chat, task_phase::scope, agent_state::awaiting_approval);
	findings.findings = gallery_findings;
	draw_cell("approval \xC2\xB7 with findings", plan_from(findings));

	chrome_facts pending = gallery_facts(chat_kind::chat, task_phase::scope, agent_state::working);
	pending.pending = true;
	pending.pending_next = policy_of(task_phase::apply).label;
	draw_cell("approval \xC2\xB7 approved, turn finishing", plan_from(pending));

	for (const bool ready : { false, true }) {
		chrome_facts fresh = gallery_facts(chat_kind::chat, task_phase::scope, ready ? agent_state::awaiting_approval : agent_state::working);
		fresh.summary_ready = ready;
		const chrome_plan plan = plan_from(fresh);
		draw_cell(std::format("view \xC2\xB7 {} \xC2\xB7 {}, toggle {}", ready ? "first summary" : "fresh chat", plan.summary_ready ? "summary" : "transcript", plan.toggle_shown ? "shown" : "hidden"), plan);
	}

	const auto build_cell = [&](const std::string& name, const agent_state state, const auto& adjust) {
		chrome_facts facts = gallery_facts(chat_kind::chat, task_phase::apply, state);
		adjust(facts);
		facts.status = build_status(facts);
		draw_cell(name, plan_from(facts));
	};
	build_cell("build \xC2\xB7 queued behind a build", agent_state::build_queued, [](chrome_facts& f) { f.hold = build_hold::building; });
	build_cell("build \xC2\xB7 queued, editor restart", agent_state::build_queued, [](chrome_facts& f) { f.hold = build_hold::editor_restart; });
	build_cell("build \xC2\xB7 queued, tree busy", agent_state::build_queued, [](chrome_facts& f) {
		f.hold = build_hold::tree_busy;
		f.blocker = gallery_blocker;
	});
	build_cell("build \xC2\xB7 compiling", agent_state::compiling, [](chrome_facts&) {});
	build_cell("build \xC2\xB7 red, blamed", agent_state::resting, [](chrome_facts& f) {
		f.blame = gallery_blame;
		f.failed = true;
	});
	build_cell("build \xC2\xB7 red, unclaimed", agent_state::resting, [](chrome_facts& f) { f.unclaimed = gallery_unclaimed; });
	build_cell("build \xC2\xB7 unbuilt", agent_state::resting, [](chrome_facts& f) {
		f.unbuilt = gallery_unbuilt;
		f.stale = true;
	});
	build_cell("build \xC2\xB7 holding builds", agent_state::working, [](chrome_facts& f) { f.holding = gallery_holding; });

	const auto wait_cell = [&](const std::string& name, const task_phase phase, const agent_state state, const auto& adjust) {
		chrome_facts facts = gallery_facts(phase == task_phase::steer ? chat_kind::goal : chat_kind::chat, phase, state);
		adjust(facts);
		facts.status = build_status(facts);
		draw_cell(name, plan_from(facts));
	};
	wait_cell("hibernate · on a build", task_phase::apply, agent_state::hibernating, [](chrome_facts& f) { f.awaited_build = gallery_awaited_build; });
	wait_cell("hibernate · blocked by a chat", task_phase::apply, agent_state::hibernating, [](chrome_facts& f) {
		f.awaited_build = gallery_awaited_build;
		f.asleep_blocker = gallery_red_blocker;
	});
	wait_cell("await · on a pid", task_phase::steer, agent_state::awaiting_goal, [](chrome_facts& f) {
		f.awaited_pid = gallery_pid;
		f.wake_in = gallery_wake_in;
	});
	wait_cell("await · on a file", task_phase::steer, agent_state::awaiting_goal, [](chrome_facts& f) {
		f.awaited_file = gallery_file;
		f.wake_in = gallery_wake_in;
	});
	wait_cell("await · capped at 48h", task_phase::steer, agent_state::awaiting_goal, [](chrome_facts& f) {
		f.awaited_file = gallery_file;
		f.wake_in = longest_await;
		f.capped = true;
	});
	wait_cell("await · fired, waking", task_phase::steer, agent_state::working, [](chrome_facts& f) { f.woke_by = gallery_fired; });

	float top = origin[1] - row_h * static_cast<float>(cell) - pad;
	const std::vector<std::pair<pending_question, float>> samples = question_samples();
	for (std::size_t index = 0; index < samples.size(); ++index) {
		const auto& [sample, room] = samples[index];
		const question_block laid = layout_question(ctx, sample, width, room * fs);
		const rectf area = rectf::from_position_size({ origin[0], top }, { width, laid.extent });

		std::vector<gui::interaction::press> presses;
		for (std::size_t at = 0; at < laid.options.size(); ++at) {
			presses.push_back({
				.widget = generate_temp_id(hash_of(frame.canvas.number(), "question", index, at)),
				.selected = sample.options[at].chosen,
			});
		}
		draw_question_body(ctx, laid, area, presses);
		top -= laid.extent + pad;
	}

	const std::vector<build_inbox::plan_step> sample = plan_sample();
	chrome_facts scoped = gallery_facts(chat_kind::chat, task_phase::scope, agent_state::awaiting_approval);
	const auto plan_cell = [&](const std::string& name, const chrome_plan& plan, const std::span<const summary_page> pages, const std::uint32_t page, const step_progress& progress, const std::span<const step_edit* const> edits, const std::optional<std::size_t> group, const review_loop& loop = {}, const std::size_t total_edits = 0, const build_fix* fix = nullptr, const goal_dashboard* goal = nullptr, const std::string_view cycle = {}, const std::string_view verdict = {}) {
		const float label_baseline = top - row_h * 0.5f + text_view->vertical_center_offset(fs);
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = ctx.intern(std::format("{} \xC2\xB7 {}", name, action_name(plan))),
			.position = { origin[0] + pad, label_baseline },
			.scale = fs,
			.color = sty.color_text_secondary,
			.clip_rect = frame.clip,
		});
		top -= row_h;
		if (!plan.waiting.empty()) {
			const rectf status = rectf::from_position_size({ origin[0], top }, { width, row_h });
			draw_status_row(ctx, plan, generate_temp_id(hash_of(frame.canvas.number(), "plan_status", name)), status, status.intersection(frame.clip));
			top -= row_h + pad;
		}

		const float column = plan.plan_split ? width * (1.f - annotation_from_enum<split_style>(split_kind::plan_followup, {}).ratio) : 0.f;
		const float plan_w = width - column;
		const plan_paging paging = paging_of(pages, page);
		const bool reviewing = paging.shown.kind == page_kind::review_loop;
		const bool reporting = paging.shown.kind == page_kind::report;
		const bool erroring = paging.shown.kind == page_kind::errors;
		const std::string review_text = reviewing ? review_loop_text(loop) : std::string{};
		transcript_view summary;
		const bool judging = paging.shown.kind == page_kind::verdict;
		sync_plan_summary(ctx, summary, reviewing ? std::string_view(review_text) : reporting ? gallery_report : judging ? verdict : plan.plan_summary.empty() ? gallery_plan_summary : plan.plan_summary, plan_w);
		if (erroring) {
			sync_error_page(ctx, summary, fix, plan_w);
		}
		const bool actionable = plan.plan_shown && paging.shown.phase == task_phase::scope;
		const bool changes = paging.shown.kind == page_kind::changes;
		const std::span<const build_inbox::plan_step> drawn = plan.plan_steps.empty() ? std::span<const build_inbox::plan_step>(sample) : plan.plan_steps;
		plan_block laid = layout_plan(ctx, drawn, progress, summary.buffer.lines.size(), paging, actionable ? std::string_view(plan.action_label) : std::string_view{}, annotation_from_enum<split_style>(plan_split_kind(paging.shown.kind), {}).ratio, plan_w, (changes ? gallery_changes_room : paging.shown.kind == page_kind::goal ? gallery_goal_room : gallery_plan_room) * fs);
		if (reviewing) {
			laid.head = review_loop_head(loop);
		}
		if (reporting) {
			laid.head = report_head(progress, loop);
			laid.diffs_link = edits_label(total_edits);
		}
		if (erroring) {
			laid.head = errors_head(fix);
		}
		if (judging) {
			laid.head = std::format("steer \xC2\xB7 {}", ledger_head({ .started = gallery_steered_at, .verdict = goal_verdict::task, .summary = verdict }));
		}
		laid.cycle_label = cycle;
		const bool steering = paging.shown.kind == page_kind::goal;
		if (steering) {
			layout_goal(ctx, laid, goal, gallery_goal_room * fs);
			sync_ledger(ctx, summary, goal, laid.files_w);
		}
		const rectf area = rectf::from_position_size({ origin[0], top }, { plan_w, laid.extent });
		if (plan.plan_split) {
			const rectf side = rectf::from_position_size({ area.right(), top }, { column, laid.extent });
			const float thickness = std::max(1.f, std::round(sty.scale_factor));
			ctx.queue_sprite({
				.rect = rectf::from_position_size(side.top_left(), { thickness, side.height() }),
				.color = sty.color_text_disabled,
				.texture = ctx.blank_texture,
				.clip_rect = frame.clip,
			});
			float line = side.top() - pad - row_h * 0.5f;
			for (const auto& [said, color] : gallery_followup) {
				ctx.queue_text({
					.font = ctx.fonts.text,
					.text = said,
					.position = { side.left() + pad, line + text_view->vertical_center_offset(fs) },
					.scale = fs,
					.color = sty.*color,
					.clip_rect = side.intersection(frame.clip),
				});
				line -= row_h;
			}
		}
		const auto focus_row = std::ranges::find(laid.groups, group.value_or(gallery_plan_focus));
		const std::optional<std::size_t> focused = focus_row == laid.groups.end() ? std::nullopt : std::optional<std::size_t>(std::distance(laid.groups.begin(), focus_row));
		const bool selectable = paging.shown.kind == page_kind::steps || changes;
		std::vector<gui::interaction::press> presses;
		for (std::size_t at = 0; at < laid.steps.size(); ++at) {
			presses.push_back({
				.widget = generate_temp_id(hash_of(frame.canvas.number(), "plan", at)),
				.hovered = selectable && at == focused,
				.selected = changes && group && at == focused,
			});
		}
		draw_plan_body(ctx, laid, area, presses, focused, 1.f, generate_temp_id(hash_of(frame.canvas.number(), "plan_body", name)));

		const plan_rects rects = plan_rects_of(ctx, laid, area);
		id hot;
		id active;
		id focus;
		if (laid.paging.shown.kind == page_kind::summary || reviewing || reporting || erroring || judging) {
			draw_plan_summary(ctx, summary, rects.summary.intersection(frame.clip), 1.f, generate_temp_id(hash_of(frame.canvas.number(), "plan_summary", name)), hot, active, focus);
		}
		if (steering) {
			draw_plan_summary(ctx, summary, rects.diffs.intersection(frame.clip), 1.f, generate_temp_id(hash_of(frame.canvas.number(), "goal_ledger", name)), hot, active, focus);
		}
		if (changes) {
			transcript_view diffs;
			std::vector<transcript_row> rows;
			sync_step_diffs(ctx, diffs, rows, edits_in_group(edits, sample.size(), group), group, laid.files_w);
			draw_plan_summary(ctx, diffs, rects.diffs.intersection(frame.clip), 1.f, generate_temp_id(hash_of(frame.canvas.number(), "step_diffs", name)), hot, active, focus);
		}

		std::vector<std::pair<rectf, std::string_view>> buttons;
		if (!laid.approve.empty()) {
			buttons.emplace_back(rects.revise, revise_label);
			buttons.emplace_back(rects.approve, std::string_view(laid.approve));
		}
		if (laid.paging.pages > 1) {
			buttons.emplace_back(rects.back, page_back_label);
			buttons.emplace_back(rects.forward, page_forward_label);
		}
		if (!laid.cycle_label.empty()) {
			buttons.emplace_back(rects.cycle_back, cycle_back_label);
			buttons.emplace_back(rects.cycle_forward, cycle_forward_label);
		}
		for (const auto& [rect, label] : buttons) {
			gui::draw::fill_surface(ctx, rect, sty.list_item.surfaces.hot, { .clip_rect = frame.clip });
			ctx.queue_text({
				.font = ctx.fonts.text,
				.text = label,
				.position = { rect.left() + pad, rect.center().y() + text_view->vertical_center_offset(fs) },
				.scale = fs,
				.color = sty.color_text,
				.clip_rect = frame.clip,
			});
		}
		top -= laid.extent + pad;
	};

	scoped.plan_summary = gallery_plan_summary;
	scoped.plan_steps = sample;
	const chrome_plan gated = plan_from(scoped);
	const chrome_plan approved = plan_from(gallery_facts(chat_kind::chat, task_phase::apply, agent_state::working));
	const std::vector<phase_step> route = phase_timeline(task_phase::apply, false, {});
	const std::vector<step_edit> edit_sample = step_edit_sample();
	const std::vector<const step_edit*> edits = edit_sample | std::views::transform([](const step_edit& edit) { return &edit; }) | std::ranges::to<std::vector>();
	const std::vector<summary_page> pages = lifecycle_pages(route, gallery_plan_summary, sample.size(), edits.size(), 0, false, false, false);
	const std::vector<summary_page> unstepped = lifecycle_pages(route, gallery_plan_summary, 0, 0, 0, false, false, false);
	const auto page_of = [&](const page_kind kind) {
		return static_cast<std::uint32_t>(std::distance(pages.begin(), std::ranges::find(pages, kind, &summary_page::kind)));
	};
	const step_progress unreported = progress_of(sample.size(), {}, {}, false);
	const std::array midway = { step_state::done, step_state::current, step_state::remaining, step_state::remaining };
	const std::vector finished(sample.size(), step_state::done);
	const step_progress attributed = progress_of(sample.size(), midway, edits, false);
	plan_cell("summary \xC2\xB7 scope summary, gate pending", gated, pages, 0, {}, {}, std::nullopt);
	plan_cell("summary \xC2\xB7 scope steps, gate pending", gated, pages, 1, {}, {}, std::nullopt);
	plan_cell("summary \xC2\xB7 scope summary after approval", approved, pages, 0, {}, {}, std::nullopt);
	plan_cell("summary \xC2\xB7 stage with no screen yet", approved, pages, page_of(page_kind::empty), {}, {}, std::nullopt);
	plan_cell("summary \xC2\xB7 scope without steps", gated, unstepped, 0, {}, {}, std::nullopt);

	chrome_facts followed = scoped;
	followed.followup_row = 0;
	const chrome_plan split = plan_from(followed);
	plan_cell("split \xC2\xB7 gate held, no follow-up", gated, pages, 1, {}, {}, std::nullopt);
	plan_cell("split \xC2\xB7 followed up, steps", split, pages, 1, {}, {}, std::nullopt);
	plan_cell("split \xC2\xB7 followed up, summary", split, pages, 0, {}, {}, std::nullopt);
	plan_cell("apply \xC2\xB7 unreported", approved, pages, page_of(page_kind::changes), unreported, edits, std::nullopt);
	plan_cell("apply \xC2\xB7 mid-way, current step", approved, pages, page_of(page_kind::changes), attributed, edits, std::nullopt);
	plan_cell("apply \xC2\xB7 ended, partial marks", approved, pages, page_of(page_kind::changes), progress_of(sample.size(), midway, edits, true), edits, std::nullopt);
	plan_cell("apply \xC2\xB7 all done", approved, pages, page_of(page_kind::changes), progress_of(sample.size(), finished, edits, false), edits, std::nullopt);
	plan_cell("apply \xC2\xB7 step selected", approved, pages, page_of(page_kind::changes), attributed, edits, 1);

	chrome_facts slept = gallery_facts(chat_kind::chat, task_phase::apply, agent_state::hibernating);
	slept.awaited_build = gallery_awaited_build;
	slept.asleep_blocker = gallery_red_blocker;
	slept.status = build_status(slept);
	plan_cell("opening \xC2\xB7 apply hibernating on a build", plan_from(slept), pages, page_of(page_kind::changes), attributed, edits, std::nullopt);
	plan_cell("opening \xC2\xB7 scope held for approval", gated, pages, 0, {}, {}, std::nullopt);

	const review_loop clean{ .cycles = { { .found = gallery_review_clean, .reviewed = true } } };
	const review_loop revised{ .cycles = {
		{ .found = gallery_review_found, .findings = gallery_findings, .reviewed = true, .revised = gallery_revise_did, .edits = gallery_revise_edits, .revise_begun = true, .revise_reported = true },
		{ .found = gallery_review_clean, .reviewed = true },
	} };
	review_loop capped{ .capped = true };
	capped.cycles.assign(max_review_cycles, { .found = gallery_review_found, .findings = gallery_findings, .reviewed = true, .revised = gallery_revise_did, .edits = gallery_revise_edits, .revise_begun = true, .revise_reported = true });
	capped.cycles.back() = { .found = gallery_review_found, .findings = 1, .reviewed = true };
	const std::vector<summary_page> reviewed_pages = lifecycle_pages(route, gallery_plan_summary, sample.size(), edits.size(), 1, false, false, false);
	const auto review_page = static_cast<std::uint32_t>(std::distance(reviewed_pages.begin(), std::ranges::find(reviewed_pages, page_kind::review_loop, &summary_page::kind)));
	plan_cell("review \xC2\xB7 no findings", approved, reviewed_pages, review_page, {}, {}, std::nullopt, clean);
	plan_cell("review \xC2\xB7 findings, revised, then clean", approved, reviewed_pages, review_page, {}, {}, std::nullopt, revised);
	plan_cell("review \xC2\xB7 cap reached, findings open", approved, reviewed_pages, review_page, {}, {}, std::nullopt, capped);

	const std::vector<summary_page> reported_pages = lifecycle_pages(route, gallery_plan_summary, sample.size(), edits.size(), revised.cycles.size(), true, false, false);
	const auto report_page = static_cast<std::uint32_t>(std::distance(reported_pages.begin(), std::ranges::find(reported_pages, page_kind::report, &summary_page::kind)));
	const std::vector<summary_page> bare_pages = lifecycle_pages(route, gallery_plan_summary, 0, 0, 0, true, false, false);
	const auto bare_page = static_cast<std::uint32_t>(std::distance(bare_pages.begin(), std::ranges::find(bare_pages, page_kind::report, &summary_page::kind)));
	plan_cell("summarize \xC2\xB7 report, steps and review", approved, reported_pages, report_page, progress_of(sample.size(), finished, edits, false), edits, std::nullopt, revised, edits.size() + gallery_revise_edits);
	plan_cell("summarize \xC2\xB7 report, no steps, no review", approved, bare_pages, bare_page, {}, {}, std::nullopt, {}, 0);

	const std::vector<summary_page> fixing_pages = lifecycle_pages(route, {}, 0, 0, 0, false, true, false);
	const auto errors_page = static_cast<std::uint32_t>(std::distance(fixing_pages.begin(), std::ranges::find(fixing_pages, page_kind::errors, &summary_page::kind)));
	const chrome_plan fixing = plan_from(gallery_facts(chat_kind::build_error, task_phase::apply, agent_state::working));
	const build_fix diagnosed = gallery_fix();
	const build_fix undiagnosed{ .errors = diagnosed.errors };
	const build_fix tailed{
		.errors = { { .message = "the build failed without a parseable diagnostic", .output = { "ninja: build stopped: subcommand failed.", "FAILED: Editor/CMakeFiles/gse.ide.dir/Agent/Panel.cpp.obj" } } },
		.denied = diagnosed.denied,
	};
	plan_cell("build error \xC2\xB7 errors, no diagnosis yet", fixing, fixing_pages, errors_page, {}, {}, std::nullopt, {}, 0, &undiagnosed);
	plan_cell("build error \xC2\xB7 errors and diagnosis", fixing, fixing_pages, errors_page, {}, {}, std::nullopt, {}, 0, &diagnosed);
	plan_cell("build error \xC2\xB7 no file, output tail, denied chats", fixing, fixing_pages, errors_page, {}, {}, std::nullopt, {}, 0, &tailed);

	const std::vector<phase_step> steering_route = phase_timeline(task_phase::steer, true, {});
	const std::vector<summary_page> goal_pages = { { .phase = task_phase::scope, .kind = page_kind::goal } };
	const std::uint32_t goal_page = 0;
	const chrome_plan steered = plan_from(gallery_facts(chat_kind::goal, task_phase::steer, agent_state::working));
	const std::vector<criterion_report> criteria = gallery_criteria();
	const goal_dashboard reported{
		.record = gallery_record,
		.aim = std::string(gallery_aim),
		.steers_left = gallery_steers_left,
		.criteria = criteria,
		.route = steering_route,
		.ledger = {
			{ .started = gallery_steered_at, .verdict = goal_verdict::task, .summary = gallery_ledger_task },
			{ .started = gallery_steered_at, .verdict = goal_verdict::escalate, .summary = gallery_ledger_escalate },
		},
	};
	const auto goal_cell = [&](const std::string& name, const auto& adjust) {
		goal_dashboard goal = reported;
		adjust(goal);
		plan_cell(name, steered, goal_pages, goal_page, {}, {}, std::nullopt, {}, 0, nullptr, &goal, "goal");
	};
	goal_cell("goal \xC2\xB7 steering, criteria reported", [](goal_dashboard&) {});
	goal_cell("goal \xC2\xB7 criteria never reported", [](goal_dashboard& g) {
		g.criteria = {};
		g.ledger = {};
	});
	goal_cell("goal \xC2\xB7 waiting on an await", [](goal_dashboard& g) {
		g.standing = goal_standing::waiting;
		g.waiting = std::format("'{}' to change", gallery_file);
	});
	goal_cell("goal \xC2\xB7 escalated or paused, held for you", [](goal_dashboard& g) { g.standing = goal_standing::held; });
	goal_cell("goal \xC2\xB7 steer budget spent", [](goal_dashboard& g) {
		g.standing = goal_standing::exhausted;
		g.steers_left = 0;
	});
	goal_cell("goal \xC2\xB7 met", [](goal_dashboard& g) {
		g.standing = goal_standing::met;
		g.ledger.insert(g.ledger.begin(), { .started = gallery_steered_at, .verdict = goal_verdict::met, .summary = "Every criterion met." });
	});
	goal_cell("goal \xC2\xB7 met before met goals were kept, record recovered", [](goal_dashboard& g) {
		g.standing = goal_standing::met;
		g.steers_left = 0;
		g.criteria = {};
	});
	goal_cell("goal \xC2\xB7 met before met goals were kept, no record found", [](goal_dashboard& g) {
		g.standing = goal_standing::met;
		g.steers_left = 0;
		g.criteria = {};
		g.record.clear();
		g.aim.clear();
	});

	const std::vector<summary_page> cycle_pages = lifecycle_pages(phase_timeline(task_phase::settled, true, {}), gallery_plan_summary, sample.size(), edits.size(), revised.cycles.size(), true, false, true);
	const auto verdict_page = static_cast<std::uint32_t>(std::distance(cycle_pages.begin(), std::ranges::find(cycle_pages, page_kind::verdict, &summary_page::kind)));
	plan_cell("cycle \xC2\xB7 past cycle, steering verdict", approved, cycle_pages, verdict_page, {}, {}, std::nullopt, revised, 0, nullptr, nullptr, "cycle 2 of 4", gallery_ledger_task);
	plan_cell("cycle \xC2\xB7 past cycle, its scope", approved, cycle_pages, 0, {}, {}, std::nullopt, revised, 0, nullptr, nullptr, "cycle 2 of 4");
	plan_cell("cycle \xC2\xB7 old chat, no verdict recorded", approved, pages, 0, {}, {}, std::nullopt, {}, 0, nullptr, nullptr, "cycle 1 of 3");

	const since_digest unseen{ .since = gallery_steered_at, .stages = 7, .verdicts = 2, .questions = 1, .waiting = true };
	for (const auto& [name, digest] : { std::pair<std::string_view, std::string>{ "since you were here \xC2\xB7 banner", digest_line(unseen) }, { "since you were here \xC2\xB7 caught up, no banner", std::string{} } }) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = name,
			.position = { origin[0] + pad, top - row_h * 0.5f + text_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = sty.color_text_secondary,
			.clip_rect = frame.clip,
		});
		top -= row_h;
		if (digest.empty()) {
			continue;
		}
		const rectf banner = rectf::from_position_size({ origin[0], top }, { width, row_h + pad });
		const float catch_w = text_view->width(catch_up_label, fs) + pad * 2.f;
		const rectf button = rectf::from_position_size({ banner.right() - pad - catch_w, banner.top() - pad * 0.5f }, { catch_w, fs * 1.4f });
		draw_since_banner(ctx, ctx.intern(digest), banner.intersection(frame.clip), button);
		gui::draw::fill_surface(ctx, button, sty.list_item.surfaces.hot, { .clip_rect = frame.clip });
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = catch_up_label,
			.position = { button.left() + pad, button.center().y() + text_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = sty.color_text,
			.clip_rect = frame.clip,
		});
		top -= banner.height() + pad;
	}

	for (const auto& [name, said] : { std::pair<std::string_view, std::string_view>{ "fork \xC2\xB7 asking", "asking about cycle 2 of 4 \xC2\xB7 review \xC2\xB7 read-only fork" }, { "fork \xC2\xB7 answered", "answer about cycle 2 of 4 \xC2\xB7 review \xC2\xB7 read-only fork" } }) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = name,
			.position = { origin[0] + pad, top - row_h * 0.5f + text_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = sty.color_text_secondary,
			.clip_rect = frame.clip,
		});
		top -= row_h;
		const rectf strip = rectf::from_position_size({ origin[0], top }, { width, row_h * 2.f + pad });
		ctx.queue_sprite({
			.rect = strip,
			.color = sty.panel_alt.fill,
			.texture = ctx.blank_texture,
			.clip_rect = frame.clip,
		});
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = said,
			.position = { strip.left() + pad, strip.top() - row_h * 0.5f + text_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = sty.color_text_secondary,
			.clip_rect = frame.clip,
		});
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = name.ends_with("asking") ? std::string_view("The fork is reading the chat...") : gallery_ledger_task,
			.position = { strip.left() + pad, strip.top() - row_h * 1.5f + text_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = sty.*annotation_from_enum<row_style>(row_kind::text, {}).color,
			.clip_rect = strip.intersection(frame.clip),
		});
		top -= strip.height() + pad;
	}

	const float line_h = gui::text_area_line_height(ctx, ctx.fonts.text);
	session answering;
	answering.rows = answer_samples();
	for (std::uint32_t index = 0; index < answering.rows.size(); ++index) {
		const transcript_row& sample = answering.rows[index];
		const auto picked = std::ranges::count(sample.options, true, &question_option::chosen);
		const std::string name = std::format("answered \xC2\xB7 {}{}", picked > 1 ? "multiple picks" : "single pick", sample.detail.empty() ? "" : ", with steering");
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = ctx.intern(name),
			.position = { origin[0] + pad, top - row_h * 0.5f + text_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = sty.color_text_secondary,
			.clip_rect = frame.clip,
		});
		top -= row_h;

		transcript_view card;
		card.filter = row_filter::conversation;
		const float wrap = std::max(0.f, width - pad * 2.f - gui::scroll_config{}.scrollbar_width);
		const auto code_view = ctx.fonts.code.resolve();
		push_row(answering, card, sty, answering.rows[index], index, {
			.fonts = ctx.fonts,
			.face = *code_view,
			.body = *text_view,
			.width = wrap,
			.scale = fs,
		});

		const float extent = static_cast<float>(card.buffer.lines.size()) * line_h + pad;
		const rectf area = rectf::from_position_size({ origin[0], top }, { width, extent });
		id hot;
		id active;
		id focus;
		draw_plan_summary(ctx, card, area.intersection(frame.clip), 1.f, generate_temp_id(hash_of(frame.canvas.number(), "answer", index)), hot, active, focus);
		top -= extent + pad;
	}
}

auto gse::ide::agent::gallery_fix() -> build_fix {
	return {
		.errors = {
			{ .file = "Editor/Editor/Source/Agent/Panel.cpp", .line = 1245, .message = "'plan' was not declared in this scope" },
			{ .file = "Engine/Engine/Source/Graphics/2D/Gui/Types.cpp", .line = 417, .message = "no matching function for call to 'ease'", .notes = { "note: candidate expects 2 arguments, 1 provided" } },
		},
		.denied = { "agent panel", "chrome tweaks" },
		.diagnosis = "**Cause:** `Panel.cpp` still calls `status_label` after the move to `chrome_plan`, and the `ease` overload in `Types.cpp` gained a curve argument.\n\n**Fix:** route the call through the plan, and pass `ease_kind::cubic_out` at the one stale call site.",
	};
}

auto gse::ide::agent::gallery_criteria() -> std::vector<criterion_report> {
	return {
		{ .id = "7", .state = criterion_state::met, .note = "a canvas draws inline" },
		{ .id = "11", .state = criterion_state::owner, .note = "fullscreen open and close" },
		{ .id = "14", .state = criterion_state::met, .note = "the draw_* grep is 0" },
		{ .id = "18", .state = criterion_state::open, .note = "every screen accepted" },
	};
}

auto gse::ide::agent::answer_samples() -> std::vector<transcript_row> {
	return {
		{
			.kind = row_kind::answer,
			.text = "Which layout should the scope gate use?",
			.options = { { .label = "Split 2/3 | 1/3, always" }, { .label = "Full width, chat slides in on follow-up", .chosen = true }, { .label = "Tabs" } },
		},
		{
			.kind = row_kind::answer,
			.text = "Which facts belong in the status row?",
			.options = { { .label = "Phase", .chosen = true }, { .label = "Elapsed time", .chosen = true }, { .label = "Cost" }, { .label = "Build hold", .chosen = true } },
		},
		{
			.kind = row_kind::answer,
			.text = "How should an answered question appear?",
			.detail = "Keep it compact, and make the picked option obvious at a glance.",
			.options = { { .label = "A card with the picks highlighted", .chosen = true }, { .label = "The full prompt, as today" } },
		},
	};
}

auto gse::ide::agent::step_edit_sample() -> std::vector<step_edit> {
	return {
		{ .file = "Tools/gse-mcp/server.mjs", .removed = { "const draw = async (args) => {" }, .added = { "const step = async (args) => {", "const draw = async (args) => {" }, .step = 0 },
		{ .file = "Editor/Editor/Source/Agent/Chrome.cpp", .removed = { "\tstd::vector<summary_page> pages;" }, .added = { "\tstd::vector<summary_page> pages = apply_pages(steps, edits);" }, .step = 1 },
		{ .file = "Editor/Editor/Source/Agent/Model.cppm", .added = { "\t\tstd::vector<step_state> progress;" } },
		{ .file = "Editor/Editor/Source/Agent/Panel.cppm", .removed = { "\tconstexpr float plan_steps_share = 0.58f;" }, .added = { "\tconstexpr float plan_steps_share = 0.6f;" } },
	};
}

auto gse::ide::agent::plan_sample() -> std::vector<build_inbox::plan_step> {
	return {
		{ .text = "Carry structured steps from gse_phase_done through the inbox to the gate", .files = { "Tools/gse-mcp/server.mjs", "Editor/Editor/Source/BuildRunner/Inbox.cpp", "Editor/Editor/Source/Agent/Phase.cpp" } },
		{ .text = "Decide when the plan block shows in plan_chrome", .files = { "Editor/Editor/Source/Agent/Chrome.cpp" } },
		{ .text = "Lay out and draw the two-column plan with Approve and Revise", .files = { "Editor/Editor/Source/Agent/Panel.cpp", "Editor/Editor/Source/Agent/Panel.cppm" } },
		{ .text = "Report the steps to gse_panel_query", .files = { "Editor/Editor/Source/PanelQuery/PanelQuery.cpp" } },
	};
}

auto gse::ide::agent::question_samples() -> std::vector<std::pair<pending_question, float>> {
	pending_question brief{
		.question = "Which reading of the task should apply follow?",
		.context = "the gallery's short question",
		.options = {
			{ .label = "Keep the status row as it is (recommended)", .chosen = true },
			{ .label = "Move the findings count into the tab" },
			{ .label = "Drop the pending note" },
		},
	};

	pending_question crowded{
		.question = "This question is long enough to wrap across several lines, so the block has to choose between showing all of the question and showing all of the options, and when the room runs out it says how many options it could not fit.",
		.options = {},
		.multi = true,
	};
	for (std::size_t at = 0; at < gallery_crowded_options; ++at) {
		crowded.options.push_back({ .label = std::format("Option {} carries a label long enough to wrap onto a second line inside the block", at + 1) });
	}

	return { { std::move(brief), gallery_open_room }, { std::move(crowded), gallery_tight_room } };
}
