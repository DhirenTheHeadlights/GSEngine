module gse.ide.agent:blame_impl;

import std;
import gse;

import gse.ide.build;
import gse.ide.config;
import gse.ide.net;

import :blame;
import :model;
import :session;

auto gse::ide::agent::note_source_change(session& s, const std::filesystem::path& file, const std::int64_t mtime) -> id {
	id touched_id;
	for (const id key : build_runner::build_keys_for(file)) {
		const auto [stamp, unseen] = s.unbuilt.try_emplace(key, mtime);
		if (!unseen) {
			stamp->second = std::max(stamp->second, mtime);
		}

		touched_id = config::path_id(file);
		const auto [touch, first] = s.touched.try_emplace(touched_id, touched_source{ .build_key = key, .mtime = mtime });
		if (!first) {
			touch->second.build_key = key;
			touch->second.mtime = std::max(touch->second.mtime, mtime);
		}
	}
	return touched_id;
}

auto gse::ide::agent::note_written_file(session& s, const std::filesystem::path& file) -> void {
	const auto requested = static_cast<std::int64_t>(std::filesystem::file_time_type::clock::now().time_since_epoch().count());
	const auto touch = s.touched.find(note_source_change(s, file, requested));
	if (touch != s.touched.end()) {
		touch->second.wrote = true;
	}
	s.wrote_this_turn = true;
}

auto gse::ide::agent::working_on_files(const session& s) -> bool {
	if (!s.running) {
		return false;
	}
	if (s.think_clock) {
		return true;
	}
	const time settled = seconds(10.f);
	return s.recent_turn && s.recent_turn->elapsed() < settled;
}

auto gse::ide::agent::is_busy(const session& s) -> bool {
	return s.running && s.think_clock.has_value();
}

auto gse::ide::agent::mid_write(const session& s) -> bool {
	return is_busy(s) && s.wrote_this_turn;
}

auto gse::ide::agent::build_wait_for(const data& d, const session& s) -> build_wait {
	if (s.info.agent_id.empty()) {
		return build_wait::none;
	}

	const auto claimed_by_session = [&s](const queued_build& queued) {
		return queued.agent == s.info.agent_id;
	};
	if (std::ranges::any_of(d.inbox_active, claimed_by_session)) {
		return build_wait::building;
	}
	if (queued_for(d, s)) {
		return build_wait::queued;
	}
	return build_wait::none;
}

auto gse::ide::agent::queued_for(const data& d, const session& s) -> const queued_build* {
	if (s.info.agent_id.empty()) {
		return nullptr;
	}

	const auto found = std::ranges::find(d.inbox_queue, s.info.agent_id, &queued_build::agent);
	return found == d.inbox_queue.end() ? nullptr : &*found;
}

auto gse::ide::agent::state_of(const data& d, const session& s) -> agent_state {
	if (s.hibernating) {
		return agent_state::hibernating;
	}
	if (s.retry.waiting && s.limited_until > unix_now()) {
		return agent_state::rate_limited;
	}

	switch (build_wait_for(d, s)) {
		case build_wait::building:
			return agent_state::compiling;
		case build_wait::queued:
			return agent_state::build_queued;
		case build_wait::none:
			break;
	}

	if (!s.running) {
		return agent_state::exited;
	}
	if (!s.think_clock) {
		return agent_state::idle;
	}
	return s.wrote_this_turn ? agent_state::editing : agent_state::working;
}

auto gse::ide::agent::style_of(const agent_state state) -> state_style {
	return annotation_from_enum<state_style>(state, {});
}

auto gse::ide::agent::status_label(const data& d, const session& s) -> std::string {
	const agent_state state = state_of(d, s);
	std::string out = style_of(state).label;
	if (state == agent_state::rate_limited) {
		out += std::format(" - resuming in {}m", std::max<std::int64_t>((s.limited_until - unix_now()) / 60, 1));
	}
	if (state == agent_state::hibernating && !s.unbuilt.empty()) {
		out += " until " + unbuilt_label(s) + " is built";
	}
	if (state == agent_state::build_queued) {
		const queued_build* queued = queued_for(d, s);
		if (queued && queued->reported == build_hold::tree_busy) {
			out += std::format(" behind '{}'", blocker_name(d, queued->blocker));
		}
	}
	return out;
}

auto gse::ide::agent::exe_label(const session& s) -> std::string {
	if (s.touched.empty() && s.unbuilt.empty()) {
		return "no edits";
	}
	return s.unbuilt.empty() ? "in the current exe" : "NOT in the exe: " + unbuilt_label(s);
}

auto gse::ide::agent::attribute_change(data& d, const build_runner::source_changed& change) -> void {
	if (build_runner::is_build_touch(change.path, change.mtime)) {
		return;
	}

	const std::string& owner = config::worktree_for(change.path).name;
	for (session& s : d.sessions) {
		if (working_on_files(s) && config::worktree_for(s.cwd).name == owner) {
			note_source_change(s, change.path, change.mtime);
		}
	}
}

auto gse::ide::agent::refresh_stale(data& d) -> void {
	for (session& s : d.sessions) {
		for (auto entry = s.unbuilt.begin(); entry != s.unbuilt.end();) {
			const auto built = d.built.find(entry->first);
			entry = built != d.built.end() && entry->second <= built->second
				? s.unbuilt.erase(entry)
				: std::next(entry);
		}
		for (auto entry = s.touched.begin(); entry != s.touched.end();) {
			const auto built = d.built.find(entry->second.build_key);
			entry = built != d.built.end() && entry->second.mtime <= built->second
				? s.touched.erase(entry)
				: std::next(entry);
		}
		s.stale = !s.unbuilt.empty();
	}
}

auto gse::ide::agent::unbuilt_label(const session& s) -> std::string {
	if (s.unbuilt.empty()) {
		return "up to date";
	}

	std::vector<std::string> pending;
	for (const config::worktree& tree : config::worktrees()) {
		if (s.unbuilt.contains(build_runner::build_key(tree, tree.game_target))) {
			pending.push_back(&tree == &config::primary()
				? tree.game_target
				: std::format("{} ({})", tree.game_target, tree.name));
		}
	}
	if (s.unbuilt.contains(build_runner::build_key(config::primary(), config::editor_target))) {
		pending.emplace_back(config::editor_target);
	}
	if (pending.empty()) {
		return "unbuilt";
	}

	std::string out = pending.front();
	for (const std::string& target : pending | std::views::drop(1)) {
		out += ", " + target;
	}
	return out;
}

auto gse::ide::agent::touch_owner(data& d, const std::span<const std::filesystem::path> files) -> session* {
	session* owner = nullptr;
	touched_source best;
	for (const std::filesystem::path& file : files) {
		const id key = config::path_id(file);
		for (session& s : d.sessions) {
			const auto touch = s.touched.find(key);
			if (touch == s.touched.end()) {
				continue;
			}
			const touched_source& candidate = touch->second;
			if (!owner || std::pair(candidate.wrote, candidate.mtime) > std::pair(best.wrote, best.mtime)) {
				best = candidate;
				owner = &s;
			}
		}
	}
	return owner;
}

auto gse::ide::agent::blame_owner(data& d, const build_runner::build_error& error) -> session* {
	if (session* direct = touch_owner(d, std::span(&error.file, 1))) {
		return direct;
	}
	return touch_owner(d, error.related);
}

auto gse::ide::agent::attribute_build_errors(data& d, const build_runner::build_finished& finished, const channel_write<blame_offer> offers) -> void {
	constexpr std::size_t blame_limit = 8;

	for (session& s : d.sessions) {
		if (s.blame_build == finished.key) {
			s.blame.clear();
			s.blame_build = {};
		}
	}
	if (d.unclaimed_build == finished.key) {
		d.unclaimed.clear();
		d.unclaimed_build = {};
	}

	std::vector<blame_offer> grouped;

	for (const build_runner::build_error& error : finished.errors) {
		session* owner = blame_owner(d, error);
		std::vector<blamed_error>& claim = owner ? owner->blame : d.unclaimed;
		if (claim.size() >= blame_limit) {
			continue;
		}

		(owner ? owner->blame_build : d.unclaimed_build) = finished.key;
		claim.push_back({
			.file = error.file,
			.line = error.line,
			.message = error.message,
			.notes = error.notes,
		});

		if (owner) {
			append_row(*owner, {
				.kind = row_kind::failure,
				.text = std::format("broke the build in {}", error.file.filename().generic_display_string()),
				.detail = std::format("line {}: {}", error.line, error.message),
				.file = error.file,
				.start_line = std::max(error.line, 1u),
			});
		}

		const std::uint32_t claimant = owner ? owner->id : 0;
		const auto opened = std::ranges::find(grouped, claimant, &blame_offer::session);
		if (opened != grouped.end()) {
			++opened->extra;
			continue;
		}

		grouped.push_back({
			.session = claimant,
			.session_name = owner ? owner->name : std::string{},
			.file = error.file,
			.line = error.line,
			.kind = finished.kind,
		});
	}

	for (const blame_offer& offer : grouped) {
		offers.push<blame_offer>(offer);
	}
}

auto gse::ide::agent::build_patience() -> time {
	return seconds(120.f);
}

auto gse::ide::agent::blocker_for(const data& d, const queued_build& queued) -> const session* {
	const config::worktree& tree = queued.tree ? *queued.tree : config::primary();

	for (const session& s : d.sessions) {
		if (!queued.agent.empty() && queued.agent == s.info.agent_id) {
			continue;
		}
		if (!mid_write(s)) {
			continue;
		}
		if (config::worktree_for(s.cwd).name != tree.name) {
			continue;
		}
		return &s;
	}
	return nullptr;
}

auto gse::ide::agent::hold_for(const data& d, const queued_build& queued, const bool building) -> build_hold_state {
	const auto rebuilding_editor = [](const queued_build& active) {
		return active.target == build_runner::build_target::editor;
	};
	if (std::ranges::any_of(d.inbox_active, rebuilding_editor)) {
		return { .reason = build_hold::editor_restart };
	}
	if (building || !d.inbox_active.empty()) {
		return { .reason = build_hold::building };
	}
	if (queued.forced || system_clock::now<time>() >= queued.requested + build_patience()) {
		return {};
	}
	if (const session* blocker = blocker_for(d, queued)) {
		return {
			.reason = build_hold::tree_busy,
			.blocker = blocker->id,
		};
	}
	return {};
}

auto gse::ide::agent::blocker_name(const data& d, const std::uint32_t blocker) -> std::string_view {
	const auto found = std::ranges::find(d.sessions, blocker, &session::id);
	return found == d.sessions.end() || found->name.empty() ? std::string_view("unnamed") : std::string_view(found->name);
}

auto gse::ide::agent::hold_message(const build_hold_state& hold, const std::string_view blocker) -> std::string {
	const hold_style style = annotation_from_enum<hold_style>(hold.reason, {});

	switch (hold.reason) {
		case build_hold::tree_busy:
			return std::format(
				"{} - chat '{}' has not finished its turn, so the tree is mid-change. Your build runs when it does. You are not blocked: hibernate with Tools/gse-hibernate --then \"<what to do next>\" and you will be woken with the result, or do unrelated work and re-run.",
				style.label,
				blocker.empty() ? std::string_view("unnamed") : blocker
			);
		case build_hold::editor_restart:
			return std::format("{} - your request is preserved across the restart and resumes automatically", style.label);
		case build_hold::building:
			return std::format("{} - you are queued behind it", style.label);
		case build_hold::none:
			break;
	}
	return {};
}

auto gse::ide::agent::hold_label(const data& d, const queued_build& queued) -> std::string {
	if (queued.reported == build_hold::none) {
		return "starting";
	}

	const hold_style style = annotation_from_enum<hold_style>(queued.reported, {});
	if (queued.reported != build_hold::tree_busy) {
		return style.label;
	}

	const time remaining = queued.requested + build_patience() - system_clock::now<time>();
	return std::format("waiting on '{}' \xC2\xB7 starts anyway in {:.0f:s}", blocker_name(d, queued.blocker), std::max(remaining, time{}));
}

auto gse::ide::agent::queue_label(const queued_build& queued) -> std::string {
	std::string out = queued.target == build_runner::build_target::editor ? "editor" : "game";
	if (queued.tree) {
		out += " \xC2\xB7 " + queued.tree->name;
	}
	if (queued.run) {
		out += " \xC2\xB7 run";
	}
	return out;
}

auto gse::ide::agent::accept_requests(data& d) -> void {
	const time now = system_clock::now<time>();

	for (build_inbox::request& incoming : build_inbox::take_requests()) {
		queued_build queued{
			.id = std::move(incoming.id),
			.agent = std::move(incoming.agent),
			.requested = now,
		};

		if (incoming.target == "editor") {
			queued.target = build_runner::build_target::editor;
		}
		else if (incoming.target != "game") {
			build_inbox::publish({
				.id = queued.id,
				.outcome = build_inbox::status::rejected,
				.lines = { std::format("unknown build target '{}'; expected 'game' or 'editor'", incoming.target) },
			});
			continue;
		}
		queued.run = incoming.run;

		if (!incoming.tree.empty()) {
			for (const config::worktree& candidate : config::worktrees()) {
				if (candidate.name == incoming.tree) {
					queued.tree = &candidate;
					break;
				}
			}
			if (!queued.tree) {
				build_inbox::publish({
					.id = queued.id,
					.outcome = build_inbox::status::rejected,
					.lines = { std::format("unknown worktree '{}'", incoming.tree) },
				});
				continue;
			}
		}

		const auto same_slot = [&queued](const queued_build& existing) {
			return !existing.agent.empty() && existing.agent == queued.agent
				&& existing.target == queued.target
				&& existing.run == queued.run
				&& existing.tree == queued.tree;
		};
		if (const auto held = std::ranges::find_if(d.inbox_queue, same_slot); held != d.inbox_queue.end()) {
			log::println(log::level::info, log::category::task, "build inbox: agent '{}' re-attached to its queued {} build ({} -> {})", queued.agent, incoming.target, held->id, queued.id);
			held->id = std::move(queued.id);
			held->reported = build_hold::none;
			held->blocker = 0;
			continue;
		}

		log::println(log::level::info, log::category::task, "build inbox: agent '{}' queued a {} build ({})", queued.agent, incoming.target, queued.id);
		d.inbox_queue.push_back(std::move(queued));
	}
}

auto gse::ide::agent::report_hold(queued_build& queued, const build_hold_state& hold, const std::string_view blocker) -> void {
	if (queued.reported == hold.reason && queued.blocker == hold.blocker) {
		return;
	}
	queued.reported = hold.reason;
	queued.blocker = hold.blocker;

	if (hold.reason == build_hold::none) {
		build_inbox::withdraw(queued.id);
		return;
	}
	build_inbox::publish({
		.id = queued.id,
		.outcome = build_inbox::status::waiting,
		.lines = { hold_message(hold, blocker) },
	});
}

auto gse::ide::agent::accept_hibernations(data& d) -> void {
	for (build_inbox::hibernate_request& incoming : build_inbox::take_hibernations()) {
		const auto found = std::ranges::find_if(d.sessions, [&incoming](const session& s) {
			return s.info.agent_id == incoming.agent;
		});

		if (found == d.sessions.end()) {
			build_inbox::publish({
				.id = incoming.id,
				.outcome = build_inbox::status::rejected,
				.lines = { "this session is not an editor chat, so the editor cannot wake it - only chats started in the agent panel can hibernate" },
			});
			continue;
		}

		found->hibernating = true;
		found->wake_prompt = std::move(incoming.prompt);

		const std::string awaited = found->unbuilt.empty() ? "the next build" : unbuilt_label(*found);

		log::println(log::level::info, log::category::task, "agent: chat '{}' is hibernating until {} lands", found->name, awaited);
		append_row(*found, {
			.kind = row_kind::note,
			.text = std::format("hibernating until built: {}", awaited),
		});

		build_inbox::publish({
			.id = incoming.id,
			.outcome = build_inbox::status::ok,
			.lines = { std::format("hibernating until {} covers your edits - end your turn now; the editor will prompt you when it lands", awaited) },
		});
	}
}

auto gse::ide::agent::hibernating_count(const data& d) -> std::size_t {
	return static_cast<std::size_t>(std::ranges::count_if(d.sessions, [](const session& s) {
		return s.hibernating;
	}));
}

auto gse::ide::agent::wake_observers(data& d, const build_runner::build_finished& finished) -> void {
	for (session& s : d.sessions) {
		if (!s.hibernating || !(s.unbuilt.empty() || s.unbuilt.contains(finished.key))) {
			continue;
		}

		s.hibernating = false;
		const std::string prompt = std::move(s.wake_prompt);
		s.wake_prompt.clear();

		std::string lead = finished.succeeded
			? "The build you were waiting on succeeded and your edits are in the current binaries."
			: "The build you were waiting on failed.";
		if (!finished.succeeded && !s.blame.empty()) {
			lead += " These errors are in files you edited:\n" + blame_prompt({}, s.blame);
			s.blame.clear();
			s.blame_build = {};
		}
		else if (!finished.succeeded) {
			lead += " None of the errors are in files you edited, so do not try to fix them - report that and stop.";
		}

		if (!s.running && !launch_session(s)) {
			append_row(s, {
				.kind = row_kind::failure,
				.text = "woke for a build but 'claude' could not be relaunched",
			});
			continue;
		}

		log::println(log::level::info, log::category::task, "agent: waking chat '{}' - its edits are in the build", s.name);
		send_to_session(s, prompt.empty() ? lead : lead + "\n\n" + prompt, {});
	}
}

auto gse::ide::agent::request_of(const queued_build& queued) -> build_inbox::request {
	return {
		.id = queued.id,
		.agent = queued.agent,
		.target = queued.target == build_runner::build_target::editor ? "editor" : "game",
		.tree = queued.tree ? queued.tree->name : std::string{},
		.run = queued.run,
	};
}

auto gse::ide::agent::cancel_queued(data& d, const std::string_view id) -> void {
	const auto found = std::ranges::find(d.inbox_queue, id, &queued_build::id);
	if (found == d.inbox_queue.end()) {
		return;
	}

	log::println(log::level::info, log::category::task, "build inbox: cancelled queued build {}", found->id);
	build_inbox::publish({
		.id = found->id,
		.outcome = build_inbox::status::aborted,
		.lines = { "cancelled from the agents panel" },
	});
	d.inbox_queue.erase(found);
}

auto gse::ide::agent::force_queued(data& d, const std::string_view id) -> void {
	const auto found = std::ranges::find(d.inbox_queue, id, &queued_build::id);
	if (found == d.inbox_queue.end()) {
		return;
	}

	log::println(log::level::info, log::category::task, "build inbox: {} was released to build now", found->id);
	found->forced = true;
}

auto gse::ide::agent::hand_off_builds(data& d, const bool relaunching) -> void {
	for (const queued_build& waiter : d.inbox_active) {
		if (relaunching && waiter.target != build_runner::build_target::editor) {
			build_inbox::restore(request_of(waiter));
			continue;
		}
		build_inbox::publish({
			.id = waiter.id,
			.outcome = relaunching ? build_inbox::status::ok : build_inbox::status::aborted,
			.lines = { relaunching
				? "the editor is relaunching into the new image"
				: "the editor shut down before the build finished" },
		});
	}

	for (const queued_build& waiter : d.inbox_queue) {
		if (relaunching) {
			build_inbox::restore(request_of(waiter));
			continue;
		}
		build_inbox::publish({
			.id = waiter.id,
			.outcome = build_inbox::status::aborted,
			.lines = { "the editor shut down before this queued build started" },
		});
	}

	if (relaunching) {
		log::println(log::level::info, log::category::task, "build inbox: handing {} queued request(s) to the relaunched editor", d.inbox_queue.size());
	}

	d.inbox_active.clear();
	d.inbox_queue.clear();
}

auto gse::ide::agent::poll_build_inbox(data& d, const channel_write<build_runner::build_request> builds, const bool building) -> void {
	const time now = system_clock::now<time>();

	if (!d.inbox_active.empty() && !building && now >= d.inbox_dispatch_deadline) {
		log::println(log::level::info, log::category::task, "build inbox: the editor took a different build, so {} request(s) go back in the queue", d.inbox_active.size());
		d.inbox_queue.insert(d.inbox_queue.begin(), std::make_move_iterator(d.inbox_active.begin()), std::make_move_iterator(d.inbox_active.end()));
		d.inbox_active.clear();
	}

	if (now < d.next_inbox_poll) {
		return;
	}
	d.next_inbox_poll = now + seconds(0.25f);

	accept_requests(d);
	if (d.inbox_queue.empty()) {
		return;
	}

	for (queued_build& queued : d.inbox_queue) {
		const build_hold_state hold = hold_for(d, queued, building);
		report_hold(queued, hold, blocker_name(d, hold.blocker));
	}

	if (!d.inbox_active.empty()) {
		return;
	}

	const auto ready = [](const queued_build& queued) {
		return queued.reported == build_hold::none;
	};
	if (std::ranges::none_of(d.inbox_queue, ready)) {
		return;
	}

	const auto is_editor = [](const queued_build& queued) {
		return queued.target == build_runner::build_target::editor;
	};
	const auto starving_editor = std::ranges::find_if(d.inbox_queue, [&is_editor, &ready, now](const queued_build& queued) {
		return is_editor(queued) && ready(queued) && (queued.forced || now >= queued.requested + build_patience());
	});
	const auto project_first = std::ranges::find_if(d.inbox_queue, [&is_editor, &ready](const queued_build& queued) {
		return !is_editor(queued) && ready(queued);
	});

	const queued_build& head = starving_editor != d.inbox_queue.end() ? *starving_editor
		: project_first != d.inbox_queue.end() ? *project_first
		: *std::ranges::find_if(d.inbox_queue, ready);

	const build_runner::build_target target = head.target;
	const bool run = head.run;
	const config::worktree* tree = head.tree;

	std::vector<queued_build> group;
	std::vector<queued_build> deferred;
	for (queued_build& queued : d.inbox_queue) {
		if (queued.target == target && queued.run == run && queued.tree == tree) {
			group.push_back(std::move(queued));
		}
		else {
			deferred.push_back(std::move(queued));
		}
	}

	log::println(log::level::info, log::category::task, "build inbox: one {} build is answering {} queued request(s), {} deferred", target == build_runner::build_target::editor ? "editor" : "project", group.size(), deferred.size());

	builds.push<build_runner::build_request>({
		.target = target,
		.run_after = run,
		.tree = tree,
		.inbox_id = group.front().id,
	});

	d.inbox_active = std::move(group);
	d.inbox_dispatch_deadline = now + seconds(5.f);
	d.inbox_queue = std::move(deferred);
}

auto gse::ide::agent::publish_inbox_result(data& d, const build_runner::build_finished& finished) -> void {
	if (d.inbox_active.empty()) {
		return;
	}

	std::vector<const session*> owners;
	owners.reserve(finished.errors.size());
	for (const build_runner::build_error& error : finished.errors) {
		owners.push_back(blame_owner(d, error));
	}

	for (const queued_build& waiter : d.inbox_active) {
		build_inbox::result outcome{
			.id = waiter.id,
			.outcome = finished.succeeded ? build_inbox::status::ok : build_inbox::status::failed,
		};
		if (finished.succeeded) {
			build_inbox::publish(outcome);
			continue;
		}

		std::vector<std::string> mine;
		std::vector<std::string> theirs;
		std::uint32_t owned = 0;
		for (std::size_t index = 0; index < finished.errors.size(); ++index) {
			const build_runner::build_error& error = finished.errors[index];
			const session* owner = owners[index];
			const bool is_mine = !owner || (!waiter.agent.empty() && owner->info.agent_id == waiter.agent);
			std::vector<std::string>& into = is_mine ? mine : theirs;
			owned += is_mine ? 1 : 0;

			into.push_back(std::format("{}:{}: {}", error.file.generic_display_string(), error.line, error.message));
			for (const std::string& note : error.notes) {
				into.push_back("  " + note);
			}
			for (const std::filesystem::path& related : error.related) {
				const session* touching = touch_owner(d, std::span(&related, 1));
				into.push_back(touching && touching->info.agent_id != waiter.agent
					? std::format("  related {} - chat '{}' is editing it", related.generic_display_string(), touching->name)
					: "  related " + related.generic_display_string());
			}
			if (!is_mine) {
				theirs.push_back(std::format("  ^ chat '{}' owns this", owner->name.empty() ? std::string("unnamed") : owner->name));
			}
		}

		outcome.owned = owned;
		if (mine.empty() && theirs.empty()) {
			outcome.lines.emplace_back("the build failed without a parseable diagnostic; open the build stream in the editor terminal");
		}
		if (!mine.empty()) {
			outcome.lines.emplace_back("your errors:");
			outcome.lines.insert(outcome.lines.end(), mine.begin(), mine.end());
		}
		if (!theirs.empty()) {
			outcome.lines.emplace_back(mine.empty()
				? "none of these errors are in files you edited - your own work is not implicated, and fixing them is not your job:"
				: "errors owned by other chats - do not act on these:");
			outcome.lines.insert(outcome.lines.end(), theirs.begin(), theirs.end());
		}
		build_inbox::publish(outcome);
	}
	d.inbox_active.clear();
}

auto gse::ide::agent::dispatch_blame(data& d, const std::uint32_t session_id) -> void {
	const auto found = std::ranges::find(d.sessions, session_id, &session::id);
	if (found == d.sessions.end() || found->blame.empty()) {
		return;
	}

	if (!found->running && !launch_session(*found)) {
		append_row(*found, {
			.kind = row_kind::failure,
			.text = "failed to launch 'claude' - is it on PATH?",
		});
		return;
	}

	d.active = session_id;
	d.overview_active = false;
	send_to_session(*found, blame_prompt("The build failed on files this session edited:", found->blame), {});
	found->blame.clear();
	found->blame_build = {};
}

auto gse::ide::agent::fix_unclaimed(data& d) -> void {
	if (d.unclaimed.empty()) {
		return;
	}

	const blamed_error& first = d.unclaimed.front();
	session& started = create_session(d, config::worktree_for(first.file).project_root);
	started.name = std::format("fix {}", first.file.filename().generic_display_string());

	if (!launch_session(started)) {
		append_row(started, {
			.kind = row_kind::failure,
			.text = "failed to launch 'claude' - is it on PATH?",
		});
		return;
	}

	send_to_session(started, blame_prompt("The build failed and no chat owns these edits:", d.unclaimed), {});
	d.unclaimed.clear();
	d.unclaimed_build = {};
}

auto gse::ide::agent::blame_label(const session& s) -> std::string {
	if (s.blame.empty()) {
		return "-";
	}

	const blamed_error& first = s.blame.front();
	const std::string head = std::format("{}:{}", first.file.filename().generic_display_string(), first.line);
	return s.blame.size() > 1
		? std::format("{} +{}", head, s.blame.size() - 1)
		: head;
}

auto gse::ide::agent::blame_prompt(const std::string_view lead, const std::span<const blamed_error> errors) -> std::string {
	std::string prompt(lead);
	prompt += '\n';
	for (const blamed_error& error : errors) {
		prompt += std::format("{}:{}: error: {}\n", error.file.generic_display_string(), error.line, error.message);
		for (const std::string& note : error.notes) {
			prompt += "  " + note + "\n";
		}
	}
	prompt += "Fix them.";
	return prompt;
}

auto gse::ide::agent::arm_retry(session& s) -> void {
	if (s.retry.prompt.empty()) {
		return;
	}

	s.retry.waiting = true;
	++s.retry.attempts;

	const std::int64_t remaining = s.limited_until - unix_now();
	append_row(s, {
		.kind = row_kind::note,
		.text = remaining > 0
			? std::format("usage limit reached - holding this turn, resending in {}m", std::max<std::int64_t>(remaining / 60, 1))
			: "holding this turn - it will be resent once the connection is back",
	});
}

auto gse::ide::agent::link_delay(const data& d) -> time {
	const time first = seconds(5.f);
	const time longest = seconds(20.f);
	return std::min<time>(first * static_cast<float>(1u << std::min(d.link_misses, 5u)), longest);
}

auto gse::ide::agent::resume_waiting(data& d) -> void {
	for (session& s : d.sessions) {
		if (!s.retry.waiting || s.limited_until > unix_now()) {
			continue;
		}
		if (!s.running && !launch_session(s)) {
			continue;
		}

		const std::string prompt = s.retry.prompt;
		std::vector<attachment> images;
		images.reserve(s.retry.images.size());
		for (const std::filesystem::path& file : s.retry.images) {
			images.push_back({
				.path = file,
			});
		}

		append_row(s, {
			.kind = row_kind::note,
			.text = std::format("retrying the interrupted turn (attempt {})", s.retry.attempts + 1),
		});
		s.retry.waiting = false;
		send_to_session(s, prompt, images);
	}
}

auto gse::ide::agent::service_link(data& d) -> void {
	constexpr std::uint32_t blind_retry_after = 6;

	const bool waiting = std::ranges::any_of(d.sessions, [](const session& s) {
		return s.retry.waiting;
	});

	if (!waiting) {
		net::cancel(d.link);
		d.link_clock.reset();
		d.link_misses = 0;
		return;
	}

	if (!d.link_clock) {
		d.link_clock.emplace();
	}

	if (net::active(d.link)) {
		const net::reach state = net::poll(d.link);
		if (state == net::reach::pending) {
			return;
		}

		if (state == net::reach::up) {
			d.link_misses = 0;
			resume_waiting(d);
		}
		else if (++d.link_misses >= blind_retry_after) {
			resume_waiting(d);
		}

		d.link_clock.emplace();
		return;
	}

	if (d.link_clock->elapsed() >= link_delay(d)) {
		net::begin(d.link);
	}
}

auto gse::ide::agent::link_label(const data& d, const session& s) -> std::string {
	if (!s.retry.waiting) {
		return {};
	}

	if (const std::int64_t remaining = s.limited_until - unix_now(); remaining > 0) {
		return remaining >= 120
			? std::format("usage limit reached - the held turn resumes in {}m", remaining / 60)
			: std::format("usage limit reached - the held turn resumes in {}s", remaining);
	}

	const std::string_view lead = d.link_misses > 0 ? "offline - " : "";
	if (net::active(d.link)) {
		return std::format("{}checking the connection (attempt {})", lead, s.retry.attempts);
	}

	const time waited = d.link_clock ? d.link_clock->elapsed() : time{};
	const time remaining = std::max<time>(link_delay(d) - waited, time{});
	return std::format("{}resending the held turn in {:.0f}s (attempt {})", lead, remaining.as<seconds>(), s.retry.attempts);
}